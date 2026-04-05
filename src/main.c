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

volatile uint8_t  __PD2;

int main(void){
    init();
    sei();

    while (1){
        // main loop
    }
}


inline void init(void){
    DDRC  |=  (1 << PC0);
    PORTC |=  (1 << PC0);

    DDRD  &= ~(1 << PD2);
    PORTD |=  (1 << PD2);

    DDRD  &= ~(1 << PD3);
    PORTD |=  (1 << PD3);

    EICRA &= ~(1 << ISC01);
    EICRA |=  (1 << ISC00);

    EICRA |=  (1 << ISC11) | (1 << ISC10);
    EIMSK |=  (1 << INT0)  | (1 << INT1);
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
                    if (behavior & D_TO_L){
                        if (!GetLStick()->y) {
                            switch (GetDPad()){
                                case 0b11000000:    // just opposing LR
                                case 0b00110000:    // just opposing UD
                                case 0b11110000:    // all opposing input
                                case 0b00000000:    // no d pad input    
                                    break;

                                case 0b00010000:    // up only
                                    GetLStick()->x = 0x00;
                                    GetCStick()->y = 0xff;
                                    break;
                                
                                case 0b00100000:    // down only
                                    GetLStick()->x = 0x80;
                                    GetCStick()->y = 0xff;
                                    break;
                                
                                case 0b10000000:    // right only
                                    GetLStick()->x = 0x40;
                                    GetCStick()->y = 0xff;
                                    break;    
                                
                                case 0b01000000:    // left only
                                    GetLStick()->x = 0xc0;
                                    GetCStick()->y = 0xff;
                                    break;    
                                
                                case 0b10010000:    // up and right
                                    GetLStick()->x = 0x20;
                                    GetCStick()->y = 0xb5;
                                    break;

                                case 0b10100000:    // down and right
                                    GetLStick()->x = 0x60;
                                    GetCStick()->y = 0xb5;
                                    break;

                                case 0b01010000:    // up and left
                                    GetLStick()->x = 0xe0;
                                    GetCStick()->y = 0xb5;
                                    break;

                                case 0b01100000:    // down and left
                                    GetLStick()->x = 0xa0;
                                    GetCStick()->y = 0xb5;
                                    break;
                            }
                        }
                    }
                }
                if (behavior & C_PREC){
                    if (behavior & L_TO_C) *GetCStick() = *GetLStick();
                    else                    CalculateStick(GetCStick());
                }

                // TODO: do D_TO_L and D_TO_C
                break;

            case BEHAVE:
                behavior |= (__PD2 << nTask);
                break;
            
            case INMASK:
                mask |= (__PD2 << nTask);
                nInupts += __PD2;
                break;

            case INVERT:
                flip |= (__PD2 << nTask);
                break;

            case RUMBLE:
                break;

            case LSETUP:
                break;
        }

        if (--nTask) return;
        latch = 0;
        task  = LEGACY;
    } else if (__PD2) {
        ++task;
        task = task % (LSETUP + 1);
    } else /* legacy */ {
        if (shift & (1ull << (64 - nTask))) PORTC |=  (1 << PC0);
        else                                PORTC &= ~(1 << PC0);
        if (--nTask == 0) nTask = 64;
    }

}

inline void console_write(void){
    __PD2 = PIND & (1 << PD2);
    if (!__PD2){
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