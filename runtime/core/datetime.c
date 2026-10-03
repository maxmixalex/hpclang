#include <errno.h>
#include <hpclang/datetime.h>
#include "datetime-internal.h"

#define EPOCH_DAYS INT64_C(730119)
#define DAY_SECONDS INT64_C(86400)

static int64_t days_before_year(uint32_t year)
{
    int64_t y = (int64_t)year - 1;
    return y * 365 + y / 4 - y / 100 + y / 400;
}

static unsigned month_days(uint32_t year, uint32_t month)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int hpclang_datetime_valid(const struct hpclang_datetime *v)
{
    return v && v->year >= 1 && v->year <= 9999 && v->month >= 1 && v->month <= 12 && v->day >= 1 &&
           v->day <= month_days(v->year, v->month) && v->hour <= 23 && v->minute <= 59 &&
           v->second <= 59;
}

int hpclang_datetime_to_seconds(const struct hpclang_datetime *v, int64_t *seconds)
{
    if (!seconds || !hpclang_datetime_valid(v)) {
        errno = EINVAL;
        return -1;
    }
    int64_t days = days_before_year(v->year) - EPOCH_DAYS + v->day - 1;
    for (unsigned m = 1; m < v->month; ++m)
        days += month_days(v->year, m);
    *seconds = days * DAY_SECONDS + v->hour * 3600 + v->minute * 60 + v->second;
    return 0;
}

int hpclang_datetime_from_seconds(int64_t seconds, struct hpclang_datetime *value)
{
    if (!value) {
        errno = EINVAL;
        return -1;
    }
    if (seconds < -EPOCH_DAYS * DAY_SECONDS ||
        seconds >= (days_before_year(10000) - EPOCH_DAYS) * DAY_SECONDS) {
        errno = ERANGE;
        return -1;
    }
    int64_t days = seconds / DAY_SECONDS;
    int64_t remainder = seconds % DAY_SECONDS;
    if (remainder < 0) {
        remainder += DAY_SECONDS;
        --days;
    }
    days += EPOCH_DAYS;
    uint32_t lo = 1, hi = 10000;
    while (lo + 1 < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (days_before_year(mid) <= days)
            lo = mid;
        else
            hi = mid;
    }
    days -= days_before_year(lo);
    struct hpclang_datetime result = {lo, 1, 0, 0, 0, 0};
    while (days >= month_days(lo, result.month)) {
        days -= month_days(lo, result.month);
        ++result.month;
    }
    result.day = (uint32_t)days + 1;
    result.hour = (uint32_t)(remainder / 3600);
    result.minute = (uint32_t)(remainder / 60 % 60);
    result.second = (uint32_t)(remainder % 60);
    *value = result;
    return 0;
}

int hpclang_datetime_weekday(const struct hpclang_datetime *value, unsigned *weekday)
{
    int64_t seconds;
    if (!weekday) {
        errno = EINVAL;
        return -1;
    }
    if (hpclang_datetime_to_seconds(value, &seconds) < 0)
        return -1;
    int64_t days = seconds / DAY_SECONDS;
    if (seconds % DAY_SECONDS < 0)
        --days;
    int64_t result = (days + 6) % 7; /* 2000-01-01 was Saturday. */
    *weekday = (unsigned)(result < 0 ? result + 7 : result);
    return 0;
}
