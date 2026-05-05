#ifndef  MAIN_H
#define  MAIN_H
#ifndef SIMULATION
#include <avr/io.h>
#include <avr/interrupt.h>
#else
#include "../sim/sim_hal.h"
#endif
#include "gc.h"
#include "types.h"

void init(void);
void main_iter(void);
#ifdef SIMULATION
void firmware_reset(void);
#endif
void handle_interrupt(void);
void console_read(void);
void console_write(void);
void InputPreprocess();

#define __OUT   (1 << 1)
#define __D0    (1 << 2)
#define __CLK   (1 << 3)

#define HANDLE(pin) \
    if ((PORTA.INTFLAGS & pin) && ((void)(PORTA.INTFLAGS = pin), 1))

enum ELegacyButtons {
    LA      = 1 << 0,
    LB      = 1 << 1,
    LSelect = 1 << 2,
    LStart  = 1 << 3,
    LUp     = 1 << 4,
    LDown   = 1 << 5,
    LLeft   = 1 << 6,
    LRight  = 1 << 7,
};

enum EInButtons {
    IA      = 1 << 0,
    IB      = 1 << 1,
    IX      = 1 << 2,
    IY      = 1 << 3,
    IStart  = 1 << 4,
    /* origin has been sent to console 0 = yes */
    /* error (latched) */
    /* error (ignore on last transfer) */
    ILeft   = 1 << 0,
    IRight  = 1 << 1,
    IDown   = 1 << 2,
    IUp     = 1 << 3,
    IZ      = 1 << 4,
    IR      = 1 << 5,
    IL      = 1 << 6,
    /* use the controller origin 1 = yes, not confirmed */
};

enum EButtons {
    B       = 1 << 0,
    Y       = 1 << 1,
    Z       = 1 << 2,
    Start   = 1 << 3,
    Up      = 1 << 4,
    Down    = 1 << 5,
    Left    = 1 << 6,
    Right   = 1 << 7,
    A       = 1 << 0,
    X       = 1 << 1,
    L       = 1 << 2,
    R       = 1 << 3
};

#ifndef SIMULATION
ISR(PORTA_PORT_vect) { handle_interrupt(); }
#endif

enum ETasks {
    LEGACY,
    REPORT,
    BEHAVE,
    INVERT,
    RUMBLE,
    LSETUP,
};

enum EBehaviors {
    L_TO_C         = 1 << 0,
    C_TO_L         = 1 << 1,
    D_TO_L         = 1 << 2,
    D_TO_C         = 1 << 3,
    UnifiedTrigger = 1 << 4,   // L/R analog triggers collapsed into max in lTrigger byte
    NoTriggers     = 1 << 5,   // analog trigger bytes zeroed (digital L/R bits remain)
    NoCStick       = 1 << 6,   // cStick bytes zeroed
    NoLStick       = 1 << 7,   // lStick bytes zeroed
};

enum ELBehaviors {
    X_IS_TURBO_A    = 1 << 0,
    Y_IS_TURBO_B    = 1 << 1,
};

void PadToStick(vec2* pStick);

#endif
