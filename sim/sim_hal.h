#ifndef SIM_HAL_H
#define SIM_HAL_H
#include <stdint.h>

#define TCA_SINGLE_CMD_RESET_gc        0x01
#define TCA_SINGLE_CLKSEL_DIV1_gc      0x01
#define TCA_SINGLE_WGMODE_SINGLESLOPE_gc 0x03
#define TCA_SINGLE_CMP0EN_bm           0x10
#define TCA_SINGLE_OVF_bm              0x01
#define TCA_SINGLE_ENABLE_bm           0x01

#define TCB_CNTMODE_PW_gc              0x00
#define TCB_CAPTEI_bm                  0x01
#define TCB_EDGE_bm                    0x02
#define TCB_FILTER_bm                  0x04
#define TCB_CAPT_bm                    0x01
#define TCB_CLKSEL_CLKDIV1_gc          0x01
#define TCB_ENABLE_bm                  0x01

#define EVSYS_ASYNCCH0_PORTA_PIN0_gc   0x00
#define EVSYS_ASYNCUSER0_ASYNCCH0_gc   0x00

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

typedef struct {
    uint8_t CTRLA;
    uint8_t STATUS;
    uint8_t LVL0PRI;
    uint8_t LVL1VEC;
} sim_cpuint_t;

typedef struct {
    uint8_t CTRLA;
    uint8_t CTRLB;
    uint8_t CTRLESET;
    uint8_t EVCTRL;
    uint8_t INTCTRL;
    uint8_t INTFLAGS;
    uint8_t CNT;
    uint8_t PER;
    uint8_t CMP0;
    uint8_t CMP0BUF;
} sim_tca_single_t;

typedef struct {
    uint8_t CTRLA;
    uint8_t CTRLB;
    uint8_t EVCTRL;
    uint8_t INTCTRL;
    uint8_t INTFLAGS;
    uint16_t CCMP;
} sim_tcb_t;

typedef struct {
    uint8_t ASYNCCH0;
    uint8_t ASYNCUSER0;
} sim_evsys_t;

typedef struct {
    sim_tca_single_t SINGLE;
} sim_tca_t;

extern sim_port_t PORTA;
extern sim_cpuint_t CPUINT;
extern sim_tca_t TCA0;
extern sim_tcb_t TCB0;
extern sim_evsys_t EVSYS;

#define PORTA_PORT_vect_num  3

// stub AVR constants
#define PORT_PULLUPEN_bm      0x08
#define PORT_ISC_BOTHEDGES_gc 0x01
#define PORT_ISC_RISING_gc    0x02

// stub sei/cli
#define sei()
#define cli()

#endif
