#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

typedef union wide_t {
    uint64_t hunk;
    uint8_t  arr[8];
} wide_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} vec2;

typedef struct {
    wide_t  content;
    uint8_t shift;
} shift_register;

#define SHIFT(reg) ++(reg).shift
#define READ_SR(reg) (                      \
    (reg).content.arr[(reg).shift >> 3] &   \
    (1 << ((reg).shift & 0b111))            \
)

#define RESET_SR(reg) (reg).shift = 0

#define W_FLIP(l, r)                    \
    for (uint8_t i = 0; i < 8; i++)     \
        (l).arr[i] ^= (r).arr[i];

#define W_MASK_BIT(wide, bit)                           \
    (wide).arr[(bit) >> 3] &= ~(1 << ((bit) & 0b111)); \
    (wide).arr[(bit) >> 3] |=  (1 << ((bit) & 0b111))

#endif
