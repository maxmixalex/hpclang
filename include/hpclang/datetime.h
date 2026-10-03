#ifndef HPCLANG_DATETIME_H
#define HPCLANG_DATETIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Gregorian civil time; no timezone, daylight-saving or leap-second policy. */
    struct hpclang_datetime
    {
        uint32_t year;   /* 1..9999 for conversion; RTC supplies 2000..2099. */
        uint32_t month;  /* 1..12 */
        uint32_t day;    /* 1..days in month */
        uint32_t hour;   /* 0..23 */
        uint32_t minute; /* 0..59 */
        uint32_t second; /* 0..59 */
    };

    /* Read the calculator's civil clock without changing RTC or alarm settings.
     * Returns 0, or -1 with EINVAL (NULL), ENOSYS (unknown device), or EIO
     * (invalid/unstable RTC). On failure the output is unchanged. */
    int hpclang_datetime_read(struct hpclang_datetime *value);

    /* Set the calculator's persistent civil clock (2000..2099). Existing
     * monotonic deadlines remain valid; RTC alarm configuration is unchanged,
     * but an alarm naturally observes the new date/time. No timezone policy.
     * Returns 0, or -1/EINVAL (invalid date), ERANGE (RTC year range), ENOSYS,
     * EBUSY (RTC test/reset mode), or EIO (clock unavailable/readback failure).
     * Invalid inputs and pre-write failures change nothing. A readback failure
     * may occur after the hardware clock has changed. Requires a working
     * monotonic clock; cannot repair an initially invalid RTC calendar.
     */
    int hpclang_datetime_set(const struct hpclang_datetime *value);

    /* Convert civil time to/from seconds relative to 2000-01-01 00:00:00 in
     * the same civil-time scale. These values are not UTC/Unix timestamps.
     * Returns 0, or -1 with EINVAL (NULL/invalid date) or ERANGE (seconds outside
     * years 1..9999). Output is unchanged on failure; no hardware is accessed. */
    int hpclang_datetime_to_seconds(const struct hpclang_datetime *value, int64_t *seconds);
    int hpclang_datetime_from_seconds(int64_t seconds, struct hpclang_datetime *value);

    /* Gregorian weekday, Sunday=0 through Saturday=6. Same EINVAL/output rules. */
    int hpclang_datetime_weekday(const struct hpclang_datetime *value, unsigned *weekday);

#ifdef __cplusplus
}
#endif
#endif
