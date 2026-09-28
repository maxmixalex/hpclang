#include <stdint.h>

#include <hpclang/time.h>

#include "time-math.h"

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

#define RTC_SECOND REG8(0x07b00070u)
#define RTC_MINUTE REG8(0x07b00074u)
#define RTC_HOUR REG8(0x07b00078u)
#define RTC_DATE REG8(0x07b0007cu)
#define RTC_MONTH REG8(0x07b00084u)
#define RTC_YEAR REG8(0x07b00088u)

struct time_state
{
    uint64_t origin_rtc;
    uint64_t freq_num;
    uint64_t freq_den;
    uint64_t last_us;
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

static int bcd(unsigned value)
{
    unsigned lo = value & 0x0fu;
    unsigned hi = (value >> 4) & 0x0fu;
    if (lo > 9u || hi > 9u)
        return -1;
    return (int)(hi * 10u + lo);
}

static int leap_year(unsigned year)
{
    return (year % 4u) == 0u;
}

static unsigned days_in_month(unsigned year, unsigned month)
{
    static const uint8_t days[] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};

    if (month == 2u && leap_year(year))
        return 29u;
    return days[month - 1u];
}

static int rtc_seconds_once(uint64_t *value)
{
    uint8_t sec_before = RTC_SECOND;
    uint8_t year_raw = RTC_YEAR;
    uint8_t month_raw = RTC_MONTH;
    uint8_t date_raw = RTC_DATE;
    uint8_t hour_raw = RTC_HOUR;
    uint8_t minute_raw = RTC_MINUTE;
    uint8_t sec_after = RTC_SECOND;

    if (sec_before != sec_after)
        return 0;

    int year2 = bcd(year_raw);
    int month = bcd(month_raw & 0x1fu);
    int date = bcd(date_raw & 0x3fu);
    int hour = bcd(hour_raw & 0x3fu);
    int minute = bcd(minute_raw & 0x7fu);
    int second = bcd(sec_before & 0x7fu);

    if (year2 < 0 || month < 1 || month > 12 || date < 1 || hour < 0 || hour > 23 || minute < 0 ||
        minute > 59 || second < 0 || second > 59)
        return 0;

    unsigned year = 2000u + (unsigned)year2;
    unsigned dim = days_in_month(year, (unsigned)month);
    if ((unsigned)date > dim)
        return 0;

    uint64_t days = 0u;
    for (unsigned y = 2000u; y < year; ++y)
        days += leap_year(y) ? 366u : 365u;

    for (unsigned m = 1u; m < (unsigned)month; ++m)
        days += days_in_month(year, m);

    days += (unsigned)date - 1u;

    *value =
        days * UINT64_C(86400) + (uint64_t)hour * 3600u + (uint64_t)minute * 60u + (uint64_t)second;
    return 1;
}

static int rtc_seconds(uint64_t *value)
{
    for (unsigned attempt = 0u; attempt < 8u; ++attempt)
        if (rtc_seconds_once(value))
            return 1;
    return 0;
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

static int timer_frequency(uint64_t *num, uint64_t *den)
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

void hpclang_time_init(void)
{
    state.ready = 0;
    state.last_us = 0u;

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

static int monotonic_now(uint64_t *value)
{
    if (!state.ready) {
        hpclang_time_init();
        if (!state.ready)
            return 0;
    }

    uint64_t rtc;
    uint16_t count;
    if (!sample(&rtc, &count) || rtc < state.origin_rtc)
        return 0;

    uint64_t elapsed = hpclang_time_elapsed_us(rtc - state.origin_rtc, state.origin_count, count,
                                               state.period, state.freq_num, state.freq_den);

    if (elapsed > state.last_us)
        state.last_us = elapsed;

    *value = state.last_us;
    return 1;
}

uint64_t hpclang_time_monotonic_us(void)
{
    uint64_t value;
    return monotonic_now(&value) ? value : state.last_us;
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
