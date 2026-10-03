#include <errno.h>
#include "sound-engine.h"

#define CONTROL_MASK UINT32_C(0xf000)
#define MUX_MASK UINT32_C(0xf00)
#define PIN_MASK UINT32_C(0x30)
#define TIMER_IRQ UINT32_C(0x1000)

static uint32_t read_reg(const struct hpclang_sound_bus *bus, enum hpclang_sound_reg reg)
{
    return bus->read(bus->context, reg);
}
static void write_reg(const struct hpclang_sound_bus *bus, enum hpclang_sound_reg reg,
                      uint32_t value)
{
    bus->write(bus->context, reg, value);
}
static void replace(const struct hpclang_sound_bus *bus, enum hpclang_sound_reg reg, uint32_t mask,
                    uint32_t value)
{
    write_reg(bus, reg, (read_reg(bus, reg) & ~mask) | (value & mask));
}

int hpclang_sound_acquire(const struct hpclang_sound_bus *bus, struct hpclang_sound_saved *saved,
                          uint64_t clock_num, uint64_t clock_den, uint32_t frequency)
{
    if (!frequency || !clock_num || !clock_den) {
        errno = EINVAL;
        return -1;
    }
    uint32_t control = read_reg(bus, SOUND_TCON);
    /* Never take a running/updating channel or a pending Stock interrupt. */
    if (saved->owned || (control & UINT32_C(0x3000)) ||
        ((read_reg(bus, SOUND_TCFG1) >> 20) & 15u) == 3u ||
        (!(read_reg(bus, SOUND_INTMASK) & TIMER_IRQ) &&
         ((read_reg(bus, SOUND_SOURCE_PENDING) | read_reg(bus, SOUND_PENDING)) & TIMER_IRQ))) {
        errno = EBUSY;
        return -1;
    }
    uint64_t prescaler = ((read_reg(bus, SOUND_TCFG0) >> 8) & 255) + 1;
    uint32_t mux;
    uint64_t count = 0;
    for (mux = 0; mux < 4; ++mux) {
        uint64_t divisor = prescaler * (2u << mux);
        if (clock_den > UINT64_MAX / divisor / frequency) {
            errno = ERANGE;
            return -1;
        }
        uint64_t denominator = clock_den * divisor * frequency;
        count = clock_num / denominator;
        if (clock_num % denominator >= denominator / 2 + denominator % 2)
            ++count;
        if (count >= 2 && count <= 65536)
            break;
    }
    if (mux == 4) {
        errno = ERANGE;
        return -1;
    }
    *saved = (struct hpclang_sound_saved){read_reg(bus, SOUND_TCFG1) & MUX_MASK,
                                          control & CONTROL_MASK,
                                          read_reg(bus, SOUND_COUNT),
                                          read_reg(bus, SOUND_COMPARE),
                                          read_reg(bus, SOUND_CURRENT),
                                          read_reg(bus, SOUND_PIN) & PIN_MASK,
                                          read_reg(bus, SOUND_SOURCE_PENDING) & TIMER_IRQ,
                                          read_reg(bus, SOUND_PENDING) & TIMER_IRQ,
                                          read_reg(bus, SOUND_INTMASK) & TIMER_IRQ,
                                          1};
    replace(bus, SOUND_INTMASK, TIMER_IRQ, TIMER_IRQ);
    replace(bus, SOUND_TCFG1, MUX_MASK, mux << 8);
    write_reg(bus, SOUND_COUNT, (uint32_t)count - 1);
    write_reg(bus, SOUND_COMPARE, (uint32_t)(count / 2) - 1);
    replace(bus, SOUND_TCON, CONTROL_MASK, UINT32_C(0x2000));
    replace(bus, SOUND_TCON, CONTROL_MASK, UINT32_C(0x8000));
    replace(bus, SOUND_PIN, PIN_MASK, UINT32_C(0x20)); /* GPB2 = TOUT2 */
    replace(bus, SOUND_TCON, CONTROL_MASK, UINT32_C(0x9000));
    return 0;
}

void hpclang_sound_release(const struct hpclang_sound_bus *bus, struct hpclang_sound_saved *saved)
{
    if (!saved->owned)
        return;
    replace(bus, SOUND_TCON, CONTROL_MASK, 0);
    replace(bus, SOUND_PIN, PIN_MASK, saved->pin);
    /* Restore the stopped counter, then its separate reload buffers. */
    write_reg(bus, SOUND_COUNT, saved->current);
    write_reg(bus, SOUND_COMPARE, saved->compare);
    replace(bus, SOUND_TCON, CONTROL_MASK, UINT32_C(0x2000));
    replace(bus, SOUND_TCON, CONTROL_MASK, saved->control);
    write_reg(bus, SOUND_COUNT, saved->count);
    replace(bus, SOUND_TCFG1, MUX_MASK, saved->mux);
    if (!saved->source_pending)
        write_reg(bus, SOUND_SOURCE_PENDING, TIMER_IRQ);
    if (!saved->pending)
        write_reg(bus, SOUND_PENDING, TIMER_IRQ);
    replace(bus, SOUND_INTMASK, TIMER_IRQ, saved->intmask);
    saved->owned = 0;
}
