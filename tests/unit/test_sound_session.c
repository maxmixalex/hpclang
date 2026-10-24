#include "sound-session.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

struct hardware
{
    uint32_t regs[15];
    struct hpclang_irq_bank bank;
    unsigned writes, exchanges;
    int in_irq;
};
static const unsigned watchdog_regs[] = {10, 11, 12, 7, 8, 9};
static const unsigned irq_regs[] = {7, 13, 9, 14};
static void write_index(struct hardware *h, unsigned reg, uint32_t value)
{
    ++h->writes;
    if (reg == 8 || reg == 9)
        h->regs[reg] &= ~value;
    else
        h->regs[reg] = value;
    if (reg == 2 && (value & 0x2000))
        h->regs[5] = h->regs[3];
    if (reg == 10)
        assert((value & 0x21) != 0x21);
}
static uint32_t sound_read(void *ctx, enum hpclang_sound_reg reg)
{
    return ((struct hardware *)ctx)->regs[reg];
}
static void sound_write(void *ctx, enum hpclang_sound_reg reg, uint32_t value)
{
    write_index(ctx, reg, value);
}
static uint32_t watchdog_read(void *ctx, enum hpclang_watchdog_reg reg)
{
    return ((struct hardware *)ctx)->regs[watchdog_regs[reg]];
}
static void watchdog_write(void *ctx, enum hpclang_watchdog_reg reg, uint32_t value)
{
    write_index(ctx, watchdog_regs[reg], value);
}
static uint32_t irq_read(void *ctx, enum hpclang_irq_owner_reg reg)
{
    return ((struct hardware *)ctx)->regs[irq_regs[reg]];
}
static void irq_write(void *ctx, enum hpclang_irq_owner_reg reg, uint32_t value)
{
    write_index(ctx, irq_regs[reg], value);
}
static void exchange(void *ctx, struct hpclang_irq_bank *bank)
{
    struct hardware *h = ctx;
    assert(!h->in_irq && h->regs[7] == UINT32_MAX);
    struct hpclang_irq_bank before = h->bank;
    h->bank = *bank;
    *bank = before;
    ++h->exchanges;
}
static int start(const struct hpclang_sound_session_bus *bus, struct hpclang_sound_session *s,
                 uint32_t frequency, uint64_t now, uint64_t deadline, int replace)
{
    return hpclang_sound_session_start(bus, s, 50000000, 1, frequency, now, deadline, 0x11008000,
                                       0x12000200, replace);
}
static void restored(const struct hardware *h, const struct hardware *before)
{
    assert(!memcmp(h->regs, before->regs, sizeof h->regs));
    assert(!memcmp(&h->bank, &before->bank, sizeof h->bank));
}
int main(void)
{
    struct hardware h = {{0x6300, 0x33333, 0x400809, 800, 200, 600, 0x100, 0xbdff8346, 0, 0, 0x919,
                          1234, 567, 0x1000, 0x08000400},
                         {0x080005fc, 123, 0x93},
                         0,
                         0,
                         0};
    struct hardware original = h;
    struct hpclang_sound_session_bus bus = {{sound_read, sound_write, &h},
                                            {&h, watchdog_read, watchdog_write},
                                            {&h, irq_read, irq_write, exchange}};
    struct hpclang_sound_session s = {0};
    assert(!start(&bus, &s, 880, 10, 100000, 0));
    assert(s.state == SOUND_SESSION_RUNNING && s.irq.owned && s.tone.owned && s.alarm.owned);
    assert(h.regs[7] == ~0x200u && h.regs[13] == 0);
    struct hardware active = h;
    assert(start(&bus, &s, 440, 20, 200000, 0) == -1 && errno == EBUSY);
    assert(!memcmp(&h, &active, sizeof h));
    assert(start(&bus, &s, 0, 20, 200000, 1) == -1 && errno == EINVAL);
    assert(!memcmp(&h, &active, sizeof h));
    h.in_irq = 1;
    errno = EDOM;
    hpclang_sound_session_advance(&bus, &s, 1000, 0);
    assert(s.state == SOUND_SESSION_RUNNING && errno == EDOM);
    h.regs[8] = h.regs[9] = 0x200;
    hpclang_sound_session_advance(&bus, &s, 100001, 0);
    assert(s.state == SOUND_SESSION_DONE && !s.error && errno == EDOM);
    assert(!s.tone.owned && !s.alarm.owned && s.irq.owned && h.exchanges == 1);
    assert(!memcmp(h.regs, original.regs, 7 * sizeof(uint32_t)));
    assert(!memcmp(h.regs + 10, original.regs + 10, 3 * sizeof(uint32_t)));
    assert(h.regs[7] == UINT32_MAX && !h.regs[8] && !h.regs[9]);
    unsigned writes = h.writes;
    hpclang_sound_session_advance(&bus, &s, 100002, 0);
    assert(h.writes == writes);
    h.in_irq = 0;
    hpclang_sound_session_close(&bus, &s);
    restored(&h, &original);
    writes = h.writes;
    hpclang_sound_session_close(&bus, &s);
    assert(h.writes == writes && s.state == SOUND_SESSION_IDLE);

    /* Replacement and cancellation restore the original host, not the replaced
     * tone's temporary configuration. Clock failures stop before foreground. */
    assert(!start(&bus, &s, 880, 0, UINT64_MAX, 0));
    assert(!start(&bus, &s, 440, 1, 100000, 1));
    h.in_irq = 1;
    hpclang_sound_session_advance(&bus, &s, 0, 0);
    assert(s.state == SOUND_SESSION_FAILED && s.error == EIO);
    assert(!s.tone.owned && !s.alarm.owned && s.irq.owned);
    h.in_irq = 0;
    assert(!start(&bus, &s, 880, 5, 10000, 0));
    h.in_irq = 1;
    hpclang_sound_session_advance(&bus, &s, 6, ERANGE);
    assert(s.state == SOUND_SESSION_FAILED && s.error == ERANGE);
    h.in_irq = 0;
    hpclang_sound_session_close(&bus, &s);
    restored(&h, &original);
    assert(!start(&bus, &s, 880, 0, 1000, 0));
    hpclang_sound_session_close(&bus, &s);
    restored(&h, &original);

    for (unsigned fault = 0; fault < 3; ++fault) {
        h = original;
        if (fault == 0)
            h.regs[10] |= 0x20;
        if (fault == 1)
            h.regs[2] |= 0x1000;
        struct hardware before = h;
        assert(start(&bus, &s, fault == 2 ? UINT32_MAX : 880, 0, 10000, 0) == -1);
        assert(errno == (fault == 2 ? ERANGE : EBUSY));
        assert(s.state == SOUND_SESSION_IDLE && !s.irq.owned && !s.tone.owned && !s.alarm.owned);
        restored(&h, &before);
    }
    h = original;
    assert(!start(&bus, &s, 880, 0, 1000, 0));
    assert(start(&bus, &s, UINT32_MAX, 1, 2000, 1) == -1 && errno == ERANGE);
    restored(&h, &original);
    return 0;
}
