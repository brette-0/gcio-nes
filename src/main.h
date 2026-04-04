#ifndef  MAIN_H
#define  MAIN_H
#include <avr/io.h>
#include <avr/interrupt.h>

extern volatile uint64_t raw;
extern volatile uint64_t capture;
extern volatile uint64_t flip;
extern volatile uint64_t mask;

extern volatile uint8_t  behavior;
extern volatile uint8_t  nInupts;

extern volatile uint8_t  shift;
extern volatile uint8_t  latch;

extern volatile uint8_t  task;
extern volatile uint8_t  nTask;

extern volatile uint8_t  __PD2;

void init(void);
void console_read(void);
void console_write(void);


ISR(INT0_vect) { console_write(); }

ISR(INT1_vect) { console_read(); }

enum ETasks {
    LEGACY,
    REPORT,
    INMASK,
    INVERT,
    RUMBLE,
    LSETUP
};

#endif