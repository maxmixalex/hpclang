#include <stdint.h>
#include <errno.h>

#include <hpclang/time.h>
#include <hpclang/device.h>

#include "time-math.h"
#include "clock-internal.h"
#include "datetime-internal.h"
#include "irq.h"

#define XTAL_HZ UINT64_C(12000000)
#define USEC_PER_SEC UINT64_C(1000000)

#define REG8(address) (*(volatile uint8_t *)(uintptr_t)(address))
#define REG16(address) (*(volatile uint16_t *)(uintptr_t)(address))
#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define MPLLCON REG32(0x07200004u)
#define CLKSLOW REG32(0x07200010u)
#define CLKDIVN REG32(0x07200014u)

#define TCFG0 REG32(0x07600000u)
#define TCFG1 REG32(0x07600004u)
#define TCON REG32(0x07600008u)
#define TCNTB0 REG16(0x0760000cu)
#define TCNTO0 REG16(0x07600014u)

struct time_state
{
    uint64_t origin_rtc;
    uint64_t freq_num;
    uint64_t freq_den;
    uint64_t last_us;
    uint64_t base_us;
    uint32_t period;
    uint16_t origin_count;
    int ready;
};

static struct time_state state;

static uint64_t gcd_u64(uint64_t a, uint64_t b)
{
    while (b != 0u) {
        uint64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static int rtc_seconds(uint64_t *value)
{
    struct hpclang_datetime now;
    int64_t seconds;
    if (!hpclang_rtc_read(&now) || hpclang_datetime_to_seconds(&now, &seconds) < 0)
        return 0;
    *value = (uint64_t)seconds;
    return 1;
}

static int sample(uint64_t *rtc, uint16_t *count)
{
    for (unsigned attempt = 0u; attempt < 8u; ++attempt) {
        uint64_t before;
        uint64_t after;

        if (!rtc_seconds(&before))
            continue;

        uint16_t timer = TCNTO0;

        if (!rtc_seconds(&after))
            continue;

        if (before == after) {
            *rtc = before;
            *count = timer;
            return 1;
        }
    }
    return 0;
}

void hpclang_clock_pclk(uint64_t *num, uint64_t *den)
{
    uint64_t numerator = XTAL_HZ;
    uint64_t denominator = 1u;

    uint32_t clkslow = CLKSLOW;
    uint32_t clkdivn = CLKDIVN;

    if (clkslow & (1u << 4)) {
        uint32_t slow = clkslow & 0x7u;
        if (slow != 0u)
            denominator *= (uint64_t)slow * 2u;
    } else {
        uint32_t mpllcon = MPLLCON;
        uint32_t m = ((mpllcon >> 12) & 0xffu) + 8u;
        uint32_t p = ((mpllcon >> 4) & 0x3fu) + 2u;
        uint32_t s = mpllcon & 0x3u;

        numerator *= m;
        denominator *= (uint64_t)p << s;
    }

    /* S3C2410 CLKDIVN: HDIVN is bit 1, PDIVN is bit 0. */
    if (clkdivn & (1u << 1))
        denominator *= 2u;
    if (clkdivn & (1u << 0))
        denominator *= 2u;

    uint64_t g = gcd_u64(numerator, denominator);
    *num = numerator / g;
    *den = denominator / g;
}

static int timer_frequency(uint64_t *num, uint64_t *den)
{
    uint64_t numerator, denominator;
    hpclang_clock_pclk(&numerator, &denominator);
    uint32_t prescaler = (TCFG0 & 0xffu) + 1u;
    uint32_t mux = TCFG1 & 0x0fu;
    if (mux > 3u)
        return 0;

    denominator *= prescaler;
    denominator *= 2u << mux;

    uint64_t g = gcd_u64(numerator, denominator);
    *num = numerator / g;
    *den = denominator / g;
    return 1;
}

static void time_init_unlocked(void)
{
    state.ready = 0;
    state.last_us = 0u;
    state.base_us = 0u;

    /*
     * Timer0 must already be running in auto-reload mode. hpclang never
     * changes these bits; all validated stock fixtures enter HPC1 this way.
     */
    uint32_t tcon = TCON;
    if ((tcon & 0x1u) == 0u || (tcon & 0x8u) == 0u)
        return;

    uint32_t reload = TCNTB0;
    uint32_t period = reload + 1u;
    if (period < 2u)
        return;

    uint64_t num;
    uint64_t den;
    if (!timer_frequency(&num, &den))
        return;

    /*
     * The RTC is quantized to one second. A timer wrap interval greater than
     * two seconds makes the number of wraps uniquely recoverable.
     */
    if ((uint64_t)period * den <= 2u * num)
        return;

    uint64_t rtc;
    uint16_t count;
    if (!sample(&rtc, &count))
        return;

    state.origin_rtc = rtc;
    state.origin_count = count;
    state.period = period;
    state.freq_num = num;
    state.freq_den = den;
    state.ready = 1;
}

void hpclang_time_init(void)
{
    uint32_t flags = hpclang_irq_save_disable();
    time_init_unlocked();
    hpclang_irq_restore(flags);
}

static int monotonic_now_unlocked(uint64_t *value)
{
    if (!state.ready) {
        time_init_unlocked();
        if (!state.ready)
            return 0;
    }

    uint64_t rtc;
    uint16_t count;
    if (!sample(&rtc, &count) || rtc < state.origin_rtc)
        return 0;

    uint64_t elapsed = hpclang_time_elapsed_us(rtc - state.origin_rtc, state.origin_count, count,
                                               state.period, state.freq_num, state.freq_den);

    if (elapsed > UINT64_MAX - state.base_us)
        return 0;
    elapsed += state.base_us;
    if (elapsed > state.last_us)
        state.last_us = elapsed;

    *value = state.last_us;
    return 1;
}

static int monotonic_now(uint64_t *value)
{
    uint32_t flags = hpclang_irq_save_disable();
    int result = monotonic_now_unlocked(value);
    if (!result)
        *value = state.last_us;
    hpclang_irq_restore(flags);
    return result;
}

uint64_t hpclang_time_monotonic_us(void)
{
    uint64_t value;
    (void)monotonic_now(&value);
    return value;
}

int hpclang_time_read_us(uint64_t *microseconds)
{
    if (!microseconds) {
        errno = EINVAL;
        return -1;
    }
    uint64_t value;
    if (!monotonic_now(&value)) {
        errno = EIO;
        return -1;
    }
    *microseconds = value;
    return 0;
}

void hpclang_delay_us(uint64_t microseconds)
{
    if (microseconds == 0u)
        return;

    uint64_t start;
    if (!monotonic_now(&start))
        return;

    for (;;) {
        uint64_t now;
        if (!monotonic_now(&now))
            return;
        if (now - start >= microseconds)
            return;
    }
}

int hpclang_time_clock_prepare(struct hpclang_clock_epoch *epoch)
{
    if (hpclang_time_read_us(&epoch->microseconds) || !rtc_seconds(&epoch->rtc)) {
        errno = EIO;
        return -1;
    }
    epoch->count = TCNTO0;
    return 0;
}

void hpclang_time_clock_commit(const struct hpclang_clock_epoch *epoch)
{
    /* Timer0 configuration is unchanged during the IRQ-guarded transaction.
     * Sample first, before doing rate arithmetic. Mixed-rate transition ticks
     * are bounded using the faster rate; never invent a forward clock jump. */
    uint16_t count = TCNTO0;
    uint64_t old_num = state.freq_num, old_den = state.freq_den;
    (void)timer_frequency(&state.freq_num, &state.freq_den);
    uint64_t num = state.freq_num, den = state.freq_den;
    if (old_num * den > num * old_den) {
        num = old_num;
        den = old_den;
    }
    uint64_t elapsed = hpclang_time_elapsed_us(0, epoch->count, count, state.period, num, den);
    uint64_t base =
        elapsed > UINT64_MAX - epoch->microseconds ? UINT64_MAX : epoch->microseconds + elapsed;
    state.origin_count = count;
    state.origin_rtc = epoch->rtc;
    state.base_us = base;
    state.last_us = base;
}

static int datetime_set_unlocked(const struct hpclang_datetime *value)
{
    if (!hpclang_datetime_valid(value)) {
        errno = EINVAL;
        return -1;
    }
    if (value->year < 2000 || value->year > 2099) {
        errno = ERANGE;
        return -1;
    }
    if (hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN) {
        errno = ENOSYS;
        return -1;
    }
    uint64_t before;
    if (hpclang_time_read_us(&before) < 0)
        return -1;
    uint16_t count_before = TCNTO0;
    if (hpclang_rtc_write(value) < 0)
        return -1;
    uint64_t rtc_after;
    uint16_t count_after;
    int sampled = sample(&rtc_after, &count_after);
    if (!sampled) {
        int64_t requested;
        hpclang_datetime_to_seconds(value, &requested);
        rtc_after = (uint64_t)requested;
        count_after = TCNTO0;
    }
    /* The bounded register transaction is shorter than one Timer0 period.
     * Count its elapsed ticks independently of the deliberately changed RTC. */
    uint64_t elapsed = hpclang_time_elapsed_us(0, count_before, count_after, state.period,
                                               state.freq_num, state.freq_den);
    state.base_us = elapsed > UINT64_MAX - before ? UINT64_MAX : before + elapsed;
    state.last_us = state.base_us;
    state.origin_rtc = rtc_after;
    state.origin_count = count_after;
    if (!sampled) {
        errno = EIO;
        return -1;
    }
    int64_t requested;
    hpclang_datetime_to_seconds(value, &requested);
    if (rtc_after < (uint64_t)requested || rtc_after - (uint64_t)requested > 1) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int hpclang_datetime_set(const struct hpclang_datetime *value)
{
    uint32_t flags = hpclang_irq_save_disable();
    int result = datetime_set_unlocked(value);
    hpclang_irq_restore(flags);
    return result;
}

int hpclang_time_read_us_masked(uint64_t *microseconds)
{
    if (!microseconds) {
        errno = EINVAL;
        return -1;
    }
    if (!monotonic_now_unlocked(microseconds)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

uint64_t hpclang_time_monotonic_us_masked(void)
{
    uint64_t value;
    return monotonic_now_unlocked(&value) ? value : state.last_us;
}
