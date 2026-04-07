#include <avr/iotn202.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "types.h"
#include "tables.h"
#include "main.h"

volatile uint64_t raw;
volatile uint64_t capture;
volatile union inputs_t inputs;
volatile uint64_t flip;
volatile uint64_t mask;

volatile uint8_t  behavior;
volatile uint8_t  nInupts;

volatile uint8_t  shift;
volatile uint8_t  latch;

volatile uint8_t  task;
volatile uint8_t  nTask;

volatile uint8_t  OUT;

int main(void){
    init();
    sei();

    while (1){
        // main loop
    }
}


inline void init(void){
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTA.DIRCLR = (1 << 3);
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;
}

inline void handle_interrupt(void){
    HANDLE(__OUT) {
        OUT = PORTA.IN & __OUT;
        console_write();
    }

    HANDLE(__CLK) {
        console_read();
    }
}

inline void console_read(void){
    if (latch){
        switch (task){
            case REPORT:
                inputs.hunk ^= flip;

                if (behavior & L_TO_C) {
                    if (!*(volatile uint16_t*)GetCStick()) *GetCStick() = *GetLStick();
                } else if (behavior & C_TO_L){
                    if (!*(volatile uint16_t*)GetLStick()) *GetLStick() = *GetCStick();
                }

                if (behavior & L_PREC) {
                    CalculateStick(GetLStick());
                }
                if (behavior & C_PREC){
                    if (behavior & L_TO_C) *GetCStick() = *GetLStick();
                    else                    CalculateStick(GetCStick());
                }
                
                if (behavior & D_TO_L){
                    PadToStick(GetLStick());
                }

                if (behavior & D_TO_C){
                    PadToStick(GetCStick());
                }
                
                break;

            case BEHAVE:
                behavior |= (OUT << nTask);
                break;
            
            case INMASK:
                mask |= (OUT << nTask);
                nInupts += OUT;
                break;

            case INVERT:
                flip |= (OUT << nTask);
                break;

            case RUMBLE:
                break;

            case LSETUP:
                break;
        }

        if (--nTask) return;
        latch = 0;
        task  = LEGACY;
    } else if (OUT) {
        ++task;
        task = task % (LSETUP + 1);
    } else /* legacy */ {
        if (shift & (1ull << (64 - nTask))) PORTA.OUTSET = __D0;
        else                                PORTA.OUTCLR = __D0;
        if (--nTask == 0) nTask = 64;
    }

}

inline void console_write(void){
    if (!OUT){
        if (task == LEGACY) shift = 0; 
        else {
            latch = 1;
            switch (task) {
                case REPORT:
                    nTask = nInupts;
                    break;

                case BEHAVE:
                    behavior = 0;
                    nTask = 8;
                    break;

                case INVERT:
                    flip = 0;
                    goto wide;

                case INMASK:
                    mask    = 0;
                    nInupts = 0;
                    goto wide;

                default:
                    nTask = 0;  // TODO: work
                    break;

                wide:
                    nTask = 64;
                    break;
            }
        }
    }
}