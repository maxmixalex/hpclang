#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <hpsys.h>
#include <hptime.h>
#include <hpclang/datetime.h>
#include <hpclang/time.h>
#include <hpclang/sound.h>

static uint64_t delay;
static int rtc_error;
static uint64_t monotonic;
static int clock_error;
static uint32_t tone_frequency, tone_duration;
static int tone_error;
static unsigned tone_calls;
static struct hpclang_datetime civil = {2026, 10, 14, 23, 59, 58};

int hpclang_sound_play(uint32_t frequency, uint32_t duration, hpclang_sound_cancel_fn cancel,
                       void *context)
{
    assert(!cancel && !context);
    ++tone_calls;
    tone_frequency = frequency;
    tone_duration = duration;
    if (tone_error) {
        errno = tone_error;
        return -1;
    }
    return 0;
}

static void tones_and_bcd(void)
{
    sys_playTone2(1234, 17);
    assert(tone_frequency == 1234 && tone_duration == 17);
    sys_playTone2(UINT_MAX, UINT_MAX);
    assert(tone_frequency == UINT_MAX && tone_duration == UINT_MAX);
    sys_playTone2(0, 0);
    assert(!tone_frequency && !tone_duration);
    const int errors[] = {EINVAL, ERANGE, EBUSY, EIO, ENOSYS, EOVERFLOW};
    for (unsigned i = 0; i < sizeof errors / sizeof errors[0]; ++i) {
        tone_error = errors[i];
        sys_playTone2(440, 3);
        assert(errno == errors[i] && tone_frequency == 440 && tone_duration == 3);
    }
    tone_error = 0;
    sys_playTone(440, 3);
    assert(tone_frequency == 440 && tone_duration == 30);
    sys_playTone(1000, UINT32_MAX / 10u);
    assert(tone_duration == UINT32_MAX - UINT32_MAX % 10u);
    unsigned calls = tone_calls;
    sys_playTone(1000, UINT32_MAX / 10u + 1u);
    assert(errno == ERANGE && tone_calls == calls);
    sys_playTone(1000, UINT_MAX);
    assert(errno == ERANGE && tone_calls == calls);
    tone_error = EBUSY;
    sys_playTone(440, 1);
    assert(errno == EBUSY && tone_duration == 10);
    tone_error = 0;
    unsigned expected = 0, place = 1;
    for (int width = 0; width <= 8; ++width) {
        assert(sys_bcd2bin_nibbles(0x12345678u, width) == expected);
        assert(sys_bcd2bin_nibbles(0x12345678u, -width) == expected);
        if (width < 8) {
            expected += (8u - (unsigned)width) * place;
            place *= 10u;
        }
    }
    assert(sys_bcd2bin_nibbles(0x12345678u, INT_MIN) == 12345678);
    assert(sys_bcd2bin_nibbles(0x12345678u, INT_MAX) == 12345678);
    assert(sys_bcd2bin_nibbles(0xffffffffu, 8) == 166666665);
    unsigned packed = 0x12345678u;
    assert(sys_bcd2bin(packed++) == 78 && packed == 0x12345679u);
    unsigned int (*convert_byte)(unsigned int) = sys_bcd2bin;
    assert(convert_byte(0x12345678u) == 78);
    assert(!strcmp(sys_bcd2str(0), "0"));
    assert(!strcmp(sys_bcd2str(0x00123450u), "123450"));
    assert(!strcmp(sys_bcd2str(0x99999999u), "99999999"));
    assert(!strcmp(sys_bcd2str(0xabcdefu), ":;<=>?"));
}

void hpclang_delay_us(uint64_t microseconds)
{
    delay = microseconds;
}

int hpclang_time_read_us(uint64_t *value)
{
    if (clock_error) {
        errno = clock_error;
        return -1;
    }
    *value = monotonic;
    return 0;
}

static void timers(void)
{
    assert(sys_updateTimer(NULL) == 0 && errno == EINVAL);
    for (unsigned index = 0; index < 5; ++index) {
        sys_timer_t timer = SYS_TIMER_INITIALIZER_NUM(index);
        monotonic = 123;
        errno = EDOM;
        assert(sys_updateTimer(&timer) == 0 && timer.inited && timer.current == 0);
        assert(errno == EDOM);
        monotonic += 17;
        assert(sys_updateTimer(&timer) == 17000 && timer.current == 17000);
        assert(sys_updateTimer(&timer) == 0 && timer.current == 17000);
        monotonic += 100000000;
        assert(sys_updateTimer(&timer) == UINT64_C(100000000000));
        assert(timer.current == UINT64_C(100000017000));
        sys_timer_t saved = timer;
        clock_error = EIO;
        assert(sys_updateTimer(&timer) == 0 && errno == EIO);
        assert(memcmp(&saved, &timer, sizeof(timer)) == 0);
        clock_error = 0;
        --monotonic;
        assert(sys_updateTimer(&timer) == 0 && errno == EIO);
        assert(memcmp(&saved, &timer, sizeof(timer)) == 0);
        timer.inited = 0;
        assert(sys_updateTimer(&timer) == 0 && timer.current == 0);
        timer.current = UINT64_MAX - 999;
        saved = timer;
        ++monotonic;
        assert(sys_updateTimer(&timer) == 0 && errno == ERANGE);
        assert(memcmp(&saved, &timer, sizeof(timer)) == 0);
    }
    sys_timer_t invalid = SYS_TIMER_INITIALIZER_NUM(5);
    assert(sys_updateTimer(&invalid) == 0 && errno == EINVAL && !invalid.inited);
    sys_timer_t large = SYS_TIMER_INITIALIZER;
    monotonic = 0;
    assert(sys_updateTimer(&large) == 0);
    monotonic = UINT64_MAX;
    assert(sys_updateTimer(&large) == 0 && errno == ERANGE && large.current == 0);
}

int hpclang_datetime_read(struct hpclang_datetime *value)
{
    if (rtc_error) {
        errno = rtc_error;
        return -1;
    }
    *value = civil;
    return 0;
}

int main(void)
{
    tones_and_bcd();
    struct tm date;
    civil = (struct hpclang_datetime){2000, 2, 29, 23, 59, 58};
    sys_tm_RTC(&date);
    assert(date.tm_year == 100 && date.tm_mon == 1 && date.tm_mday == 29);
    assert(date.tm_wday == 2 && date.tm_yday == 59 && date.tm_isdst == -1);
    assert(date.tm_hour == 23 && date.tm_min == 59 && date.tm_sec == 58);
    civil = (struct hpclang_datetime){2100, 3, 1, 0, 0, 0};
    sys_tm_RTC(&date);
    assert(date.tm_year == 200 && date.tm_yday == 59 && date.tm_wday == 1);
    struct tm saved = date;
    rtc_error = EIO;
    sys_tm_RTC(&date);
    assert(errno == EIO && memcmp(&saved, &date, sizeof(date)) == 0);
    rtc_error = 0;
    civil.day = 32;
    sys_tm_RTC(&date);
    assert(errno == EINVAL && memcmp(&saved, &date, sizeof(date)) == 0);
    sys_tm_RTC(NULL);
    assert(errno == EINVAL);
    civil = (struct hpclang_datetime){2026, 10, 14, 23, 59, 58};
    timers();
    sys_sleep(0);
    assert(delay == 0);
    sys_sleep(1);
    assert(delay == 1000);
    sys_sleep(1000);
    assert(delay == 1000000);
    sys_sleep(UINT_MAX);
    assert(delay == (uint64_t)UINT_MAX * 1000);
    sys_waitTicks(0);
    assert(delay == 0);
    sys_waitTicks(1);
    assert(delay == 2);
    sys_waitTicks(2);
    assert(delay == 3);
    sys_waitTicks(3);
    assert(delay == 4);
    sys_waitTicks(750000);
    assert(delay == 1000000);
    sys_waitTicks(UINT_MAX);
    assert(delay == ((uint64_t)UINT_MAX * 4 + 2) / 3);
    errno = EDOM;
    assert(sys_RTC_seconds() == 86398 && errno == EDOM);
    civil.hour = civil.minute = civil.second = 0;
    assert(sys_RTC_seconds() == 0);
    rtc_error = EIO;
    assert(sys_RTC_seconds() == -1 && errno == EIO);
    rtc_error = ENOSYS;
    assert(sys_RTC_seconds() == -1 && errno == ENOSYS);
    assert(sys_delta_seconds(86398, 2) == 4);
    assert(sys_delta_seconds(12, 12) == 0);
    assert(sys_delta_seconds(0, 86399) == 86399);
    assert(sys_delta_seconds(86399, 0) == 1);
    const int invalid[] = {-1, 86400, INT_MIN, INT_MAX};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(sys_delta_seconds(invalid[i], 0) == -1 && errno == EINVAL);
        assert(sys_delta_seconds(0, invalid[i]) == -1 && errno == EINVAL);
    }
    return 0;
}
