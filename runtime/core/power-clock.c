#include <errno.h>
#include "power-clock.h"

uint32_t hpclang_power_clock_hz(uint32_t pll, uint32_t slow)
{
    if (slow & 0x10u)
        return (slow & 7u) ? 12000000u / (2u * (slow & 7u)) : 12000000u;
    uint64_t numerator = UINT64_C(12000000) * (((pll >> 12) & 255u) + 8u);
    uint32_t denominator = (((pll >> 4) & 63u) + 2u) << (pll & 3u);
    return (uint32_t)(numerator / denominator);
}

int hpclang_power_economy_clock(uint32_t pll, uint32_t slow, uint32_t lcd, uint32_t *next_slow,
                                uint32_t *next_lcd)
{
    if (!next_slow || !next_lcd) {
        errno = EINVAL;
        return -1;
    }
    /* S3C2410A 7-11..7-13 and 7-22: leave both PLLs and bus divisors alone.
     * The crystal path lowers FCLK without changing a PLL or its lock timing.
     * An already slower host profile must never be sped up by economy mode. */
    uint32_t frequency = hpclang_power_clock_hz(pll, slow);
    uint32_t target = frequency > 12000000u ? (slow & ~0x17u) | 0x10u : slow;
    uint32_t divider = (lcd >> 8) & 1023u;
    if ((lcd & 0x60u) != 0x20u || divider < 2u) {
        errno = ENOTSUP;
        return -1;
    }
    /* STN uses HCLK/(2*CLKVAL), unlike TFT's (CLKVAL+1). HCLK's divisor
     * is unchanged, so the FCLK ratio is also the HCLK ratio. Round upward:
     * the panel must not be driven faster than its existing configuration. */
    uint64_t scaled = (uint64_t)divider * hpclang_power_clock_hz(pll, target);
    divider = (uint32_t)((scaled + frequency - 1u) / frequency);
    if (divider < 2u)
        divider = 2u;
    *next_slow = target;
    *next_lcd = (lcd & 0xffu) | (divider << 8);
    return 0;
}
