#ifndef HPCLANG_E2E_RTC_CLOCK_H
#define HPCLANG_E2E_RTC_CLOCK_H
#include <stdint.h>
#include <hpclang/datetime.h>

/* Calendar registers: second, minute, hour, date, weekday, month, year.
 * The emulator supplies elapsed microseconds; the calendar has no timezone. */
struct hpclang_rtc_clock
{
    int64_t seconds;
    int64_t anchor_us;
    uint8_t raw[7];
    uint8_t weekday;
    int valid;
    int stopped;
};
void hpclang_rtc_clock_init(struct hpclang_rtc_clock *clock, const struct hpclang_datetime *civil,
                            int64_t now_us);
void hpclang_rtc_clock_read(struct hpclang_rtc_clock *clock, int64_t now_us, uint8_t raw[7]);
void hpclang_rtc_clock_write(struct hpclang_rtc_clock *clock, unsigned index, uint8_t value,
                             int64_t now_us);
void hpclang_rtc_clock_stop(struct hpclang_rtc_clock *clock, int stopped, int64_t now_us);
/* Restore a committed calendar snapshot against this process's time origin.
 * phase_us is elapsed time within the saved second, never an old absolute timer
 * reading. Stopped clocks require phase zero. Reject invalid input unchanged. */
int hpclang_rtc_clock_resume(struct hpclang_rtc_clock *clock, uint32_t phase_us, int64_t now_us);
/* RTC tick divider: period=(selector+1)/128 seconds. Initialize remainder to
 * 127 when starting/reprogramming the divider to round deadlines upward.
 * Preserve it across expirations so fractional microseconds do not drift. */
uint32_t hpclang_rtc_tick_interval(uint8_t selector, unsigned *remainder);
#endif
