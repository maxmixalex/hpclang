#include <assert.h>
#include "rtc-clock.h"

int main(void)
{
    /* Saved process time is unrelated to the next process's timer origin. */
    struct hpclang_datetime saved = {2026, 10, 23, 12, 34, 56};
    struct hpclang_rtc_clock resumed;
    uint8_t sample[7];
    hpclang_rtc_clock_init(&resumed, &saved, 90000000);
    assert(hpclang_rtc_clock_resume(&resumed, 250000, 100));
    hpclang_rtc_clock_read(&resumed, 750099, sample);
    assert(sample[0] == 0x56);
    hpclang_rtc_clock_read(&resumed, 750100, sample);
    assert(sample[0] == 0x57);
    int64_t anchor = resumed.anchor_us;
    assert(!hpclang_rtc_clock_resume(&resumed, 1000000, 100));
    assert(!hpclang_rtc_clock_resume(&resumed, 0, -1));
    assert(resumed.anchor_us == anchor);
    hpclang_rtc_clock_stop(&resumed, 1, 750100);
    anchor = resumed.anchor_us;
    assert(!hpclang_rtc_clock_resume(&resumed, 1, 0));
    assert(resumed.anchor_us == anchor);
    assert(hpclang_rtc_clock_resume(&resumed, 0, 0));
    hpclang_rtc_clock_read(&resumed, 90000000, sample);
    assert(sample[0] == 0x57 && resumed.stopped);

    for (unsigned selector = 0; selector < 128; ++selector) {
        unsigned remainder = 127;
        uint64_t deadline = 0;
        for (unsigned tick = 1; tick <= 4096; ++tick) {
            deadline += hpclang_rtc_tick_interval((uint8_t)selector, &remainder);
            uint64_t numerator = (uint64_t)tick * (selector + 1u) * 1000000u;
            assert(deadline == (numerator + 127u) / 128u);
            assert(remainder < 128);
        }
    }
    struct hpclang_rtc_clock clock;
    struct hpclang_datetime leap = {2024, 2, 28, 23, 59, 59};
    uint8_t raw[7];
    hpclang_rtc_clock_init(&clock, &leap, 1000000);
    hpclang_rtc_clock_read(&clock, 2999999, raw);
    assert(raw[0] == 0 && raw[1] == 0 && raw[2] == 0 && raw[3] == 0x29 && raw[5] == 2);
    hpclang_rtc_clock_read(&clock, 86402000000LL, raw);
    assert(raw[3] == 1 && raw[5] == 3);

    hpclang_rtc_clock_stop(&clock, 1, 86402000000LL);
    hpclang_rtc_clock_write(&clock, 6, 0x25, 86403000000LL);
    hpclang_rtc_clock_write(&clock, 5, 0x12, 86403000000LL);
    hpclang_rtc_clock_write(&clock, 3, 0x31, 86403000000LL);
    hpclang_rtc_clock_write(&clock, 2, 0x23, 86403000000LL);
    hpclang_rtc_clock_write(&clock, 1, 0x59, 86403000000LL);
    hpclang_rtc_clock_write(&clock, 0, 0x59, 86403000000LL);
    hpclang_rtc_clock_read(&clock, 86410000000LL, raw);
    assert(raw[0] == 0x59 && raw[3] == 0x31 && raw[6] == 0x25);
    hpclang_rtc_clock_stop(&clock, 0, 86410000000LL);
    hpclang_rtc_clock_read(&clock, 86410999999LL, raw);
    assert(raw[0] == 0x59);
    hpclang_rtc_clock_read(&clock, 86411000000LL, raw);
    assert(raw[0] == 0 && raw[3] == 1 && raw[5] == 1 && raw[6] == 0x26);

    hpclang_rtc_clock_write(&clock, 0, 0x7a, 86411000000LL);
    assert(!clock.valid);
    hpclang_rtc_clock_read(&clock, 86412000000LL, raw);
    assert(raw[0] == 0x7a);
    hpclang_rtc_clock_write(&clock, 0, 0x10, 86412000000LL);
    assert(clock.valid);
    hpclang_rtc_clock_read(&clock, 86413000000LL, raw);
    assert(raw[0] == 0x11);
    return 0;
}
