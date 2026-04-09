#ifndef GC_H
#define GC_H

#ifndef SIMULATION
#include <avr/io.h>
#else
#include "../sim/sim_hal.h"
#endif

#include "types.h"

#define GC_PIN 0

#define GC_BIT_1    19
#define GC_BIT_0    59
#define GC_RESPONSE_LEN 8

typedef struct {
    const uint8_t data[3];
    const uint8_t bits;     // number of data bits (not including stop)
} gc_cmd_entry;

enum gc_command {
    GC_CMD_PROBE,
    GC_CMD_POLL,
    GC_CMD_ORIGIN,
    GC_CMD_RUMBLE_ON,
    GC_CMD_COUNT
};

extern volatile uint8_t gc_tx_bit;
extern volatile uint8_t gc_tx_byte;
extern volatile uint8_t gc_tx_total;
extern volatile const gc_cmd_entry *gc_active_cmd;
extern volatile uint8_t gc_tx_done;

extern volatile uint8_t gc_rx_buffer[GC_RESPONSE_LEN];
extern volatile uint8_t gc_rx_byte;
extern volatile uint8_t gc_rx_bit;
extern volatile uint8_t gc_rx_count;
extern volatile uint8_t gc_rx_done;

extern const gc_cmd_entry gc_commands[GC_CMD_COUNT];

void gc_init_tca(void);
void gc_init_tcb(void);
void gc_init_events(void);
void gc_send(enum gc_command cmd);
void gc_start_receive(void);

#endif
