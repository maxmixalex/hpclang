#include <stdint.h>

volatile uint32_t hpclang_probe_word = 0x12345678u;
static uint32_t hpclang_accum = 7u;

uint32_t hpclang_add(uint32_t a, uint32_t b)
{
    hpclang_accum += a;
    return hpclang_accum + b + hpclang_probe_word;
}
