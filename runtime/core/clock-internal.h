#ifndef HPCLANG_CLOCK_INTERNAL_H
#define HPCLANG_CLOCK_INTERNAL_H
#include <stdint.h>
void hpclang_clock_pclk(uint64_t *numerator, uint64_t *denominator);
/* Internal IRQ path: caller already masks IRQ/FIQ and preserves errno. Does
 * not enter the supervisor bridge. Foreground clock reads, initialization and
 * civil-time writes serialize against this operation. No user callbacks. */
int hpclang_time_read_us_masked(uint64_t *microseconds);
struct hpclang_clock_epoch
{
    uint64_t microseconds;
    uint64_t rtc;
    uint16_t count;
};
/* Caller keeps IRQ/FIQ masked across prepare, hardware transition and commit. */
int hpclang_time_clock_prepare(struct hpclang_clock_epoch *epoch);
void hpclang_time_clock_commit(const struct hpclang_clock_epoch *epoch);
int hpclang_sound_active(void);
void hpclang_sound_restore(void);
void hpclang_power_restore(void);
#endif
