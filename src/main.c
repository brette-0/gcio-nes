#ifndef SIMULATION
#include <avr/io.h>
#include <avr/interrupt.h>
#else
#include "../sim/sim_hal.h"
#endif

#include "types.h"
#include "gc.h"
#include "main.h"

input_t  buffers[2];
volatile wide_t   flip;
volatile wide_t   mask;
input_t* targetBuffer;
volatile uint8_t  shift;

volatile uint8_t behavior;
static   uint8_t pollClock;
static   uint8_t lSetup;

// ISR-private state — accessed only inside __vector_3
static uint8_t  nInupts;
static uint8_t  latch;
static uint8_t  task;
static uint8_t  nTask;
static uint8_t  OUT;


#ifndef SIMULATION
int main(void){
    init();
    sei();

    while (1){
        // Job 1: GC response arrived — process and reload buffers
        if (gc_rx_done) {
            gc_rx_done = 0;

            // raw view: copy RX directly into legacy shift register
            cli();

            for (uint8_t i = 0; i < GC_RESPONSE_LEN; i++)
                ((wide_t*)targetBuffer)->arr[i] = gc_rx_buffer[i];

            sei();

            // processed view: preprocess on a stack-local, then publish
            InputPreprocess();

            cli();
            sei();

            // kick off next poll immediately
            gc_send(GC_CMD_POLL);
        }

        // Job 2: nothing pending and GC idle — start polling
        if (gc_tx_done && !gc_rx_done) {
            gc_send(GC_CMD_POLL);
        }
    }
}
#endif


void init(void){
    // NES pins
    PORTA.DIRSET = (1 << 2);                                       // D0 output
    PORTA.OUTSET = (1 << 2);                                       // D0 idle high
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;     // OUT — any edge
    PORTA.DIRCLR = (1 << 3);
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;        // CLK — rising

    // GC peripherals
    gc_init_tca();
    gc_init_tcb();
    gc_init_events();

    // NES interrupts are highest priority
    CPUINT.LVL1VEC = PORTA_PORT_vect_num;

    // start first GC poll
    gc_send(GC_CMD_POLL);
    targetBuffer = buffers;
}


void handle_interrupt(void){
    HANDLE(__OUT) {
        pollClock = 0;
        OUT = PORTA.IN & __OUT;
        console_write();
    }

    HANDLE(__CLK) {
        if (latch && task != REPORT) {
            // console sending bits TO us
            console_read();
            return;
        }

        // driving D0 — legacy or REPORT
        if (((wide_t*)targetBuffer)->arr[shift >> 3] & (1 << (shift & 0b111)))  PORTA.OUTSET = __D0;
        else                                                                    PORTA.OUTCLR = __D0;
        shift++;
        pollClock++;
    }
}

void console_read(void){
    switch (task){
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

        case LSETUP:
            lSetup |= (OUT << nTask);
            break;

        case RUMBLE:
            break;

        default:
            break;
    }

    if (--nTask) return;
    latch = 0;
    task  = LEGACY;
}

static inline void LegacyButtonPreProcess(wide_t* raw) {
    targetBuffer->buttons[0] =
        ((raw->arr[0] & IA)     ? LA         : 0) |
        ((raw->arr[0] & IB)     ? LB         : 0) |
        ((raw->arr[1] & IUp)    ? LUp        : 0) |
        ((raw->arr[1] & IDown)  ? LDown      : 0) |
        ((raw->arr[1] & ILeft)  ? LLeft      : 0) |
        ((raw->arr[1] & IRight) ? LRight     : 0) |
        ((raw->arr[1] & IZ)     ? LSelect    : 0) |
        ((raw->arr[0] & IStart) ? LStart     : 0);
}
static inline void ButtonPreprocess(wide_t* raw) {
    targetBuffer->buttons[0] = 
        ((raw->arr[0] & IB)        ? B     : 0) |
        ((raw->arr[0] & IY)        ? Y     : 0) |
        ((raw->arr[1] & IZ)        ? Z     : 0) |
        ((raw->arr[0] & IStart)    ? Start : 0) |
        ((raw->arr[1] & IUp)       ? Up    : 0) |
        ((raw->arr[1] & IDown)     ? Down  : 0) |
        ((raw->arr[1] & ILeft)     ? Left  : 0) |
        ((raw->arr[1] & IRight)    ? Right : 0);    
    ;

        targetBuffer->buttons[1] = 
            ((raw->arr[0] & IA)    ? A     : 0) |
            ((raw->arr[0] & IX)    ? X     : 0) |
            ((raw->arr[1] & IL)    ? L     : 0) |
            ((raw->arr[1] & IR)    ? R     : 0) ;
        
}

static inline uint8_t angle_sign_to_pad(const int8_t angle) {
    if      (!angle)    return 0b00;
    else if (angle > 0) return 0b10;
    else                return 0b01;
}

inline static void LStickToPad(wide_t* raw) {
    raw->arr[0] |= angle_sign_to_pad((int8_t)targetBuffer->lStick.x) << 6;
    raw->arr[0] |= angle_sign_to_pad((int8_t)targetBuffer->lStick.y) << 4;
}

void InputPreprocess(){
    wide_t* raw = (wide_t*)targetBuffer;
    if (task == LEGACY) {
        LegacyButtonPreProcess(raw);
        LStickToPad(raw);
    } else {
        ButtonPreprocess(raw);

        for (uint8_t i = 0; i < 8; i++)
            raw->arr[i] ^= flip.arr[i];

        if (behavior & L_TO_C) {
            if (!*(uint16_t*)&targetBuffer->cStick) targetBuffer->cStick = targetBuffer->lStick;
        } else if (behavior & C_TO_L){
            if (!*(uint16_t*)&targetBuffer->lStick) targetBuffer->lStick = targetBuffer->cStick;
        }

        if (behavior & D_TO_L){
            PadToStick(&targetBuffer->lStick);
        }

        if (behavior & D_TO_C){
            PadToStick(&targetBuffer->cStick);
        }
    }
}

void console_write(void){
    if (!OUT){
        if (task == LEGACY) {
            shift = 0;
        } else {
            latch = 1;
            switch (task) {
                case REPORT:
                    shift = 0;
                    nTask = nInupts;
                    break;

                case BEHAVE:
                    behavior = 0;
                    nTask = 8;
                    break;

                case INMASK:
                    nInupts = 0;
                    goto wide;

                case INVERT:
                wide:
                    nTask = 64;
                    break;

                case LSETUP:
                    nTask = 2;
                    break;

                default:
                    lSetup = 0;
                    nTask  = 0;
                    break;
            }
        }
    }
}

void PadToStick(vec2* pStick) {
    const uint8_t temp = targetBuffer->buttons[0] & (
        Up | Down | Left | Right
    );
    pStick->x  = temp & 0x40 ? 0x80 : 0;
    pStick->x += temp & 0x80 ? 0x7f : 0;
    pStick->y  = temp & 0x10 ? 0x80 : 0;
    pStick->y += temp & 0x20 ? 0x7f : 0;
}


void LegacyProcessTurboButtons(wide_t* raw) {
    if (
        (lSetup & X_IS_TURBO_A)     && 
        (raw->arr[0] & IX)
    ) {
        targetBuffer->buttons[0] |= (pollClock & 0x07)
                            ? 0
                            : LA;
    }

    if (
        (lSetup & Y_IS_TURBO_B)     && 
        (raw->arr[0] & IY)
    ) {
        targetBuffer->buttons[0] |= (pollClock & 0x07)
                            ? 0
                            : LB;
    }
}