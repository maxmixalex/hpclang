#include <errno.h>
#include "rtc-tick.h"

#define TICK_IRQ UINT32_C(0x100)

static int valid_selector(unsigned selector)
{
    if (selector && selector <= 127u)
        return 1;
    errno = EINVAL;
    return 0;
}

int hpclang_rtc_tick_acquire(const struct hpclang_rtc_tick_bus *bus,
                             struct hpclang_rtc_tick_saved *saved, unsigned selector)
{
    if (!valid_selector(selector))
        return -1;
    if (saved->owned) {
        errno = EBUSY;
        return -1;
    }
    if ((bus->read(bus->context, RTC_TICK_MODE) & TICK_IRQ) ||
        (bus->read(bus->context, RTC_TICK_CONTROL) & 0x0eu)) {
        errno = ENOTSUP;
        return -1;
    }
    uint32_t source = bus->read(bus->context, RTC_TICK_SOURCE);
    if (!(source & TICK_IRQ) && (bus->read(bus->context, RTC_TICK_PENDING) & TICK_IRQ)) {
        errno = EBUSY;
        return -1;
    }
    uint32_t mask = bus->read(bus->context, RTC_TICK_MASK);
    *saved = (struct hpclang_rtc_tick_saved){bus->read(bus->context, RTC_TICK_COUNT),
                                             mask & TICK_IRQ, 1, !!(source & TICK_IRQ)};
    bus->write(bus->context, RTC_TICK_MASK, mask | TICK_IRQ);
    bus->write(bus->context, RTC_TICK_COUNT, selector | 0x80u);
    return 0;
}

int hpclang_rtc_tick_set(const struct hpclang_rtc_tick_bus *bus,
                         const struct hpclang_rtc_tick_saved *saved, unsigned selector)
{
    if (!valid_selector(selector))
        return -1;
    if (!saved->owned) {
        errno = EINVAL;
        return -1;
    }
    bus->write(bus->context, RTC_TICK_COUNT, selector | 0x80u);
    return 0;
}

int hpclang_rtc_tick_poll(const struct hpclang_rtc_tick_bus *bus,
                          struct hpclang_rtc_tick_saved *saved)
{
    if (!saved->owned) {
        errno = EINVAL;
        return -1;
    }
    if (!(bus->read(bus->context, RTC_TICK_SOURCE) & TICK_IRQ))
        return 0;
    bus->write(bus->context, RTC_TICK_SOURCE, TICK_IRQ);
    bus->write(bus->context, RTC_TICK_PENDING, TICK_IRQ);
    saved->preserve_pending = 0;
    return 1;
}

void hpclang_rtc_tick_release(const struct hpclang_rtc_tick_bus *bus,
                              struct hpclang_rtc_tick_saved *saved)
{
    if (!saved->owned)
        return;
    bus->write(bus->context, RTC_TICK_COUNT, bus->read(bus->context, RTC_TICK_COUNT) & 0x7fu);
    if (!saved->preserve_pending) {
        bus->write(bus->context, RTC_TICK_SOURCE, TICK_IRQ);
        bus->write(bus->context, RTC_TICK_PENDING, TICK_IRQ);
    }
    bus->write(bus->context, RTC_TICK_COUNT, saved->count);
    uint32_t mask = bus->read(bus->context, RTC_TICK_MASK);
    bus->write(bus->context, RTC_TICK_MASK, (mask & ~TICK_IRQ) | saved->mask);
    saved->owned = 0;
}
