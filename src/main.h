#ifndef  MAIN_H
#define  MAIN_H
#include <avr/iotn202.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "tables.h"

typedef union inputs_t {
    uint64_t hunk;
    uint8_t  arr[8];
};

extern volatile union inputs_t inputs;

void init(void);
void handle_interrupt(void);

#define __OUT   (1 << 1)
#define __D0    (1 << 2)
#define __CLK   (1 << 3)

#define HANDLE(pin) \
    if ((PORTA.INTFLAGS & pin) && (PORTA.INTFLAGS = pin), 1)

#define B       0
#define Y       1
#define Z       2
#define Start   3
#define Up      4
#define Down    5
#define Left    6
#define Right   7
#define A       8
#define X       9
#define L       10
#define R       11
#define C       12


ISR(PORTA_PORT_vect) { handle_interrupt(); }

enum ETasks {
    LEGACY,
    REPORT,
    BEHAVE,
    INMASK,
    INVERT,
    RUMBLE,
    DEZONE,
    LSETUP
};

enum EBehaviors {
    L_TO_C,
    C_TO_L,
    D_TO_L,
    D_TO_C,
    L_PREC,
    C_PREC,
};

inline vec2* GetLStick() {
    return inputs.arr[3];
}

inline vec2* GetCStick() {
    return inputs.arr[5];
}

inline volatile uint8_t GetTrigger(const volatile uint8_t i){
    switch (i){
        case L:
            return inputs.arr[2] >> 4;
        
        case C:
            return inputs.arr[2] & 15;
    }
}

inline volatile uint8_t GetButton(const volatile uint8_t i){
    return inputs.arr[i > 7] & (1 << (i - (i > 7 ? 8 : 0)));
}

inline volatile uint8_t GetDPad(){
    return inputs.arr[0] & 0xf0;
}

void PadToStick(vec2* pStick){
    static volatile uint8_t temp;
    if (pStick == GetLStick())
        temp = behavior & L_PREC;
    else
        temp = behavior & C_PREC;
    
    // crude solution, will need some change when deadzone is in use.
    if (!!temp && pStick->y)                return;
    else if ((*(volatile uint16_t*)pStick)) return;

    // dpad to vector
    
    pStick->x  = temp & 0x40 ? 0x80 : 0;
    pStick->x += temp & 0x80 ? 0x7f : 0;
    pStick->y  = temp & 0x10 ? 0x80 : 0;
    pStick->y += temp & 0x20 ? 0x7f : 0;

    temp &= 1;
    if (!temp) return;
    CalculateStick(pStick);    
}


#endif