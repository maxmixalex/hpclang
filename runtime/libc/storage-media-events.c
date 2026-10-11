#include <errno.h>
#include "storage-media-events.h"
#include "storage-mmio.h"
#include "../core/irq.h"

enum
{
    SOURCE_PENDING = 0x07100000u,
    INTERRUPT_ROUTE = 0x07100004u,
    INTERRUPT_MASK = 0x07100008u,
    INTERRUPT_PENDING = 0x07100010u,
    PIN_FUNCTION = 0x07a00050u,
    EXTERNAL_TRIGGER = 0x07a00088u,
    CARD_EVENT = 8u,
    CARD_PIN_MASK = 0xc0u,
    CARD_PIN_INTERRUPT = 0x80u,
    CARD_TRIGGER_MASK = 0x7000u,
    CARD_FALLING_EDGE = 0x2000u
};

static void replace(uintptr_t address, uint32_t mask, uint32_t value)
{
    uint32_t current = hpclang_storage_mmio_read32(address);
    hpclang_storage_mmio_write32(address, (current & ~mask) | (value & mask));
}

static void acknowledge(void)
{
    hpclang_storage_mmio_write32(SOURCE_PENDING, CARD_EVENT);
    hpclang_storage_mmio_write32(INTERRUPT_PENDING, CARD_EVENT);
}

int hpclang_storage_media_events_acquire(struct hpclang_storage_media_events *events)
{
    if (!events || events->owned) {
        errno = events ? EBUSY : EINVAL;
        return -1;
    }
    uint32_t irq = hpclang_irq_save_disable();
    uint32_t pin = hpclang_storage_mmio_read32(PIN_FUNCTION) & CARD_PIN_MASK;
    if (pin != 0u && pin != CARD_PIN_INTERRUPT) {
        hpclang_irq_restore(irq);
        errno = ENOTSUP;
        return -1;
    }
    events->pin = pin;
    events->trigger = hpclang_storage_mmio_read32(EXTERNAL_TRIGGER) & CARD_TRIGGER_MASK;
    events->mask = hpclang_storage_mmio_read32(INTERRUPT_MASK) & CARD_EVENT;
    events->route = hpclang_storage_mmio_read32(INTERRUPT_ROUTE) & CARD_EVENT;
    replace(INTERRUPT_MASK, CARD_EVENT, CARD_EVENT);
    replace(INTERRUPT_ROUTE, CARD_EVENT, 0);
    replace(EXTERNAL_TRIGGER, CARD_TRIGGER_MASK, CARD_FALLING_EDGE);
    replace(PIN_FUNCTION, CARD_PIN_MASK, CARD_PIN_INTERRUPT);
    /* No mount exists yet. Retire the old card notification before mounting
     * from scratch; every subsequent removal invalidates that new mount. */
    acknowledge();
    events->owned = 1;
    hpclang_irq_restore(irq);
    return 0;
}

int hpclang_storage_media_events_changed(struct hpclang_storage_media_events *events)
{
    if (!events || !events->owned) {
        errno = EPERM;
        return -1;
    }
    uint32_t irq = hpclang_irq_save_disable();
    int changed = (hpclang_storage_mmio_read32(SOURCE_PENDING) & CARD_EVENT) != 0;
    if (changed)
        acknowledge();
    hpclang_irq_restore(irq);
    return changed;
}

void hpclang_storage_media_events_release(struct hpclang_storage_media_events *events)
{
    if (!events || !events->owned)
        return;
    uint32_t irq = hpclang_irq_save_disable();
    replace(PIN_FUNCTION, CARD_PIN_MASK, events->pin);
    replace(EXTERNAL_TRIGGER, CARD_TRIGGER_MASK, events->trigger);
    replace(INTERRUPT_ROUTE, CARD_EVENT, events->route);
    replace(INTERRUPT_MASK, CARD_EVENT, events->mask);
    events->owned = 0;
    hpclang_irq_restore(irq);
}
