#include <avr/io.h>
#include <avr/interrupt.h>

#include "types.h"
#include "tables.h"
#include "main.h"
volatile wide_t raw;
volatile wide_t capture;
volatile wide_t inputs;
volatile wide_t flip;
volatile wide_t mask;

volatile uint8_t  behavior;
volatile uint8_t  nInupts;

volatile shift_register shift;
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


void init(void){
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
    PORTA.DIRCLR = (1 << 3);
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;
}

void handle_interrupt(void){
    HANDLE(__OUT) {
        OUT = PORTA.IN & __OUT;
        console_write();
    }

    HANDLE(__CLK) {
        console_read();
    }
}

void console_read(void){
    if (latch){
        switch (task){
            case REPORT:
                W_FLIP(inputs, flip)

                if (behavior & L_TO_C) {
                    if (!*(volatile uint16_t*)C_STICK) *C_STICK = *L_STICK;
                } else if (behavior & C_TO_L){
                    if (!*(volatile uint16_t*)L_STICK) *L_STICK = *C_STICK;
                }

                if (behavior & L_PREC) {
                    CalculateStick(L_STICK);
                }
                if (behavior & C_PREC){
                    if (behavior & L_TO_C) *L_STICK = *C_STICK;
                    else                    CalculateStick(C_STICK);
                }
                
                if (behavior & D_TO_L){
                    PadToStick(L_STICK);
                }

                if (behavior & D_TO_C){
                    PadToStick(C_STICK);
                }
                
                break;

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
        }

        if (--nTask) return;
        latch = 0;
        task  = LEGACY;
    } else if (OUT) {
        ++task;
        if (task > LSETUP) task = LEGACY;
    } else /* legacy */ {
        if (READ_SR(shift)) PORTA.OUTSET = __D0;
        else                PORTA.OUTCLR = __D0;
        if (--nTask == 0) nTask = 64;
    }

}

void console_write(){
    if (!OUT){
        if (task == LEGACY) RESET_SR(shift); 
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
                    goto wide;

                case INMASK:
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