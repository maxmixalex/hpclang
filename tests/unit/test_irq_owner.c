#include "irq-owner.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

struct hardware
{
    uint32_t regs[4];
    struct hpclang_irq_bank bank;
    unsigned writes, exchanges;
};
static uint32_t read_reg(void *ctx, enum hpclang_irq_owner_reg reg)
{
    return ((struct hardware *)ctx)->regs[reg];
}
static void write_reg(void *ctx, enum hpclang_irq_owner_reg reg, uint32_t value)
{
    struct hardware *h = ctx;
    ++h->writes;
    if (reg != IRQ_OWNER_MASK)
        assert(h->regs[IRQ_OWNER_MASK] == UINT32_MAX);
    if (reg == IRQ_OWNER_PENDING)
        h->regs[reg] &= ~value;
    else
        h->regs[reg] = value;
}
static void exchange(void *ctx, struct hpclang_irq_bank *bank)
{
    struct hardware *h = ctx;
    assert(h->regs[IRQ_OWNER_MASK] == UINT32_MAX);
    struct hpclang_irq_bank old = h->bank;
    h->bank = *bank;
    *bank = old;
    ++h->exchanges;
}
int main(void)
{
    struct hardware h = {{0xbdff8346, 0x1200, 0x40, 0x08000400}, {0x080005fc, 123, 0x93}, 0, 0};
    struct hardware before = h;
    struct hpclang_irq_owner_bus bus = {&h, read_reg, write_reg, exchange};
    struct hpclang_irq_owner owner = {0};
    for (unsigned bad = 0; bad < 7; ++bad) {
        uint32_t source = 0x200, stack = 0x10008000, handler = 0x10000200;
        if (bad == 0)
            source = 0;
        if (bad == 1)
            source = 3;
        if (bad == 2)
            stack = 0;
        if (bad == 3)
            stack += 4;
        if (bad == 4)
            handler = 0;
        if (bad == 5)
            handler += 1;
        if (bad == 6)
            handler += 2;
        assert(hpclang_irq_owner_acquire(&bus, &owner, source, stack, handler) == -1);
        assert(errno == EINVAL && !owner.owned && !memcmp(&h, &before, sizeof h));
    }
    assert(!hpclang_irq_owner_acquire(&bus, &owner, 0x200, 0x10008000, 0x10000200));
    assert(owner.owned && owner.source == 0x200);
    assert(!memcmp(&owner.bank, &before.bank, sizeof owner.bank));
    assert(h.regs[IRQ_OWNER_MASK] == ~0x200u && h.regs[IRQ_OWNER_MODE] == 0);
    assert(h.regs[IRQ_OWNER_VECTOR] == 0x10000200 && !h.regs[IRQ_OWNER_PENDING]);
    assert(h.bank.stack == 0x10008000 && !h.bank.link && !h.bank.status);
    struct hardware running = h;
    assert(hpclang_irq_owner_acquire(&bus, &owner, 0x200, 0x10008000, 0x10000200) == -1);
    assert(errno == EBUSY && !memcmp(&h, &running, sizeof h));
    h.bank.link = 456;                 /* Handler may change its own bank; restore the old bank. */
    h.regs[IRQ_OWNER_PENDING] = 0x100; /* Never clear new unrelated selections on release. */
    hpclang_irq_owner_release(&bus, &owner);
    assert(!owner.owned && h.exchanges == 2);
    assert(!memcmp(&h.bank, &before.bank, sizeof h.bank));
    assert(h.regs[IRQ_OWNER_MASK] == before.regs[IRQ_OWNER_MASK]);
    assert(h.regs[IRQ_OWNER_MODE] == before.regs[IRQ_OWNER_MODE]);
    assert(h.regs[IRQ_OWNER_VECTOR] == before.regs[IRQ_OWNER_VECTOR]);
    assert(h.regs[IRQ_OWNER_PENDING] == 0x100);
    unsigned writes = h.writes;
    hpclang_irq_owner_release(&bus, &owner);
    assert(h.writes == writes && h.exchanges == 2);
    return 0;
}
