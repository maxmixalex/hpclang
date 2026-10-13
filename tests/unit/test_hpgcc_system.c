#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <hpsys.h>
#include <hpclang/datetime.h>
#include <hpclang/time.h>

static uint64_t delay;
static int rtc_error;
static struct hpclang_datetime civil = {2026, 10, 14, 23, 59, 58};

void hpclang_delay_us(uint64_t microseconds)
{
    delay = microseconds;
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
    sys_sleep(0);
    assert(delay == 0);
    sys_sleep(1);
    assert(delay == 1000000);
    sys_sleep(UINT_MAX);
    assert(delay == (uint64_t)UINT_MAX * 1000000);
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
