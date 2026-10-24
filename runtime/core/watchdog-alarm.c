#include "watchdog-alarm.h"
#include <errno.h>

#define IRQ UINT32_C(0x200)
#define ENABLE UINT32_C(0x20)
#define FIELDS UINT32_C(0xff3d)
#define IRQ_ONLY_DIV128 UINT32_C(0x1c)

static int valid(const struct hpclang_watchdog_bus *bus, const struct hpclang_watchdog_alarm *alarm)
{
    return bus && bus->read && bus->write && alarm;
}
static uint32_t read_reg(const struct hpclang_watchdog_bus *bus, enum hpclang_watchdog_reg reg)
{
    return bus->read(bus->context, reg);
}
static void write_reg(const struct hpclang_watchdog_bus *bus, enum hpclang_watchdog_reg reg,
                      uint32_t value)
{
    bus->write(bus->context, reg, value);
}
static void mask(const struct hpclang_watchdog_bus *bus, uint32_t bit)
{
    write_reg(bus, WDT_MASK, (read_reg(bus, WDT_MASK) & ~IRQ) | (bit & IRQ));
}
static int plan(uint64_t num, uint64_t den, uint64_t now, uint64_t deadline, uint32_t *count)
{
    if (!num || !den || deadline <= now) {
        errno = EINVAL;
        return -1;
    }
    const uint64_t factor = UINT64_C(128000000);
    if (den > UINT64_MAX / factor / 65535) {
        errno = ERANGE;
        return -1;
    }
    uint64_t unit = den * factor;
    uint64_t delay = deadline - now;
    uint64_t ticks = 65535;
    /* An overflowing product is greater than 65535*unit, checked above. */
    if (num <= UINT64_MAX / delay) {
        uint64_t product = num * delay;
        ticks = product / unit + (product % unit != 0);
    }
    *count = (uint32_t)(ticks > 65535 ? 65535 : ticks);
    return 0;
}
static void arm(const struct hpclang_watchdog_bus *bus, const struct hpclang_watchdog_alarm *alarm,
                uint32_t count)
{
    uint32_t old_mask = read_reg(bus, WDT_MASK);
    uint32_t control = (alarm->control & ~FIELDS) | IRQ_ONLY_DIV128;
    mask(bus, IRQ);
    write_reg(bus, WDT_CONTROL, control);
    write_reg(bus, WDT_SOURCE, IRQ);
    write_reg(bus, WDT_PENDING, IRQ);
    write_reg(bus, WDT_RELOAD, count);
    write_reg(bus, WDT_COUNT, count);
    write_reg(bus, WDT_CONTROL, control | ENABLE);
    mask(bus, old_mask);
}
int hpclang_watchdog_acquire(const struct hpclang_watchdog_bus *bus,
                             struct hpclang_watchdog_alarm *alarm, uint64_t num, uint64_t den,
                             uint64_t now, uint64_t deadline)
{
    if (!valid(bus, alarm)) {
        errno = EINVAL;
        return -1;
    }
    uint32_t control = read_reg(bus, WDT_CONTROL);
    if (alarm->owned || (control & ENABLE) ||
        ((read_reg(bus, WDT_SOURCE) | read_reg(bus, WDT_PENDING)) & IRQ)) {
        errno = EBUSY;
        return -1;
    }
    uint32_t count;
    if (plan(num, den, now, deadline, &count))
        return -1;
    *alarm = (struct hpclang_watchdog_alarm){control,
                                             read_reg(bus, WDT_RELOAD),
                                             read_reg(bus, WDT_COUNT),
                                             read_reg(bus, WDT_MASK) & IRQ,
                                             num,
                                             den,
                                             1};
    mask(bus, IRQ);
    arm(bus, alarm, count);
    return 0;
}
void hpclang_watchdog_release(const struct hpclang_watchdog_bus *bus,
                              struct hpclang_watchdog_alarm *alarm)
{
    if (!valid(bus, alarm) || !alarm->owned)
        return;
    mask(bus, IRQ);
    write_reg(bus, WDT_CONTROL, (alarm->control & ~FIELDS) | IRQ_ONLY_DIV128);
    write_reg(bus, WDT_RELOAD, alarm->reload);
    write_reg(bus, WDT_COUNT, alarm->count);
    write_reg(bus, WDT_CONTROL, alarm->control);
    write_reg(bus, WDT_SOURCE, IRQ);
    write_reg(bus, WDT_PENDING, IRQ);
    mask(bus, alarm->mask);
    alarm->owned = 0;
}
int hpclang_watchdog_advance(const struct hpclang_watchdog_bus *bus,
                             struct hpclang_watchdog_alarm *alarm, uint64_t now, uint64_t deadline)
{
    if (!valid(bus, alarm) || !alarm->owned) {
        errno = EINVAL;
        return -1;
    }
    if (now >= deadline) {
        hpclang_watchdog_release(bus, alarm);
        return 1;
    }
    uint32_t count;
    if (plan(alarm->clock_num, alarm->clock_den, now, deadline, &count))
        return -1;
    arm(bus, alarm, count);
    return 0;
}
