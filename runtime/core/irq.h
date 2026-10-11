#ifndef HPCLANG_IRQ_H
#define HPCLANG_IRQ_H

#include <stdint.h>

/* Save the caller CPSR and mask both IRQ/FIQ through the supervisor bridge. */
uint32_t hpclang_irq_save_disable(void);
void hpclang_irq_restore(uint32_t state);

#endif
