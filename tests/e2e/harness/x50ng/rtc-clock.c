#include <string.h>
#include "rtc-clock.h"
#include "datetime-internal.h"

static uint8_t bcd(unsigned value)
{
    return (uint8_t)((value / 10) * 16 + value % 10);
}

static void encode(const struct hpclang_datetime *civil, uint8_t raw[7])
{
    unsigned weekday;
    hpclang_datetime_weekday(civil, &weekday);
    raw[0] = bcd(civil->second);
    raw[1] = bcd(civil->minute);
    raw[2] = bcd(civil->hour);
    raw[3] = bcd(civil->day);
    raw[4] = bcd(weekday + 1);
    raw[5] = bcd(civil->month);
    raw[6] = bcd(civil->year % 100);
}

void hpclang_rtc_clock_init(struct hpclang_rtc_clock *clock, const struct hpclang_datetime *civil,
                            int64_t now_us)
{
    memset(clock, 0, sizeof(*clock));
    clock->anchor_us = now_us;
    clock->valid = hpclang_datetime_to_seconds(civil, &clock->seconds) == 0;
    if (clock->valid)
        encode(civil, clock->raw);
    clock->weekday = clock->raw[4];
}

void hpclang_rtc_clock_read(struct hpclang_rtc_clock *clock, int64_t now_us, uint8_t raw[7])
{
    if (clock->valid && !clock->stopped) {
        struct hpclang_datetime civil;
        int64_t elapsed = now_us > clock->anchor_us ? (now_us - clock->anchor_us) / 1000000 : 0;
        if (hpclang_datetime_from_seconds(clock->seconds + elapsed, &civil) == 0) {
            encode(&civil, clock->raw);
            int64_t days = (clock->seconds + elapsed) / 86400 - clock->seconds / 86400;
            clock->raw[4] = clock->weekday >= 1 && clock->weekday <= 7
                                ? (uint8_t)((clock->weekday - 1 + days) % 7 + 1)
                                : clock->weekday;
        }
    }
    memcpy(raw, clock->raw, 7);
}

void hpclang_rtc_clock_write(struct hpclang_rtc_clock *clock, unsigned index, uint8_t value,
                             int64_t now_us)
{
    if (index >= 7)
        return;
    uint8_t raw[7];
    hpclang_rtc_clock_read(clock, now_us, raw);
    raw[index] = value;
    uint8_t sample[7] = {raw[0], raw[6], raw[5], raw[3], raw[2], raw[1], raw[0]};
    struct hpclang_datetime civil;
    clock->valid = hpclang_rtc_decode(sample, &civil);
    if (clock->valid)
        hpclang_datetime_to_seconds(&civil, &clock->seconds);
    int64_t fraction =
        !clock->stopped && now_us >= clock->anchor_us ? (now_us - clock->anchor_us) % 1000000 : 0;
    clock->anchor_us = now_us - fraction;
    memcpy(clock->raw, raw, 7);
    clock->weekday = raw[4];
}

uint32_t hpclang_rtc_tick_interval(uint8_t selector, unsigned *remainder)
{
    unsigned numerator = ((unsigned)(selector & 127u) + 1u) * 1000000u + *remainder;
    *remainder = numerator % 128u;
    return numerator / 128u;
}

void hpclang_rtc_clock_stop(struct hpclang_rtc_clock *clock, int stopped, int64_t now_us)
{
    if (!!stopped == clock->stopped)
        return;
    uint8_t raw[7];
    hpclang_rtc_clock_read(clock, now_us, raw);
    /* Commit the current calendar before resetting the divider phase. */
    hpclang_rtc_clock_write(clock, 0, raw[0], now_us);
    clock->anchor_us = now_us;
    clock->stopped = !!stopped;
}

int hpclang_rtc_clock_resume(struct hpclang_rtc_clock *clock, uint32_t phase_us, int64_t now_us)
{
    if (phase_us >= 1000000u || (clock->stopped && phase_us) || now_us < 0)
        return 0;
    clock->anchor_us = now_us - phase_us;
    return 1;
}
