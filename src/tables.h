#ifndef TABLES_H
#define TABLES_H
#include "types.h"

void CalculateStick(vec2* pStick);
uint8_t fast_atan2(vec2* pStick);
uint8_t root(uint16_t n);

extern const uint8_t atan_table[128];
#endif