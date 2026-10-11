#ifndef HPCLANG_POWER_CLOCK_H
#define HPCLANG_POWER_CLOCK_H
#include <stdint.h>

uint32_t hpclang_power_clock_hz(uint32_t pll, uint32_t slow);
int hpclang_power_economy_clock(uint32_t pll, uint32_t slow, uint32_t lcd, uint32_t *next_slow,
                                uint32_t *next_lcd);
#endif
