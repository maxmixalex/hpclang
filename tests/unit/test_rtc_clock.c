#include <assert.h>
#include "rtc-clock.h"

int main(void)
{
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
