#ifndef HPCLANG_CLOCK_INTERNAL_H
#define HPCLANG_CLOCK_INTERNAL_H
#include <stdint.h>
void hpclang_clock_pclk(uint64_t *numerator, uint64_t *denominator);
struct hpclang_clock_epoch
{
    uint64_t microseconds;
    uint64_t rtc;
    uint16_t count;
};
int hpclang_time_clock_prepare(struct hpclang_clock_epoch *epoch);
void hpclang_time_clock_commit(const struct hpclang_clock_epoch *epoch);
int hpclang_sound_active(void);
void hpclang_sound_restore(void);
void hpclang_power_restore(void);
#endif
