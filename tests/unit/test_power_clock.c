#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include "power-clock.h"

int main(void)
{
    const uint32_t pll = 0x43012u; /* (67+8)*12MHz / ((1+2)*4) = 75MHz. */
    assert(hpclang_power_clock_hz(pll, 0) == 75000000);
    assert(hpclang_power_clock_hz(pll, 0x10) == 12000000);
    assert(hpclang_power_clock_hz(pll, 0x11) == 6000000);
    assert(hpclang_power_clock_hz(pll, 0x17) == 857142);
    uint32_t slow = 99, lcd = 99;
    assert(hpclang_power_economy_clock(pll, 0x80, 0x6421, &slow, &lcd) == 0);
    assert(slow == 0x90 && lcd == 0x1021); /* Exact 100→16 divider ratio. */
    assert(hpclang_power_economy_clock(pll, 0x80, 0x1021, &slow, &lcd) == 0);
    assert(lcd == 0x321); /* Round up, never increase panel scan frequency. */
    assert(hpclang_power_economy_clock(pll, 0x80, 0x221, &slow, &lcd) == 0);
    assert(lcd == 0x221); /* STN hardware minimum, lower refresh is allowed. */
    assert(hpclang_power_economy_clock(pll, 0xb1, 0x820, &slow, &lcd) == 0);
    assert(slow == 0xb1 && lcd == 0x820); /* Already 6MHz; preserve PLL/panel off. */
    assert(hpclang_power_economy_clock(pll, 0, 0x101, &slow, &lcd) == -1 && errno == ENOTSUP);
    assert(slow == 0xb1 && lcd == 0x820);
    assert(hpclang_power_economy_clock(pll, 0, 0x221, NULL, &lcd) == -1 && errno == EINVAL);
    return 0;
}
