#ifndef SIMULATION
#include <avr/io.h>
#include <avr/interrupt.h>
#else
#include "../sim/sim_hal.h"
#endif

#include "types.h"
#include "gc.h"
#include "main.h"

input_t  buffers[2];
volatile wide_t   flip;
volatile wide_t   origin;   // GC analog-stick rest values from 0x41 origin response
input_t* targetBuffer;   // filled by main loop from GC poll
input_t* outputBuffer;   // shifted out by ISR — the bits not being worked on
volatile uint8_t  shift;

volatile uint8_t behavior;
volatile uint8_t rumble;       // 1 = next poll uses GC_CMD_RUMBLE_ON
static   uint8_t pollClock;
static   uint8_t lSetup;

// ISR-private state — accessed only inside __vector_3
static uint8_t  latch;
static uint8_t  task;
static uint8_t  nTask;
static uint8_t  nShift;
static uint8_t  cmd;       // CLKs counted while OUT is high — selects next task
static uint8_t  OUT;


void main_iter(void) {
    if (gc_rx_done) {
        gc_rx_done = 0;

        cli();
        const uint8_t status = gc_rx_buffer[0];

        if (gc_active_cmd->data[0] == 0x41) {
            // ORIGIN response — stash analog rest values
            for (uint8_t i = 0; i < GC_RESPONSE_LEN; i++)
                origin.arr[i] = gc_rx_buffer[i];
        } else if (status & (1 << 7)) {
            // error on last transfer — skip publish, just re-poll
        } else if (status & (1 << 5)) {
            // controller wants origin (reset/replug) — fetch before polling again
            sei();
            gc_send(GC_CMD_ORIGIN);
            return;
        } else {
            for (uint8_t i = 0; i < GC_RESPONSE_LEN; i++)
                ((wide_t*)targetBuffer)->arr[i] = gc_rx_buffer[i];
            sei();
            InputPreprocess();
            cli();
            input_t* t   = outputBuffer;
            outputBuffer = targetBuffer;
            targetBuffer = t;
        }
        sei();
        gc_send(rumble ? GC_CMD_RUMBLE_ON : GC_CMD_POLL);
    }

    if (gc_tx_done && !gc_rx_done) {
        gc_send(GC_CMD_POLL);   // idle kick — one non-rumble poll on resume is fine
    }
}

#ifndef SIMULATION
int main(void){
    init();
    sei();
    while (1) main_iter();
}
#endif

#ifdef SIMULATION
// Test harness only — wipe all firmware-side state so tests run in isolation.
void firmware_reset(void) {
    for (uint8_t i = 0; i < 2; i++) {
        for (uint8_t j = 0; j < 8; j++) ((wide_t*)&buffers[i])->arr[j] = 0;
    }
    for (uint8_t j = 0; j < 8; j++) { flip.arr[j] = 0; origin.arr[j] = 0; }
    shift     = 0;
    behavior  = 0;
    rumble    = 0;
    pollClock = 0;
    lSetup    = 0;
    latch     = 0;
    task      = LEGACY;
    nTask     = 0;
    nShift    = 0;
    cmd       = 0;
    OUT       = 0;
    init();
    // sim shortcut: bypass the ORIGIN handshake init() just kicked off, so OP_POLL
    // tests start in the steady-state where main_iter expects a POLL response.
    // Origin stays zero (no correction) unless tests set it via OP_ORIGIN.
    gc_send(GC_CMD_POLL);
}
#endif


void init(void){
    // NES pins
    PORTA.DIRSET = (1 << 2);                                       // D0 output
    PORTA.OUTSET = (1 << 2);                                       // D0 idle high
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;     // OUT — any edge
    PORTA.DIRCLR = (1 << 3);
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;        // CLK — rising

    // GC peripherals
    gc_init_tca();
    gc_init_tcb();
    gc_init_events();

    // NES interrupts are highest priority
    CPUINT.LVL1VEC = PORTA_PORT_vect_num;

    // request stick origin first; main_iter routes the response into `origin`
    // and then drives normal POLL cadence.
    gc_send(GC_CMD_ORIGIN);
    targetBuffer = &buffers[0];
    outputBuffer = &buffers[1];
}


void handle_interrupt(void){
    HANDLE(__OUT) {
        OUT = PORTA.IN & __OUT;
        // Only act on idle-falling: rising / mid-task edges are no-ops here.
        if (latch | OUT) return;
        task = cmd & 0b111;
        cmd  = 0;     // a stray falling-without-rising must not re-enter this task
        console_write();
    }

    HANDLE(__CLK) {
        if (latch) {
            // console sending bits TO us — OUT here is the data bit value.
            // (REPORT clears latch on entry, so it falls through to shift-out below.)
            console_read();
            return;
        }

        // command-count phase: clocks while OUT is high (and we're idle) select task
        if (OUT) { cmd++; return; }

        // driving D0 — legacy or REPORT — read from the stable (not-being-worked-on) buffer
        if (shift < nShift) {
            if (((wide_t*)outputBuffer)->arr[shift >> 3] & (1 << (shift & 0b111)))  PORTA.OUTSET = __D0;
            else                                                                    PORTA.OUTCLR = __D0;
            shift++;
        } else {
            PORTA.OUTCLR = __D0;
        }
    }
}

void console_read(void){
    switch (task){
        case BEHAVE:
            behavior = (behavior << 1) | (OUT ? 1 : 0);
            break;

        case INVERT: {
            const uint8_t idx = nTask - 1;
            if (OUT) flip.arr[idx >> 3] |=  (1 << (idx & 7));
            else     flip.arr[idx >> 3] &= ~(1 << (idx & 7));
            break;
        }

        case RUMBLE:
            rumble = OUT ? 1 : 0;
            break;

        case LSETUP:
            lSetup = (lSetup << 1) | (OUT ? 1 : 0);
            break;
    }

    if (--nTask) return;
    latch = 0;
    // task selection is owned by the cmd channel — no auto-advance here.
    // nShift not re-locked from nInupts: see InputPreprocess where mask zeros bytes
    // past nInupts, so over-shifting just streams zeros regardless.
}

static inline void LegacyButtonPreProcess(wide_t* raw) {
    const uint8_t a = raw->arr[0];
    const uint8_t b = raw->arr[1];
    targetBuffer->buttons[0] =
           (a & 0x03)              // IA→LA, IB→LB    (bits 0,1 stay)
        | ((a & 0x10) >> 1)        // IStart→LStart   (bit 4 → 3)
        | ((b & 0x10) >> 2)        // IZ→LSelect      (bit 4 → 2)
        | ((b & 0x08) << 1)        // IUp→LUp         (bit 3 → 4)
        | ((b & 0x04) << 3)        // IDown→LDown     (bit 2 → 5)
        | ((b & 0x03) << 6);       // ILeft→LLeft, IRight→LRight (bits 0,1 → 6,7)
}
static inline void ButtonPreprocess(wide_t* raw) {
    const uint8_t a = raw->arr[0];
    const uint8_t b = raw->arr[1];
    targetBuffer->buttons[0] =
          ((a & 0x02) >> 1)        // IB→B            (bit 1 → 0)
        | ((a & 0x08) >> 2)        // IY→Y            (bit 3 → 1)
        | ((b & 0x10) >> 2)        // IZ→Z            (bit 4 → 2)
        | ((a & 0x10) >> 1)        // IStart→Start    (bit 4 → 3)
        | ((b & 0x08) << 1)        // IUp→Up          (bit 3 → 4)
        | ((b & 0x04) << 3)        // IDown→Down      (bit 2 → 5)
        | ((b & 0x03) << 6);       // ILeft→Left, IRight→Right
    targetBuffer->buttons[1] =
          (a & 0x01)               // IA→A            (bit 0 stays)
        | ((a & 0x04) >> 1)        // IX→X            (bit 2 → 1)
        | ((b & 0x40) >> 4)        // IL→L            (bit 6 → 2)
        | ((b & 0x20) >> 2);       // IR→R            (bit 5 → 3)
}

static inline uint8_t angle_sign_to_pad(const int8_t angle) {
    if      (!angle)    return 0b00;
    else if (angle > 0) return 0b10;
    else                return 0b01;
}

inline static void LStickToPad(wide_t* raw) {
    raw->arr[0] |= angle_sign_to_pad((int8_t)targetBuffer->lStick.x) << 6;
    raw->arr[0] |= angle_sign_to_pad((int8_t)targetBuffer->lStick.y) << 4;
}

static inline void FlipBuffer(wide_t* raw) {
    for (uint8_t i = 0; i < 8; i++) raw->arr[i] ^= flip.arr[i];
}

static inline void LegacyProcessTurboButtons(uint8_t gc0) {
    if ((lSetup & X_IS_TURBO_A) && (gc0 & IX)) {
        targetBuffer->buttons[0] |= (pollClock & 0x07) ? 0 : LA;
    }

    if ((lSetup & Y_IS_TURBO_B) && (gc0 & IY)) {
        targetBuffer->buttons[0] |= (pollClock & 0x07) ? 0 : LB;
    }
}

void InputPreprocess(){
    wide_t* raw = (wide_t*)targetBuffer;
    // origin correction: shift analog bytes (sticks + triggers) into signed
    // deviations from rest. Wraps in uint8_t which is exactly 2's-complement.
    for (uint8_t i = 2; i < 8; i++) raw->arr[i] -= origin.arr[i];
    if (task == LEGACY) {
        const uint8_t gc0 = raw->arr[0];   // snapshot before legacy encode stomps it
        LegacyButtonPreProcess(raw);
        LegacyProcessTurboButtons(gc0);
        LStickToPad(raw);
    } else {
        ButtonPreprocess(raw);

        if (behavior & L_TO_C) {
            if (!*(uint16_t*)&targetBuffer->cStick) targetBuffer->cStick = targetBuffer->lStick;
        } else if (behavior & C_TO_L){
            if (!*(uint16_t*)&targetBuffer->lStick) targetBuffer->lStick = targetBuffer->cStick;
        }

        if (behavior & D_TO_L){
            PadToStick(&targetBuffer->lStick);
        }

        if (behavior & D_TO_C){
            PadToStick(&targetBuffer->cStick);
        }

        // typical-usage region zeroers — replace what arbitrary INMASK used to do
        if (behavior & UnifiedTrigger) {
            if (raw->arr[7] > raw->arr[6]) raw->arr[6] = raw->arr[7];
            raw->arr[7] = 0;
        }
        if (behavior & NoTriggers) { raw->arr[6] = 0; raw->arr[7] = 0; }
        if (behavior & NoCStick)   { raw->arr[4] = 0; raw->arr[5] = 0; }
        if (behavior & NoLStick)   { raw->arr[2] = 0; raw->arr[3] = 0; }

        // wire-level inversion (INVERT) applied last
        FlipBuffer(raw);
    }
}

void console_write(void){
    if (!OUT){
        if (task == LEGACY) {
            shift  = 0;
            nShift = 8;
            pollClock++;       // one tick per controller read
        } else {
            latch = 1;
            switch (task) {
                case REPORT:
                    shift  = 0;
                    nShift = 64;   // full input_t — refine when REPORT subset selection lands
                    latch  = 0;    // REPORT is a read phase
                    break;

                case BEHAVE:
                    behavior = 0;
                    nTask    = 8;
                    break;

                case INVERT:
                    nTask = 64;
                    break;

                case RUMBLE:
                    nTask = 1;
                    break;

                case LSETUP:
                    nTask = 2;
                    break;

                default:
                    nTask = 0;
            }
        }
    }
}

void PadToStick(vec2* pStick) {
    const uint8_t temp = targetBuffer->buttons[0] & (
        Up | Down | Left | Right
    );
    
    pStick->x  = temp & 0x40 ? 0x80 : 0;
    pStick->x += temp & 0x80 ? 0x7f : 0;
    pStick->y  = temp & 0x10 ? 0x80 : 0;
    pStick->y += temp & 0x20 ? 0x7f : 0;
}