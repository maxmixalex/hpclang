#include "sound-alarm.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

struct hardware
{
    uint32_t regs[8];
    unsigned writes;
};
static uint32_t read_reg(void *context, enum hpclang_sound_alarm_reg reg)
{
    return ((struct hardware *)context)->regs[reg];
}
static void write_reg(void *context, enum hpclang_sound_alarm_reg reg, uint32_t value)
{
    struct hardware *h = context;
    assert(reg != ALARM_CONFIG && reg != ALARM_CURRENT);
    ++h->writes;
    if (reg == ALARM_SOURCE || reg == ALARM_PENDING) {
        assert(value == 0x4000);
        h->regs[reg] &= ~value;
    } else {
        h->regs[reg] = value;
        if (reg == ALARM_CONTROL && (value & 0x200000))
            h->regs[ALARM_CURRENT] = h->regs[ALARM_COUNT];
    }
}
static struct hardware initial(void)
{
    return (struct hardware){{0x6300, 0x33333, 0x498909, 400, 222, 0xffffbfff, 0x2040, 0x40}, 0};
}
static void timing(void)
{
    const uint32_t delays[] = {1, 7, 100, 100000, 1000000, UINT32_MAX};
    const uint64_t clocks[][2] = {{50000000, 1}, {147456000, 7}, {1, 1}, {UINT64_MAX, 1}};
    for (unsigned prescaler = 0; prescaler < 256; ++prescaler) {
        for (unsigned c = 0; c < sizeof clocks / sizeof clocks[0]; ++c) {
            for (unsigned d = 0; d < sizeof delays / sizeof delays[0]; ++d) {
                struct hardware h = initial();
                h.regs[ALARM_CONFIG] = prescaler << 8;
                struct hardware before = h;
                struct hpclang_sound_alarm_bus bus = {&h, read_reg, write_reg};
                struct hpclang_sound_alarm alarm = {0};
                assert(!hpclang_sound_alarm_acquire(&bus, &alarm, clocks[c][0], clocks[c][1],
                                                    delays[d]));
                assert(alarm.owned && (h.regs[ALARM_MASK] & 0x4000));
                unsigned mux = (h.regs[ALARM_MUX] >> 16) & 15;
                uint64_t ticks = (uint64_t)h.regs[ALARM_COUNT] + 1;
                assert(mux <= 3 && ticks >= 1 && ticks <= 65536);
                __uint128_t required = (__uint128_t)clocks[c][0] * delays[d];
                __uint128_t unit =
                    (__uint128_t)clocks[c][1] * (prescaler + 1) * (2u << mux) * 1000000;
                if (required > 65536 * unit)
                    assert(mux == 3 && ticks == 65536);
                else
                    assert(ticks * unit >= required && (ticks - 1) * unit < required);
                assert((h.regs[ALARM_CONTROL] & 0x700000) == 0x100000);
                assert(h.regs[ALARM_CONFIG] == before.regs[ALARM_CONFIG]);
                hpclang_sound_alarm_release(&bus, &alarm);
                assert(!alarm.owned && !memcmp(h.regs, before.regs, sizeof h.regs));
            }
        }
    }
}
static void ownership(void)
{
    for (unsigned failure = 0; failure < 10; ++failure) {
        struct hardware h = initial();
        struct hpclang_sound_alarm_bus bus = {&h, read_reg, write_reg};
        struct hpclang_sound_alarm alarm = {0};
        uint64_t num = 50000000, den = 1;
        uint32_t delay = 100;
        switch (failure) {
        case 0:
            h.regs[ALARM_CONTROL] |= 0x100000;
            break;
        case 1:
            h.regs[ALARM_CONTROL] |= 0x200000;
            break;
        case 2:
            h.regs[ALARM_MUX] |= 5u << 20;
            break;
        case 3:
            alarm.owned = 1;
            break;
        case 4:
            h.regs[ALARM_SOURCE] |= 0x4000;
            break;
        case 5:
            h.regs[ALARM_PENDING] |= 0x4000;
            h.regs[ALARM_MASK] |= 0x4000;
            break;
        case 6:
            num = 0;
            break;
        case 7:
            den = 0;
            break;
        case 8:
            delay = 0;
            break;
        case 9:
            den = UINT64_MAX;
            break;
        }
        struct hardware before = h;
        assert(hpclang_sound_alarm_acquire(&bus, &alarm, num, den, delay) == -1);
        assert(errno == (failure < 6 ? EBUSY : failure == 9 ? ERANGE : EINVAL));
        assert(!h.writes && alarm.owned == (failure == 3) && !memcmp(&h, &before, sizeof h));
    }
    struct hardware h = initial();
    struct hardware before = h;
    struct hpclang_sound_alarm_bus bus = {&h, read_reg, write_reg};
    struct hpclang_sound_alarm alarm = {0};
    assert(!hpclang_sound_alarm_acquire(&bus, &alarm, 50000000, 1, 100000));
    unsigned writes = h.writes;
    assert(hpclang_sound_alarm_acquire(&bus, &alarm, 50000000, 1, 10) == -1);
    assert(errno == EBUSY && h.writes == writes);
    assert(hpclang_sound_alarm_rearm(&bus, &alarm, 0) == -1);
    assert(errno == EINVAL && h.writes == writes && alarm.owned);
    /* Simulate expiry and unrelated activity while the lease is held. */
    h.regs[ALARM_SOURCE] |= 0x4000;
    h.regs[ALARM_PENDING] |= 0x4000;
    h.regs[ALARM_MASK] &= ~0x4000u;
    h.regs[ALARM_MASK] ^= 0x100;
    before.regs[ALARM_MASK] ^= 0x100;
    h.regs[ALARM_CONTROL] ^= 0x400;
    before.regs[ALARM_CONTROL] ^= 0x400;
    h.regs[ALARM_MUX] ^= 0x400;
    before.regs[ALARM_MUX] ^= 0x400;
    assert(!hpclang_sound_alarm_rearm(&bus, &alarm, 200));
    assert(!(h.regs[ALARM_MASK] & 0x4000));
    assert(h.regs[ALARM_SOURCE] == before.regs[ALARM_SOURCE]);
    assert(h.regs[ALARM_PENDING] == before.regs[ALARM_PENDING]);
    hpclang_sound_alarm_release(&bus, &alarm);
    assert(!memcmp(h.regs, before.regs, sizeof h.regs));
    writes = h.writes;
    hpclang_sound_alarm_release(&bus, &alarm);
    assert(hpclang_sound_alarm_rearm(&bus, &alarm, 1) == -1 && errno == EINVAL);
    assert(h.writes == writes);
    assert(hpclang_sound_alarm_acquire(NULL, &alarm, 1, 1, 1) == -1 && errno == EINVAL);
    assert(hpclang_sound_alarm_acquire(&bus, NULL, 1, 1, 1) == -1 && errno == EINVAL);
    struct hpclang_sound_alarm_bus unavailable = {&h, read_reg, NULL};
    assert(hpclang_sound_alarm_acquire(&unavailable, &alarm, 1, 1, 1) == -1 && errno == EINVAL);
    unavailable.read = NULL;
    unavailable.write = write_reg;
    assert(hpclang_sound_alarm_acquire(&unavailable, &alarm, 1, 1, 1) == -1 && errno == EINVAL);
    hpclang_sound_alarm_release(NULL, &alarm);
    hpclang_sound_alarm_release(&bus, NULL);
    assert(h.writes == writes);
}
int main(void)
{
    timing();
    ownership();
    return 0;
}
