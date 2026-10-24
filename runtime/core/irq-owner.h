#ifndef HPCLANG_IRQ_OWNER_H
#define HPCLANG_IRQ_OWNER_H
#include "irq.h"

enum hpclang_irq_owner_reg
{
    IRQ_OWNER_MASK,
    IRQ_OWNER_MODE,
    IRQ_OWNER_PENDING,
    IRQ_OWNER_VECTOR
};
struct hpclang_irq_owner_bus
{
    void *context;
    uint32_t (*read)(void *, enum hpclang_irq_owner_reg);
    void (*write)(void *, enum hpclang_irq_owner_reg, uint32_t);
    void (*exchange_bank)(void *, struct hpclang_irq_bank *);
};
struct hpclang_irq_owner
{
    struct hpclang_irq_bank bank;
    uint32_t mask, mode, vector, source;
    int owned;
};
/* Exclusive application IRQ-vector/stack lease. Call only from foreground
 * with IRQ/FIQ masked. Caller serializes all owners and supplies a live,
 * 8-byte-aligned stack and ARM handler. Acquisition masks all sources before
 * replacing the bank/vector, acknowledges the old selection (never SRCPND),
 * routes all sources to IRQ, and leaves only the owned source unmasked.
 * CPU IRQ enabling belongs to the caller. Stop the peripheral before release.
 * Release restores bank/vector/routing/masks and is idempotent. It must never
 * run on the IRQ stack being released. No allocation or user callbacks.
 */
int hpclang_irq_owner_acquire(const struct hpclang_irq_owner_bus *, struct hpclang_irq_owner *,
                              uint32_t source, uint32_t stack_top, uint32_t handler);
void hpclang_irq_owner_release(const struct hpclang_irq_owner_bus *, struct hpclang_irq_owner *);
#endif
