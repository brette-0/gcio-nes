#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

typedef struct {
    uint8_t x;
    uint8_t y;
} vec2;

typedef struct wide_t {
    uint8_t  arr[8];
} wide_t;

typedef struct input_t {
    uint8_t  buttons[2];
    vec2     lStick;
    vec2     cStick;
    uint8_t  lTrigger;
    uint8_t  rTrigger;
} input_t;


#define W_MASK_BIT(wide, bit)                           \
    (wide).arr[(bit) >> 3] &= ~(1 << ((bit) & 0b111)); \
    (wide).arr[(bit) >> 3] |=  (1 << ((bit) & 0b111))

#endif
