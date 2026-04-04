#include <avr/io.h>
#include <avr/interrupt.h>

#include "main.h"

int main(void){
    init();
    sei();

    while (1){
        // main loop
    }
}


inline void init(void){
    DDRC  |= (1 << PC0);
    PORTC |= (1 << PC0);

    DDRD  &= ~(1 << PD2);
    PORTD |=  (1 << PD2);

    DDRD  &= ~(1 << PD3);
    PORTD |=  (1 << PD3);

    EICRA &= ~(1 << ISC01);
    EICRA |=  (1 << ISC00);

    EICRA |= (1 << ISC11) | (1 << ISC10);
    EIMSK |= (1 << INT0)  | (1 << INT1);
}

inline void console_read(void){
    if (latch){
        switch (task){
            case REPORT:
                break;
            
            case INMASK:
                mask |= (__PD2 << nTask);
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
    } else if (__PD2){
        task = ++task % (LSETUP + 1);
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
            switch (task){
                case INVERT:
                    flip = 0;
                    goto wide;

                case INMASK:
                    mask = 0;
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