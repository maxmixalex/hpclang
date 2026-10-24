#include "irq-owner.h"
#include <errno.h>

static int valid(const struct hpclang_irq_owner_bus *bus, const struct hpclang_irq_owner *owner)
{
    return bus && bus->read && bus->write && bus->exchange_bank && owner;
}
int hpclang_irq_owner_acquire(const struct hpclang_irq_owner_bus *bus,
                              struct hpclang_irq_owner *owner, uint32_t source, uint32_t stack_top,
                              uint32_t handler)
{
    if (!valid(bus, owner) || !source || (source & (source - 1)) || !stack_top || (stack_top & 7) ||
        !handler || (handler & 3)) {
        errno = EINVAL;
        return -1;
    }
    if (owner->owned) {
        errno = EBUSY;
        return -1;
    }
    *owner = (struct hpclang_irq_owner){{stack_top, 0, 0},
                                        bus->read(bus->context, IRQ_OWNER_MASK),
                                        bus->read(bus->context, IRQ_OWNER_MODE),
                                        bus->read(bus->context, IRQ_OWNER_VECTOR),
                                        source,
                                        1};
    bus->write(bus->context, IRQ_OWNER_MASK, UINT32_MAX);
    /* Route all sources to IRQ while borrowed. INTMSK does not isolate an
     * FIQ-routed source; retaining Stock FIQ routing can block owned IRQ
     * arbitration even though the CPU masks FIQ. Unowned IRQs stay masked. */
    bus->write(bus->context, IRQ_OWNER_MODE, 0);
    bus->write(bus->context, IRQ_OWNER_PENDING, bus->read(bus->context, IRQ_OWNER_PENDING));
    bus->exchange_bank(bus->context, &owner->bank);
    bus->write(bus->context, IRQ_OWNER_VECTOR, handler);
    bus->write(bus->context, IRQ_OWNER_MASK, ~source);
    return 0;
}
void hpclang_irq_owner_release(const struct hpclang_irq_owner_bus *bus,
                               struct hpclang_irq_owner *owner)
{
    if (!valid(bus, owner) || !owner->owned)
        return;
    bus->write(bus->context, IRQ_OWNER_MASK, UINT32_MAX);
    bus->write(bus->context, IRQ_OWNER_VECTOR, owner->vector);
    bus->exchange_bank(bus->context, &owner->bank);
    bus->write(bus->context, IRQ_OWNER_MODE, owner->mode);
    bus->write(bus->context, IRQ_OWNER_MASK, owner->mask);
    owner->owned = 0;
}
