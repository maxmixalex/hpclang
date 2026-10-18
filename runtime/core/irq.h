#ifndef HPCLANG_IRQ_H
#define HPCLANG_IRQ_H

#include <stdint.h>

/* Save the caller CPSR and mask both IRQ/FIQ through the supervisor bridge. */
uint32_t hpclang_irq_save_disable(void);
void hpclang_irq_restore(uint32_t state);

struct hpclang_irq_bank
{
    uint32_t stack, link, status;
};

/* Exchange the banked IRQ SP/LR/SPSR through the supervisor bridge. The caller
 * must mask IRQ/FIQ, supply valid writable state and stay outside IRQ context.
 * To install, initialize stack to an 8-byte-aligned owned stack top and the
 * other fields to zero. The exchanged state retains the original bank; exchange
 * it again after masking interrupts to restore it before releasing the stack.
 * This does not change the IRQ vector, controller routing or interrupt masks.
 */
void hpclang_irq_bank_exchange(struct hpclang_irq_bank *state);

#endif
