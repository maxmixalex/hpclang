#include <assert.h>
#include <errno.h>
#include <string.h>
#include "rtc-tick.h"

struct fixture
{
    uint32_t regs[6];
    unsigned writes;
};

static uint32_t read_reg(void *context, enum hpclang_rtc_tick_reg reg)
{
    return ((struct fixture *)context)->regs[reg];
}

static void write_reg(void *context, enum hpclang_rtc_tick_reg reg, uint32_t value)
{
    struct fixture *f = context;
    ++f->writes;
    if (reg == RTC_TICK_SOURCE || reg == RTC_TICK_PENDING) {
        assert(value == 0x100);
        f->regs[reg] &= ~value; /* Physical write-one-to-clear semantics. */
    } else {
        assert(reg == RTC_TICK_MASK || reg == RTC_TICK_COUNT);
        f->regs[reg] = value;
    }
}

int main(void)
{
    struct fixture f = {{0, 0x8f, 0x1000, 0x4000, 0x2000, 0x2000}, 0};
    const struct fixture initial = f;
    const struct hpclang_rtc_tick_bus bus = {&f, read_reg, write_reg};
    struct hpclang_rtc_tick_saved saved = {0};
    assert(hpclang_rtc_tick_poll(&bus, &saved) == -1 && errno == EINVAL);
    assert(hpclang_rtc_tick_set(&bus, &saved, 15) == -1 && errno == EINVAL);
    hpclang_rtc_tick_release(&bus, &saved);
    assert(!f.writes);
    for (unsigned selector = 0; selector <= 255; ++selector) {
        f = initial;
        memset(&saved, 0, sizeof saved);
        int result = hpclang_rtc_tick_acquire(&bus, &saved, selector);
        if (!selector || selector > 127) {
            assert(result == -1 && errno == EINVAL && !f.writes && !saved.owned);
            continue;
        }
        assert(!result && saved.owned && f.regs[RTC_TICK_COUNT] == (selector | 0x80));
        assert(f.regs[RTC_TICK_MASK] == 0x4100);
        unsigned writes = f.writes;
        assert(hpclang_rtc_tick_acquire(&bus, &saved, selector) == -1 && errno == EBUSY);
        assert(f.writes == writes);
        assert(!hpclang_rtc_tick_poll(&bus, &saved));
        f.regs[RTC_TICK_SOURCE] |= 0x100;
        f.regs[RTC_TICK_SOURCE] |= 0x100; /* Two events still yield one indication. */
        assert(hpclang_rtc_tick_poll(&bus, &saved) == 1);
        assert(!hpclang_rtc_tick_poll(&bus, &saved));
        assert(f.regs[RTC_TICK_SOURCE] == 0x2000 && f.regs[RTC_TICK_PENDING] == 0x2000);
        f.regs[RTC_TICK_MASK] |= 0x8000; /* Another service's mask change survives. */
        f.regs[RTC_TICK_SOURCE] |= 0x100;
        hpclang_rtc_tick_release(&bus, &saved);
        assert(!saved.owned && f.regs[RTC_TICK_COUNT] == 0x8f);
        assert(f.regs[RTC_TICK_MASK] == 0xc000 && f.regs[RTC_TICK_SOURCE] == 0x2000);
        assert(f.regs[RTC_TICK_PENDING] == 0x2000);
        writes = f.writes;
        hpclang_rtc_tick_release(&bus, &saved);
        assert(f.writes == writes);
    }
    for (unsigned consume = 0; consume < 2; ++consume) {
        f = initial;
        f.regs[RTC_TICK_COUNT] = 7; /* Preserve a host-disabled divider too. */
        f.regs[RTC_TICK_MASK] |= 0x100;
        f.regs[RTC_TICK_SOURCE] |= 0x100;
        f.regs[RTC_TICK_PENDING] = 0x100;
        memset(&saved, 0, sizeof saved);
        assert(!hpclang_rtc_tick_acquire(&bus, &saved, 15));
        unsigned writes = f.writes;
        assert(hpclang_rtc_tick_set(&bus, &saved, 128) == -1 && errno == EINVAL);
        assert(f.writes == writes && f.regs[RTC_TICK_COUNT] == 0x8f);
        assert(!hpclang_rtc_tick_set(&bus, &saved, 127));
        assert(f.regs[RTC_TICK_SOURCE] == 0x2100); /* Changing period retains pending. */
        if (consume)
            assert(hpclang_rtc_tick_poll(&bus, &saved) == 1);
        hpclang_rtc_tick_release(&bus, &saved);
        assert(f.regs[RTC_TICK_COUNT] == 7 && f.regs[RTC_TICK_MASK] == 0x4100);
        assert(f.regs[RTC_TICK_SOURCE] == (consume ? 0x2000 : 0x2100));
        assert(f.regs[RTC_TICK_PENDING] == (consume ? 0 : 0x100));
    }
    for (unsigned conflict = 0; conflict < 5; ++conflict) {
        f = initial;
        memset(&saved, 0, sizeof saved);
        if (conflict < 3)
            f.regs[RTC_TICK_CONTROL] |= 2u << conflict;
        else if (conflict == 3)
            f.regs[RTC_TICK_MODE] |= 0x100;
        else
            f.regs[RTC_TICK_PENDING] |= 0x100;
        struct fixture before = f;
        assert(hpclang_rtc_tick_acquire(&bus, &saved, 15) == -1);
        assert(errno == (conflict == 4 ? EBUSY : ENOTSUP));
        assert(!memcmp(&f, &before, sizeof f) && !saved.owned);
    }
    return 0;
}
