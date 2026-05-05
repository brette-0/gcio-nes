// gcio sim — bytecode-driven test harness.
//
// Loads one or more compiled test files (see scripts/gen_test.py),
// drives the firmware's PORTA pins + ISRs, and logs results in bulk
// at the end of each test (no per-check noise).

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
    #include "../src/main.h"
    #include "../src/gc.h"
    extern input_t* targetBuffer;
    extern input_t* outputBuffer;
    extern volatile wide_t flip;
    extern volatile wide_t origin;
    extern volatile uint8_t behavior;
    extern volatile uint8_t rumble;
    void firmware_reset(void);
}

// Bytecode opcodes — must match scripts/gen_test.py.
enum Op : uint8_t {
    OP_GC       = 0x01,   // 8 bytes: gc_rx_buffer
    OP_POLL     = 0x02,   // run main_iter to consume the GC poll
    OP_OUT      = 0x03,   // 1 byte: 0|1 — drive OUT pin and fire OUT ISR
    OP_CLK      = 0x04,   // fire CLK rising-edge ISR
    OP_EXP_D0   = 0x05,   // 1 byte: 0|1 — assert D0 == value
    OP_LABEL    = 0x06,   // 1 byte len, len bytes utf-8
    OP_EXP_BUF  = 0x07,   // 1 byte which (0=target,1=output), 8 bytes expected
    OP_ORIGIN   = 0x08,   // 8 bytes: directly set the stick-origin storage
    OP_EXP_RUM  = 0x09,   // 1 byte: assert rumble flag matches
    OP_END      = 0xFF,
};

struct Check {
    std::string label;
    std::string kind;
    bool        ok;
    std::string detail;
};

static std::vector<Check> g_checks;
static std::string         g_label = "<unlabeled>";

static std::string hex8(const uint8_t* p) {
    std::ostringstream o;
    o << std::hex << std::setfill('0');
    for (int i = 0; i < 8; i++) {
        if (i) o << ' ';
        o << std::setw(2) << (int)p[i];
    }
    return o.str();
}

// AVR's OUTSET/OUTCLR/OUTTGL are aliased registers that modify OUT in
// hardware. The plain struct in sim_hal.h doesn't mirror them, so we apply
// pending side-effects manually after any firmware code path.
static void sim_hal_apply(void) {
    PORTA.OUT |=  PORTA.OUTSET;  PORTA.OUTSET = 0;
    PORTA.OUT &= ~PORTA.OUTCLR;  PORTA.OUTCLR = 0;
    PORTA.OUT ^=  PORTA.OUTTGL;  PORTA.OUTTGL = 0;
    PORTA.DIR |=  PORTA.DIRSET;  PORTA.DIRSET = 0;
    PORTA.DIR &= ~PORTA.DIRCLR;  PORTA.DIRCLR = 0;
    PORTA.DIR ^=  PORTA.DIRTGL;  PORTA.DIRTGL = 0;
}

static uint8_t s_out_pin = 0;

static void fire_out(uint8_t v) {
    v &= 1;
    if (v == s_out_pin) return;   // no edge → no ISR (real hardware behavior)
    s_out_pin = v;
    if (v) PORTA.IN |=  __OUT;
    else   PORTA.IN &= ~__OUT;
    PORTA.INTFLAGS = __OUT;
    handle_interrupt();
    sim_hal_apply();
}

static void fire_clk() {
    PORTA.INTFLAGS = __CLK;
    handle_interrupt();
    sim_hal_apply();
}

static void reset_firmware_state() {
    // Fresh hardware regs.
    std::memset((void*)&PORTA,  0, sizeof(PORTA));
    std::memset((void*)&CPUINT, 0, sizeof(CPUINT));
    std::memset((void*)&TCA0,   0, sizeof(TCA0));
    std::memset((void*)&TCB0,   0, sizeof(TCB0));
    std::memset((void*)&EVSYS,  0, sizeof(EVSYS));

    // Fresh gc.c state.
    for (int i = 0; i < GC_RESPONSE_LEN; i++) gc_rx_buffer[i] = 0;
    gc_rx_byte = 0; gc_rx_bit = 0; gc_rx_count = 0; gc_rx_done = 0;
    gc_tx_bit = 0; gc_tx_byte = 0; gc_tx_total = 0; gc_active_cmd = nullptr;
    gc_tx_done = 1;   // idle, ready to send

    // Fresh main.c state — also re-runs init() internally.
    firmware_reset();
    sim_hal_apply();
    s_out_pin = 0;
}

static bool run(const std::vector<uint8_t>& bc) {
    reset_firmware_state();
    size_t i = 0;
    while (i < bc.size()) {
        uint8_t op = bc[i++];
        switch (op) {
            case OP_GC: {
                if (i + 8 > bc.size()) { std::cerr << "OP_GC truncated\n"; return false; }
                for (int j = 0; j < 8; j++) gc_rx_buffer[j] = bc[i++];
                break;
            }
            case OP_POLL: {
                gc_rx_done = 1;
                gc_tx_done = 1;
                main_iter();
                sim_hal_apply();
                if (std::getenv("GCIO_SIM_TRACE")) {
                    uint8_t f[8];
                    for (int j = 0; j < 8; j++) f[j] = flip.arr[j];
                    std::cerr << "  POLL: target=[" << hex8(reinterpret_cast<const uint8_t*>(targetBuffer))
                              << "] output=[" << hex8(reinterpret_cast<const uint8_t*>(outputBuffer)) << "]\n"
                              << "        flip=[" << hex8(f) << "] behavior=" << (int)behavior << "\n";
                }
                break;
            }
            case OP_OUT: {
                if (i >= bc.size()) { std::cerr << "OP_OUT truncated\n"; return false; }
                fire_out(bc[i++]);
                break;
            }
            case OP_CLK: {
                fire_clk();
                break;
            }
            case OP_EXP_D0: {
                if (i >= bc.size()) { std::cerr << "OP_EXP_D0 truncated\n"; return false; }
                uint8_t want = bc[i++];
                uint8_t got  = (PORTA.OUT & __D0) ? 1 : 0;
                std::ostringstream d;
                d << "want=" << (int)want << " got=" << (int)got;
                g_checks.push_back({g_label, "D0", got == want, d.str()});
                break;
            }
            case OP_LABEL: {
                if (i >= bc.size()) { std::cerr << "OP_LABEL truncated\n"; return false; }
                uint8_t len = bc[i++];
                if (i + len > bc.size()) { std::cerr << "OP_LABEL str truncated\n"; return false; }
                g_label.assign(reinterpret_cast<const char*>(&bc[i]), len);
                i += len;
                break;
            }
            case OP_ORIGIN: {
                if (i + 8 > bc.size()) { std::cerr << "OP_ORIGIN truncated\n"; return false; }
                for (int j = 0; j < 8; j++) origin.arr[j] = bc[i++];
                break;
            }
            case OP_EXP_RUM: {
                if (i >= bc.size()) { std::cerr << "OP_EXP_RUM truncated\n"; return false; }
                uint8_t want = bc[i++];
                uint8_t got  = rumble ? 1 : 0;
                std::ostringstream d;
                d << "want=" << (int)want << " got=" << (int)got;
                g_checks.push_back({g_label, "RUM", got == want, d.str()});
                break;
            }
            case OP_EXP_BUF: {
                if (i + 9 > bc.size()) { std::cerr << "OP_EXP_BUF truncated\n"; return false; }
                uint8_t which = bc[i++];
                uint8_t want[8];
                for (int j = 0; j < 8; j++) want[j] = bc[i++];
                const uint8_t* got = reinterpret_cast<const uint8_t*>(
                    which == 0 ? targetBuffer : outputBuffer);
                bool ok = std::memcmp(got, want, 8) == 0;
                std::ostringstream d;
                d << (which == 0 ? "target" : "output")
                  << " want=[" << hex8(want) << "] got=[" << hex8(got) << "]";
                g_checks.push_back({g_label, "BUF", ok, d.str()});
                break;
            }
            case OP_END:
                return true;
            default:
                std::cerr << "unknown opcode 0x" << std::hex << (int)op
                          << " at offset " << std::dec << (i - 1) << "\n";
                return false;
        }
    }
    return true;
}

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

int main(int argc, char** argv) {
    std::cout << "gcio sim Brette Allen (2026)\n";
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <test.bc> [<test.bc> ...]\n";
        return 2;
    }

    int total_fail = 0;
    for (int a = 1; a < argc; a++) {
        g_checks.clear();
        g_label = argv[a];

        auto bc = slurp(argv[a]);
        if (bc.empty()) {
            std::cerr << "[" << argv[a] << "] empty / unreadable\n";
            total_fail++;
            continue;
        }

        bool ran = run(bc);

        // Bulk log: one summary line, then one line per failure.
        size_t pass = 0, fail = 0;
        for (auto& c : g_checks) (c.ok ? pass : fail)++;
        std::cout << "[" << argv[a] << "] "
                  << (ran ? "ran" : "ABORTED") << " — "
                  << pass << "/" << g_checks.size() << " checks passed";
        if (fail) std::cout << ", " << fail << " FAILED";
        std::cout << "\n";
        for (auto& c : g_checks) {
            if (!c.ok) {
                std::cout << "  FAIL  [" << c.label << "] " << c.kind << ": "
                          << c.detail << "\n";
            }
        }
        total_fail += fail + (ran ? 0 : 1);
    }
    return total_fail ? 1 : 0;
}
