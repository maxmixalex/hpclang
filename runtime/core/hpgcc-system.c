#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <hpsys.h>
#include <hpclang/datetime.h>
#include <hpclang/time.h>

ULONGLONG sys_updateTimer(sys_timer_t *timer)
{
    if (timer == NULL || timer->timer > 4u) {
        errno = EINVAL;
        return 0;
    }
    uint64_t now;
    if (hpclang_time_read_us(&now) != 0)
        return 0;
    if (!timer->inited) {
        timer->last_val = now;
        timer->current = 0;
        timer->inited = 1;
        return 0;
    }
    if (now < timer->last_val) {
        errno = EIO;
        return 0;
    }
    uint64_t elapsed = now - timer->last_val;
    if (elapsed > (UINT64_MAX - timer->current) / 1000u) {
        errno = ERANGE;
        return 0;
    }
    elapsed *= 1000u;
    timer->current += elapsed;
    timer->last_val = now;
    return elapsed;
}

void sys_sleep(unsigned int millis)
{
    hpclang_delay_us((uint64_t)millis * UINT64_C(1000));
}

void sys_waitTicks(unsigned int ticks)
{
    hpclang_delay_us(((uint64_t)ticks * 4u + 2u) / 3u);
}

int sys_RTC_seconds(void)
{
    struct hpclang_datetime now;
    if (hpclang_datetime_read(&now) != 0)
        return -1;
    return (int)(now.hour * 3600u + now.minute * 60u + now.second);
}

int sys_delta_seconds(int start, int end)
{
    if (start < 0 || start >= 86400 || end < 0 || end >= 86400) {
        errno = EINVAL;
        return -1;
    }
    return (end + 86400 - start) % 86400;
}

void sys_tm_RTC(struct tm *value)
{
    if (!value) {
        errno = EINVAL;
        return;
    }
    struct hpclang_datetime now;
    if (hpclang_datetime_read(&now))
        return;
    unsigned weekday;
    int64_t seconds, first;
    const struct hpclang_datetime year_start = {now.year, 1, 1, 0, 0, 0};
    if (hpclang_datetime_weekday(&now, &weekday) || hpclang_datetime_to_seconds(&now, &seconds) ||
        hpclang_datetime_to_seconds(&year_start, &first))
        return;
    struct tm result = {0};
    result.tm_year = (int)now.year - 1900;
    result.tm_mon = (int)now.month - 1;
    result.tm_mday = (int)now.day;
    result.tm_hour = (int)now.hour;
    result.tm_min = (int)now.minute;
    result.tm_sec = (int)now.second;
    result.tm_wday = (int)weekday;
    result.tm_yday = (int)((seconds - first) / 86400);
    result.tm_isdst = -1;
    *value = result;
}
