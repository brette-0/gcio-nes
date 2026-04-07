#ifndef  MAIN_H
#define  MAIN_H
#ifndef SIMULATION
#include <avr/io.h>
#include <avr/interrupt.h>
#else
#include "../sim/sim_hal.h"
#endif
#include "types.h"
#include "tables.h"


extern volatile wide_t inputs;
extern volatile uint8_t behavior;

void init(void);
void handle_interrupt(void);
void console_read(void);
void console_write(void);

#define __OUT   (1 << 1)
#define __D0    (1 << 2)
#define __CLK   (1 << 3)

#define HANDLE(pin) \
    if ((PORTA.INTFLAGS & pin) && ((void)(PORTA.INTFLAGS = pin), 1))

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


#ifndef SIMULATION
ISR(PORTA_PORT_vect) { handle_interrupt(); }
#endif

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


#define L_STICK (vec2*)&inputs.arr[2]
#define C_STICK (vec2*)&inputs.arr[4]
#define D_PAD (inputs.arr[0] & 0x40)

void PadToStick(vec2* pStick);


#endif