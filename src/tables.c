#include "types.h"
#include "tables.h"

const uint8_t atan_table[128] = {
    #include "../gen/atan_table"
};

uint8_t fast_atan2(vec2* pStick) {
    uint8_t ax;
    uint8_t ay;
    ax = pStick->x < 0 ? -pStick->x : pStick->x;
    ay = pStick->y < 0 ? -pStick->y : pStick->y;

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

uint8_t root(uint16_t n) {
    uint8_t  result = 0;
    uint16_t bit    = 1u << 14;
    uint8_t trial;

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

void CalculateStick(vec2* pStick){
    uint8_t temp;
    temp = root(
        (pStick->x * pStick->x) +
        (pStick->y * pStick->y)
    );

    pStick->x = fast_atan2(pStick);
    pStick->y = temp;
}
