#ifndef HPCLANG_E2E_STOCK_RETURN_PHASE_H
#define HPCLANG_E2E_STOCK_RETURN_PHASE_H

#include <stdint.h>

/*
 * ARMToolbox stage0 temporarily owns L1[0x90] while its 0x09000000 code/stack
 * window is live. The Stock launcher records the exact coarse descriptor it
 * observes for that invocation and waits until that descriptor no longer
 * occupies the slot before injecting the post-return native UI action.
 *
 * Waiting for descriptor ownership rather than a literal zero is intentional:
 * stage0 clears the slot before restoring interrupts, but Stock may reuse the
 * address space before the host's next 20 ms poll observes that transient zero.
 */
#define HPCLANG_STOCK_STAGE_L1_SLOT UINT32_C(0x07f00240)
#define HPCLANG_STOCK_TEARDOWN_DEADLINE_US INT64_C(2000000)

#endif
