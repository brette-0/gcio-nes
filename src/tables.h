#ifndef TYPES_H
#define TYPES_H
#include <avr/io.h>>
#include "types.h"

static volatile const uint16_t square_table[256] = {
    #include "gen/square_table"
};

static volatile const uint8_t atan_table[128] = {
    #include "gen/atan_table"
};

void CalculateStick(vec2* pStick){
    static volatile vec2 temp;
    temp.y = sqrt(
        square_table[pStick->x] +
        square_table[pStick->y]
    );

    temp.x = fast_atan2(pStick);
    pStick = &temp;
}

volatile uint8_t sqrt(uint16_t n) {
    static volatile uint8_t  result = 0;
    static volatile uint16_t bit    = 1u << 14;
    static volatile trial;

    while (bit > n) bit >>= 2;

    while (bit) {
        trial = result + bit;
        if (n >= trial) {
            n      -= trial;
            result  = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return result;
}

volatile uint8_t fast_atan2(vec2* pStick) {
    uint8_t ax = pStick->x < 0 ? -pStick->x : pStick->x;
    uint8_t ay = pStick->y < 0 ? -pStick->y : pStick->y;

    uint8_t ratio;
    uint8_t base;

    if (ax >= ay) {
        ratio = ax ? (uint16_t)ay * 128 / ax : 0;
        base = atan_table[ratio];
    } else {
        ratio = ay ? (uint16_t)ax * 128 / ay : 0;
        base = 64 - atan_table[ratio];          // π/2 minus angle
    }

    // octant reconstruction
    if (pStick->x < 0) base = 128 - base;       // mirror across π/2
    if (pStick->y < 0) base = 256 - base;       // mirror across 0

    return base;
}
#endif