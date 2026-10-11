#include <assert.h>
#include <stdint.h>
#include "timer-clock.h"

int main(void)
{
    assert(timer_clock_remaining(1000000, 75000000, 12000000) == 6250000);
    assert(timer_clock_remaining(6250000, 12000000, 75000000) == 1000000);
    assert(timer_clock_remaining(1, 12000000, 75000000) == 1);
    assert(timer_clock_remaining(0, 75000000, 12000000) == 0);
    assert(timer_clock_remaining(12345, 12000000, 12000000) == 12345);
    assert(timer_clock_remaining(UINT64_MAX, UINT32_MAX, 1) == UINT64_MAX);
    assert(timer_clock_remaining(UINT64_MAX, 1, UINT32_MAX) == UINT64_C(4294967297));
    return 0;
}
