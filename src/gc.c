#ifndef SIMULATION
#include <avr/io.h>
#include <avr/interrupt.h>
#else
#include "../sim/sim_hal.h"
#endif

#include "gc.h"

// raw command bytes — 12 bytes total vs 104 for pre-baked
const gc_cmd_entry gc_commands[GC_CMD_COUNT] = {
    [GC_CMD_PROBE]      = { .data = {0x00, 0x00, 0x00}, .bits = 8  },
    [GC_CMD_POLL]        = { .data = {0x40, 0x03, 0x00}, .bits = 24 },
    [GC_CMD_ORIGIN]      = { .data = {0x41, 0x00, 0x00}, .bits = 8  },
    [GC_CMD_RUMBLE_ON]   = { .data = {0x40, 0x03, 0x01}, .bits = 24 },
};

volatile uint8_t gc_tx_bit;
volatile uint8_t gc_tx_byte;
volatile uint8_t gc_tx_total;
volatile const gc_cmd_entry *gc_active_cmd;
volatile uint8_t gc_tx_done = 1;

volatile uint8_t gc_rx_buffer[GC_RESPONSE_LEN];
volatile uint8_t gc_rx_byte;
volatile uint8_t gc_rx_bit;
volatile uint8_t gc_rx_count;
volatile uint8_t gc_rx_done = 0;

void gc_init_tca(void) {
    TCA0.SINGLE.CTRLA = 0;
    TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_RESET_gc;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc;
    TCA0.SINGLE.EVCTRL = 0;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc
                       | TCA_SINGLE_CMP0EN_bm;
    TCA0.SINGLE.PER = 79;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    PORTA.OUTCLR = (1 << GC_PIN);
}

void gc_init_tcb(void) {
    TCB0.CTRLB = TCB_CNTMODE_PW_gc;
    TCB0.EVCTRL = TCB_CAPTEI_bm | TCB_EDGE_bm | TCB_FILTER_bm;
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA = TCB_CLKSEL_CLKDIV1_gc;
}

void gc_init_events(void) {
    EVSYS.ASYNCCH0 = EVSYS_ASYNCCH0_PORTA_PIN0_gc;
    EVSYS.ASYNCUSER0 = EVSYS_ASYNCUSER0_ASYNCCH0_gc;
}

// convert current bit to compare value
static uint8_t gc_next_cmp(void) {
    uint8_t byte_idx = gc_tx_bit >> 3;
    uint8_t bit_pos  = 7 - (gc_tx_bit & 7);  // MSB first
    return (gc_active_cmd->data[byte_idx] & (1 << bit_pos)) ? GC_BIT_1 : GC_BIT_0;
}

void gc_send(enum gc_command cmd) {
    gc_active_cmd = &gc_commands[cmd];
    gc_tx_bit = 0;
    gc_tx_total = gc_active_cmd->bits;
    gc_tx_done = 0;
    gc_rx_done = 0;

    // load first two compare values
    uint8_t cmp0 = gc_next_cmp(); gc_tx_bit++;
    uint8_t cmp1 = (gc_tx_bit < gc_tx_total) ? gc_next_cmp() : GC_BIT_1;
    if (gc_tx_bit < gc_tx_total) gc_tx_bit++;

    TCA0.SINGLE.CMP0 = cmp0;
    TCA0.SINGLE.CMP0BUF = cmp1;
    TCA0.SINGLE.CNT = 0;
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

    PORTA.DIRSET = (1 << GC_PIN);
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

void gc_start_receive(void) {
    PORTA.DIRCLR = (1 << GC_PIN);
    gc_rx_byte = 0;
    gc_rx_bit = 0;
    gc_rx_count = 0;
    TCB0.INTFLAGS = TCB_CAPT_bm;
    TCB0.CTRLA |= TCB_ENABLE_bm;
}

#ifndef SIMULATION
ISR(TCA0_OVF_vect) {
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

    if (gc_tx_bit < gc_tx_total) {
        TCA0.SINGLE.CMP0BUF = gc_next_cmp();
        gc_tx_bit++;
    } else if (gc_tx_bit == gc_tx_total) {
        // stop bit
        TCA0.SINGLE.CMP0BUF = GC_BIT_1;
        gc_tx_bit++;
    } else {
        // done
        TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;
        TCA0.SINGLE.CTRLB &= ~TCA_SINGLE_CMP0EN_bm;
        gc_tx_done = 1;
        gc_start_receive();
    }
}

ISR(TCB0_INT_vect) {
    uint16_t width = TCB0.CCMP;
    TCB0.INTFLAGS = TCB_CAPT_bm;

    gc_rx_byte <<= 1;
    if (width < 40) gc_rx_byte |= 1;

    if (++gc_rx_bit == 8) {
        gc_rx_buffer[gc_rx_count++] = gc_rx_byte;
        gc_rx_byte = 0;
        gc_rx_bit = 0;

        if (gc_rx_count >= GC_RESPONSE_LEN) {
            TCB0.CTRLA &= ~TCB_ENABLE_bm;
            gc_rx_done = 1;
        }
    }
}
#endif
