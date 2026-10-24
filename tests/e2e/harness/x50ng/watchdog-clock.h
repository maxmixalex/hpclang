#ifndef HPCLANG_WATCHDOG_CLOCK_H
#define HPCLANG_WATCHDOG_CLOCK_H
#include <stdint.h>

/* Host-only S3C2410 down-counter model. phase is peripheral cycles scaled by
 * 1e6; retain fractions across observations instead of rounding every tick.
 * Register writes initialize a new divider phase. Clock-rate changes first
 * advance at the old rate, then replace hz without losing that phase. */
struct hpclang_watchdog_clock
{
    uint64_t last_us, phase;
    uint32_t count, reload, divider, hz;
    int enabled;
};

static inline int hpclang_watchdog_advance(struct hpclang_watchdog_clock *c, uint64_t now)
{
    if (now < c->last_us)
        return 0;
    uint64_t elapsed = now - c->last_us;
    c->last_us = now;
    if (!c->enabled || !c->hz || !c->divider)
        return 0;
    uint64_t unit = (uint64_t)c->divider * 1000000;
    __uint128_t cycles = (__uint128_t)elapsed * c->hz + c->phase;
    __uint128_t ticks = cycles / unit;
    c->phase = (uint64_t)(cycles % unit);
    uint32_t first = c->count ? c->count : 1;
    if (ticks < first) {
        c->count -= (uint32_t)ticks;
        return 0;
    }
    uint32_t period = c->reload ? c->reload : 1;
    c->count = c->reload ? period - (uint32_t)((ticks - first) % period) : 0;
    return 1; /* Multiple elapsed periods coalesce at the interrupt controller. */
}

static inline uint64_t hpclang_watchdog_delay(const struct hpclang_watchdog_clock *c)
{
    if (!c->enabled || !c->hz || !c->divider)
        return UINT64_MAX;
    uint64_t cycles = (uint64_t)(c->count ? c->count : 1) * c->divider * 1000000 - c->phase;
    return cycles / c->hz + (cycles % c->hz != 0);
}
#endif
