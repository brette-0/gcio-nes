#ifndef SIMULATION
#include <avr/io.h>
#include <avr/interrupt.h>
#else
#include "../sim/sim_hal.h"
#endif

#include "types.h"
#include "gc.h"
#include "main.h"

volatile wide_t inputs;
volatile shift_register legacy_sr;
volatile shift_register report_sr;
volatile shift_register *active_sr = &legacy_sr;
volatile wide_t flip;
volatile wide_t mask;

volatile uint8_t  behavior;
volatile uint8_t  nInupts;

volatile uint8_t  latch;

volatile uint8_t  task;
volatile uint8_t  nTask;

volatile uint8_t  OUT;

#ifndef SIMULATION
int main(void){
    init();
    sei();

    while (1){
        // Job 1: GC response arrived — process and reload buffers
        if (gc_rx_done) {
            gc_rx_done = 0;

            // copy raw response into inputs
            for (uint8_t i = 0; i < GC_RESPONSE_LEN; i++)
                inputs.arr[i] = gc_rx_buffer[i];

            // populate legacy shift register (raw mapped buttons)
            cli();
            legacy_sr.content = inputs;
            RESET_SR(legacy_sr);
            sei();

            // preprocess for report mode
            input_preprocess();

            cli();
            report_sr.content = inputs;
            RESET_SR(report_sr);
            sei();

            // kick off next poll immediately
            gc_send(GC_CMD_POLL);
        }

        // Job 2: nothing pending and GC idle — start polling
        if (gc_tx_done && !gc_rx_done) {
            gc_send(GC_CMD_POLL);
        }
    }
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

    // start first GC poll
    gc_send(GC_CMD_POLL);
}


void handle_interrupt(void){
    HANDLE(__OUT) {
        OUT = PORTA.IN & __OUT;
        console_write();
    }

    HANDLE(__CLK) {
        if (latch && task != REPORT) {
            // console sending bits TO us
            console_read();
            return;
        }

        // driving D0 — legacy or REPORT
        if (READ_SR(*active_sr)) PORTA.OUTSET = __D0;
        else                     PORTA.OUTCLR = __D0;
        SHIFT(*active_sr);
    }
}

void console_read(void){
    switch (task){
        case BEHAVE:
            behavior |= (OUT << nTask);
            break;

        case INMASK:
            W_MASK_BIT(mask, (OUT << nTask));
            nInupts += OUT;
            break;

        case INVERT:
            W_MASK_BIT(flip, (OUT << nTask));
            break;

        case RUMBLE:
            break;

        case LSETUP:
            break;

        default:
            break;
    }

    if (--nTask) return;
    latch = 0;
    task  = LEGACY;
    active_sr = &legacy_sr;
}

void input_preprocess(void){
    // work on a local copy
    wide_t temp = inputs;

    W_FLIP(temp, flip)

    vec2 *lstick = (vec2*)&temp.arr[2];
    vec2 *cstick = (vec2*)&temp.arr[4];

    if (behavior & L_TO_C) {
        if (!*(uint16_t*)cstick) *cstick = *lstick;
    } else if (behavior & C_TO_L){
        if (!*(uint16_t*)lstick) *lstick = *cstick;
    }

    if (behavior & D_TO_L){
        PadToStick(lstick);
    }

    if (behavior & D_TO_C){
        PadToStick(cstick);
    }

    // write back processed data
    inputs = temp;
}

void console_write(void){
    if (!OUT){
        if (task == LEGACY) {
            active_sr = &legacy_sr;
            RESET_SR(*active_sr);
        } else {
            latch = 1;
            switch (task) {
                case REPORT:
                    active_sr = &report_sr;
                    RESET_SR(*active_sr);
                    nTask = nInupts;
                    break;

                case BEHAVE:
                    behavior = 0;
                    nTask = 8;
                    break;

                case INMASK:
                    nInupts = 0;
                    goto wide;

                case INVERT:
                wide:
                    nTask = 64;
                    break;

                default:
                    nTask = 0;
                    break;
            }
        }
    }
}

void PadToStick(vec2* pStick){
    uint8_t temp;

    temp = D_PAD;
    pStick->x  = temp & 0x40 ? 0x80 : 0;
    pStick->x += temp & 0x80 ? 0x7f : 0;
    pStick->y  = temp & 0x10 ? 0x80 : 0;
    pStick->y += temp & 0x20 ? 0x7f : 0;
}
