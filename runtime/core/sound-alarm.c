#include "sound-alarm.h"
#include <errno.h>

#define IRQ UINT32_C(0x4000)
#define CONTROL UINT32_C(0x700000)
#define MUX UINT32_C(0xf0000)

static uint32_t read_reg(const struct hpclang_sound_alarm_bus *bus,
                         enum hpclang_sound_alarm_reg reg)
{
    return bus->read(bus->context, reg);
}
static void write_reg(const struct hpclang_sound_alarm_bus *bus, enum hpclang_sound_alarm_reg reg,
                      uint32_t value)
{
    bus->write(bus->context, reg, value);
}
static void replace(const struct hpclang_sound_alarm_bus *bus, enum hpclang_sound_alarm_reg reg,
                    uint32_t mask, uint32_t value)
{
    write_reg(bus, reg, (read_reg(bus, reg) & ~mask) | (value & mask));
}
static int valid(const struct hpclang_sound_alarm_bus *bus, const struct hpclang_sound_alarm *alarm)
{
    return bus && bus->read && bus->write && alarm;
}

static int plan(const struct hpclang_sound_alarm_bus *bus, uint64_t num, uint64_t den,
                uint32_t delay, uint32_t *mux, uint32_t *reload)
{
    if (!num || !den || !delay) {
        errno = EINVAL;
        return -1;
    }
    uint64_t prescaler = ((read_reg(bus, ALARM_CONFIG) >> 8) & 255u) + 1;
    for (uint32_t divider = 0; divider < 4; ++divider) {
        uint64_t factor = prescaler * (2u << divider) * 1000000u;
        if (den > UINT64_MAX / factor / 65536u) {
            errno = ERANGE;
            return -1;
        }
        uint64_t denominator = den * factor;
        uint64_t ticks = 65537u;
        if (num <= UINT64_MAX / delay) {
            uint64_t product = num * delay;
            ticks = product / denominator + (product % denominator != 0);
        }
        /* An overflowing product exceeds 65536*denominator, checked above. */
        if (ticks <= 65536u || divider == 3) {
            *mux = divider;
            *reload = (uint32_t)(ticks > 65536u ? 65535u : ticks - 1);
            return 0;
        }
    }
    return -1;
}

static void arm(const struct hpclang_sound_alarm_bus *bus, uint32_t mux, uint32_t reload)
{
    uint32_t mask = read_reg(bus, ALARM_MASK) & IRQ;
    replace(bus, ALARM_MASK, IRQ, IRQ);
    replace(bus, ALARM_CONTROL, CONTROL, 0);
    write_reg(bus, ALARM_SOURCE, IRQ);
    write_reg(bus, ALARM_PENDING, IRQ);
    replace(bus, ALARM_MUX, MUX, mux << 16);
    write_reg(bus, ALARM_COUNT, reload);
    replace(bus, ALARM_CONTROL, CONTROL, 0x200000u);
    replace(bus, ALARM_CONTROL, CONTROL, 0);
    replace(bus, ALARM_CONTROL, CONTROL, 0x100000u);
    replace(bus, ALARM_MASK, IRQ, mask);
}

int hpclang_sound_alarm_acquire(const struct hpclang_sound_alarm_bus *bus,
                                struct hpclang_sound_alarm *alarm, uint64_t clock_num,
                                uint64_t clock_den, uint32_t delay_us)
{
    if (!valid(bus, alarm)) {
        errno = EINVAL;
        return -1;
    }
    uint32_t control = read_reg(bus, ALARM_CONTROL);
    if (alarm->owned || (control & 0x300000u) || ((read_reg(bus, ALARM_MUX) >> 20) & 15u) == 5u ||
        ((read_reg(bus, ALARM_SOURCE) | read_reg(bus, ALARM_PENDING)) & IRQ)) {
        errno = EBUSY;
        return -1;
    }
    uint32_t mux, reload;
    if (plan(bus, clock_num, clock_den, delay_us, &mux, &reload))
        return -1;
    *alarm = (struct hpclang_sound_alarm){read_reg(bus, ALARM_MUX) & MUX,
                                          control & CONTROL,
                                          read_reg(bus, ALARM_COUNT),
                                          read_reg(bus, ALARM_CURRENT),
                                          read_reg(bus, ALARM_MASK) & IRQ,
                                          clock_num,
                                          clock_den,
                                          1};
    replace(bus, ALARM_MASK, IRQ, IRQ);
    arm(bus, mux, reload);
    return 0;
}

int hpclang_sound_alarm_rearm(const struct hpclang_sound_alarm_bus *bus,
                              struct hpclang_sound_alarm *alarm, uint32_t delay_us)
{
    if (!valid(bus, alarm) || !alarm->owned) {
        errno = EINVAL;
        return -1;
    }
    uint32_t mux, reload;
    if (plan(bus, alarm->clock_num, alarm->clock_den, delay_us, &mux, &reload))
        return -1;
    arm(bus, mux, reload);
    return 0;
}

void hpclang_sound_alarm_release(const struct hpclang_sound_alarm_bus *bus,
                                 struct hpclang_sound_alarm *alarm)
{
    if (!valid(bus, alarm) || !alarm->owned)
        return;
    replace(bus, ALARM_MASK, IRQ, IRQ);
    replace(bus, ALARM_CONTROL, CONTROL, 0);
    write_reg(bus, ALARM_COUNT, alarm->current);
    replace(bus, ALARM_CONTROL, CONTROL, 0x200000u);
    replace(bus, ALARM_CONTROL, CONTROL, alarm->control);
    write_reg(bus, ALARM_COUNT, alarm->count);
    replace(bus, ALARM_MUX, MUX, alarm->mux);
    write_reg(bus, ALARM_SOURCE, IRQ);
    write_reg(bus, ALARM_PENDING, IRQ);
    replace(bus, ALARM_MASK, IRQ, alarm->mask);
    alarm->owned = 0;
}
