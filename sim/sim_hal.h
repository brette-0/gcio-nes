#ifndef SIM_HAL_H
#define SIM_HAL_H
#include <stdint.h>

typedef struct {
    uint8_t DIR;
    uint8_t DIRSET;
    uint8_t DIRCLR;
    uint8_t DIRTGL;
    uint8_t OUT;
    uint8_t OUTSET;
    uint8_t OUTCLR;
    uint8_t OUTTGL;
    uint8_t IN;
    uint8_t INTFLAGS;
    uint8_t PIN0CTRL;
    uint8_t PIN1CTRL;
    uint8_t PIN2CTRL;
    uint8_t PIN3CTRL;
    uint8_t PIN4CTRL;
    uint8_t PIN5CTRL;
    uint8_t PIN6CTRL;
    uint8_t PIN7CTRL;
} sim_port_t;

extern sim_port_t PORTA;

// stub AVR constants
#define PORT_PULLUPEN_bm      0x08
#define PORT_ISC_BOTHEDGES_gc 0x01
#define PORT_ISC_RISING_gc    0x02

// stub sei/cli
#define sei()
#define cli()

#endif
