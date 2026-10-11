#ifndef HPCLANG_TIMER_CLOCK_H
#define HPCLANG_TIMER_CLOCK_H
#include <stdint.h>

/* Preserve the remaining peripheral-clock cycles across a frequency change.
 * Round the new microsecond deadline upward, never firing a timer early. */
static inline uint64_t timer_clock_remaining(uint64_t remaining, uint32_t before, uint32_t after)
{
    if (!before || !after || before == after)
        return remaining;
    uint64_t whole = remaining / after;
    uint64_t fraction = ((remaining % after) * before + after - 1u) / after;
    if (whole > (UINT64_MAX - fraction) / before)
        return UINT64_MAX;
    return whole * before + fraction;
}
#endif
