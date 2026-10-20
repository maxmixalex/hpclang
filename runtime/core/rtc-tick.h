#ifndef HPCLANG_RTC_TICK_H
#define HPCLANG_RTC_TICK_H

#include <stdint.h>

enum hpclang_rtc_tick_reg
{
    RTC_TICK_CONTROL,
    RTC_TICK_COUNT,
    RTC_TICK_MODE,
    RTC_TICK_MASK,
    RTC_TICK_SOURCE,
    RTC_TICK_PENDING
};

struct hpclang_rtc_tick_bus
{
    void *context;
    uint32_t (*read)(void *, enum hpclang_rtc_tick_reg);
    void (*write)(void *, enum hpclang_rtc_tick_reg, uint32_t);
};

struct hpclang_rtc_tick_saved
{
    uint32_t count, mask;
    int owned, preserve_pending;
};

extern const struct hpclang_rtc_tick_bus hpclang_rtc_tick_bus;

/* Internal polling lease for the physical RTC tick latch. The caller excludes
 * IRQs across each operation and owns the bus/state. No calendar/PWM registers
 * are changed. selector 1..127 means (selector+1)/128 seconds. FIQ routing and
 * RTC test/reset modes are rejected before writes. Changing the divider starts
 * a new hardware phase; the prior sub-period phase cannot be restored.
 * Poll consumes one pending indication, including a pre-acquisition indication;
 * elapsed ticks coalesce. Release restores divider/enable and this source's
 * interrupt mask, preserving an initial indication only if never consumed.
 * Consumed events are not manufactured again on exit. Unrelated IRQs are kept.
 * This engine does not register cleanup or expose the historical API itself. */
int hpclang_rtc_tick_acquire(const struct hpclang_rtc_tick_bus *bus,
                             struct hpclang_rtc_tick_saved *saved, unsigned selector);
int hpclang_rtc_tick_set(const struct hpclang_rtc_tick_bus *bus,
                         const struct hpclang_rtc_tick_saved *saved, unsigned selector);
int hpclang_rtc_tick_poll(const struct hpclang_rtc_tick_bus *bus,
                          struct hpclang_rtc_tick_saved *saved);
void hpclang_rtc_tick_release(const struct hpclang_rtc_tick_bus *bus,
                              struct hpclang_rtc_tick_saved *saved);

#endif
