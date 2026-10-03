#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <hpclang/datetime.h>
#include "datetime-internal.h"

static void vector(struct hpclang_datetime value, int64_t seconds, unsigned weekday)
{
    struct hpclang_datetime back;
    int64_t result = INT64_MIN;
    unsigned day = 99;
    assert(hpclang_datetime_to_seconds(&value, &result) == 0 && result == seconds);
    assert(hpclang_datetime_from_seconds(seconds, &back) == 0);
    assert(memcmp(&value, &back, sizeof(value)) == 0);
    assert(hpclang_datetime_weekday(&value, &day) == 0 && day == weekday);
}

int main(void)
{
    vector((struct hpclang_datetime){2000, 1, 1, 0, 0, 0}, 0, 6);
    vector((struct hpclang_datetime){1999, 12, 31, 23, 59, 59}, -1, 5);
    vector((struct hpclang_datetime){1970, 1, 1, 0, 0, 0}, -INT64_C(946684800), 4);
    vector((struct hpclang_datetime){2000, 2, 29, 0, 0, 0}, INT64_C(5097600), 2);
    vector((struct hpclang_datetime){1, 1, 1, 0, 0, 0}, -INT64_C(63082281600), 1);
    vector((struct hpclang_datetime){9999, 12, 31, 23, 59, 59}, INT64_C(252455615999), 5);

    /* Independently step each civil day, including Gregorian century rules. */
    static const unsigned lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int64_t expected = -INT64_C(63082281600);
    for (unsigned year = 1; year <= 9999; ++year) {
        for (unsigned month = 1; month <= 12; ++month) {
            unsigned days = lengths[month - 1];
            if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
                ++days;
            for (unsigned day = 1; day <= days; ++day) {
                struct hpclang_datetime date = {year, month, day, 12, 34, 56}, back;
                int64_t seconds;
                assert(hpclang_datetime_to_seconds(&date, &seconds) == 0);
                assert(seconds == expected + 45296);
                assert(hpclang_datetime_from_seconds(seconds, &back) == 0);
                assert(memcmp(&date, &back, sizeof(date)) == 0);
                expected += 86400;
            }
        }
    }

    struct hpclang_datetime sentinel = {2026, 10, 3, 12, 34, 56}, out = sentinel;
    int64_t seconds = 123;
    struct hpclang_datetime invalid[] = {
        {0, 1, 1, 0, 0, 0},     {10000, 1, 1, 0, 0, 0}, {2000, 0, 1, 0, 0, 0},
        {2000, 13, 1, 0, 0, 0}, {2000, 1, 0, 0, 0, 0},  {2001, 2, 29, 0, 0, 0},
        {2100, 2, 29, 0, 0, 0}, {2000, 4, 31, 0, 0, 0}, {2000, 1, 1, 24, 0, 0},
        {2000, 1, 1, 0, 60, 0}, {2000, 1, 1, 0, 0, 60}, {UINT32_MAX, 1, 1, 0, 0, 0}};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        errno = 0;
        assert(hpclang_datetime_to_seconds(&invalid[i], &seconds) == -1);
        assert(errno == EINVAL && seconds == 123);
    }
    int64_t outside[] = {INT64_MIN, INT64_MAX, -INT64_C(63082281601), INT64_C(252455616000)};
    for (unsigned i = 0; i < sizeof(outside) / sizeof(outside[0]); ++i) {
        assert(hpclang_datetime_from_seconds(outside[i], &out) == -1);
        assert(errno == ERANGE && memcmp(&out, &sentinel, sizeof(out)) == 0);
    }
    assert(hpclang_datetime_to_seconds(NULL, &seconds) == -1 && errno == EINVAL);
    assert(hpclang_datetime_to_seconds(&sentinel, NULL) == -1 && errno == EINVAL);
    assert(hpclang_datetime_from_seconds(0, NULL) == -1 && errno == EINVAL);
    assert(hpclang_datetime_weekday(&sentinel, NULL) == -1 && errno == EINVAL);

    uint8_t raw[] = {0x56, 0x26, 0x10, 0x03, 0x12, 0x34, 0x56};
    assert(hpclang_rtc_decode(raw, &out));
    assert(memcmp(&out, &sentinel, sizeof(out)) == 0);
    for (unsigned i = 0; i < 7; ++i) {
        uint8_t saved = raw[i];
        raw[i] = 0xff;
        assert(!hpclang_rtc_decode(raw, &out));
        assert(memcmp(&out, &sentinel, sizeof(out)) == 0);
        raw[i] = saved;
    }
    raw[6] = 0x57;
    assert(!hpclang_rtc_decode(raw, &out));
    raw[6] = raw[0];
    raw[2] = 0x02;
    raw[3] = 0x31;
    assert(!hpclang_rtc_decode(raw, &out));
    assert(memcmp(&out, &sentinel, sizeof(out)) == 0);
    puts("PASS civil calendar, conversion bounds, RTC BCD and rollover");
    return 0;
}
