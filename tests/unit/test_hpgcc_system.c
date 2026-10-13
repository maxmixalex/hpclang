#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <hpsys.h>
#include <hpclang/datetime.h>
#include <hpclang/time.h>

static uint64_t delay;
static int rtc_error;
static uint64_t monotonic;
static int clock_error;
static struct hpclang_datetime civil = {2026, 10, 14, 23, 59, 58};

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
