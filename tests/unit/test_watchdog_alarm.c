#include "watchdog-alarm.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

struct hardware
{
    uint32_t regs[6];
    unsigned writes;
};
static uint32_t read_reg(void *context, enum hpclang_watchdog_reg reg)
{
    return ((struct hardware *)context)->regs[reg];
}
static void write_reg(void *context, enum hpclang_watchdog_reg reg, uint32_t value)
{
    struct hardware *h = context;
    ++h->writes;
    if (reg == WDT_CONTROL)
        assert((value & 0x21) != 0x21); /* No write may enable hardware reset. */
    if (reg == WDT_COUNT || reg == WDT_RELOAD)
        assert(value <= 65535);
    if (reg == WDT_SOURCE || reg == WDT_PENDING) {
        assert(value == 0x200);
        h->regs[reg] &= ~value;
    } else
        h->regs[reg] = value;
}
static struct hardware initial(void)
{
    return (struct hardware){{0x919, 12345, 6789, 0xfffffdff, 0x4040, 0x40}, 0};
}
static void timing(void)
{
    const uint64_t clocks[][2] = {{60000000, 1}, {147456000, 7}, {1, 1}, {UINT64_MAX, 1}};
    const uint64_t delays[] = {1, 7, 1000, 100000, 1000000, UINT32_MAX, UINT64_MAX - 10};
    for (unsigned c = 0; c < sizeof clocks / sizeof clocks[0]; ++c) {
        for (unsigned d = 0; d < sizeof delays / sizeof delays[0]; ++d) {
            struct hardware h = initial(), before = h;
            struct hpclang_watchdog_bus bus = {&h, read_reg, write_reg};
            struct hpclang_watchdog_alarm alarm = {0};
            assert(!hpclang_watchdog_acquire(&bus, &alarm, clocks[c][0], clocks[c][1], 10,
                                             10 + delays[d]));
            assert(alarm.owned && (h.regs[WDT_MASK] & 0x200));
            assert((h.regs[WDT_CONTROL] & 0xff3d) == 0x3c);
            uint64_t ticks = h.regs[WDT_COUNT];
            __uint128_t required = (__uint128_t)clocks[c][0] * delays[d];
            __uint128_t unit = (__uint128_t)clocks[c][1] * 128000000;
            assert(ticks >= 1 && ticks <= 65535 && h.regs[WDT_RELOAD] == ticks);
            if (required > 65535 * unit)
                assert(ticks == 65535);
            else
                assert(ticks * unit >= required && (ticks - 1) * unit < required);
            h.regs[WDT_MASK] &= ~0x200u;
            h.regs[WDT_SOURCE] |= 0x200;
            h.regs[WDT_PENDING] |= 0x200;
            assert(!hpclang_watchdog_advance(&bus, &alarm, 10, 10 + delays[d]));
            assert(!(h.regs[WDT_MASK] & 0x200));
            assert(h.regs[WDT_SOURCE] == before.regs[WDT_SOURCE]);
            assert(h.regs[WDT_PENDING] == before.regs[WDT_PENDING]);
            assert(hpclang_watchdog_advance(&bus, &alarm, 10 + delays[d], 10 + delays[d]) == 1);
            assert(!alarm.owned && !memcmp(h.regs, before.regs, sizeof h.regs));
            unsigned writes = h.writes;
            hpclang_watchdog_release(&bus, &alarm);
            assert(h.writes == writes);
        }
    }
}
static void rejection(void)
{
    for (unsigned failure = 0; failure < 9; ++failure) {
        struct hardware h = initial();
        struct hpclang_watchdog_bus bus = {&h, read_reg, write_reg};
        struct hpclang_watchdog_alarm alarm = {0};
        uint64_t num = 60000000, den = 1, deadline = 100;
        switch (failure) {
        case 0:
            h.regs[WDT_CONTROL] |= 0x20;
            break;
        case 1:
            h.regs[WDT_SOURCE] |= 0x200;
            break;
        case 2:
            h.regs[WDT_PENDING] |= 0x200;
            break;
        case 3:
            alarm.owned = 1;
            break;
        case 4:
            num = 0;
            break;
        case 5:
            den = 0;
            break;
        case 6:
            den = UINT64_MAX;
            break;
        case 7:
            deadline = 10;
            break;
        case 8:
            deadline = 9;
            break;
        }
        struct hardware before = h;
        struct hpclang_watchdog_alarm saved = alarm;
        assert(hpclang_watchdog_acquire(&bus, &alarm, num, den, 10, deadline) == -1);
        assert(errno == (failure < 4 ? EBUSY : failure == 6 ? ERANGE : EINVAL));
        assert(!memcmp(&h, &before, sizeof h));
        assert(!memcmp(&alarm, &saved, sizeof alarm));
    }
    struct hardware h = initial();
    struct hpclang_watchdog_bus bus = {&h, read_reg, write_reg};
    struct hpclang_watchdog_alarm alarm = {0};
    assert(hpclang_watchdog_acquire(0, &alarm, 1, 1, 0, 1) == -1 && errno == EINVAL);
    assert(hpclang_watchdog_advance(&bus, &alarm, 1, 2) == -1 && errno == EINVAL);
    assert(!h.writes);
    /* Cancellation restores an originally masked source, leaves concurrent
     * unrelated source/mask changes intact, and supports late dispatch. */
    h.regs[WDT_MASK] |= 0x200;
    assert(!hpclang_watchdog_acquire(&bus, &alarm, 60000000, 1, 0, 100));
    h.regs[WDT_SOURCE] |= 0x100;
    h.regs[WDT_MASK] &= ~0x100u;
    assert(hpclang_watchdog_advance(&bus, &alarm, 1000, 100) == 1);
    assert((h.regs[WDT_SOURCE] & 0x100) && !(h.regs[WDT_MASK] & 0x100));
    assert(h.regs[WDT_MASK] & 0x200);
    assert(h.regs[WDT_CONTROL] == initial().regs[WDT_CONTROL]);
}
int main(void)
{
    timing();
    rejection();
    return 0;
}
