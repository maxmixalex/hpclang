#ifndef HPCLANG_RUNTIME_RPL_EXECUTION_H
#define HPCLANG_RUNTIME_RPL_EXECUTION_H

#include <hpclang/abi.h>
#include "rpl-code.h"

enum hpclang_rpl_execution_space
{
    HPCLANG_RPL_EXECUTION_CODE,
    HPCLANG_RPL_EXECUTION_DATA,
    HPCLANG_RPL_EXECUTION_NATIVE,
    HPCLANG_RPL_EXECUTION_PC
};

/* CODE/DATA use Saturn nibble addresses; NATIVE uses CPU-relative offsets;
 * PC uses offsets 0..7 in the named RPL5 execution-PC word. The observer
 * retains mapping/field identity and all reads as transaction dependencies.
 * Return zero or a negative error; positive errors become MALFORMED. */
typedef int (*hpclang_rpl_execution_observe_fn)(void *context, unsigned space, uint32_t address,
                                                uint8_t value);

/* Called after each supported instruction, with its original PC and resulting
 * simulated state. Zero continues, one stops at a caller-selected boundary,
 * negative aborts; other positive values are malformed. Stopping is not a
 * completeness certificate. Uses include generated/unknown zero origins,
 * which still require separate proof before relocation. */
typedef int (*hpclang_rpl_execution_visit_fn)(void *context, uint32_t pc,
                                              const struct hpclang_rpl_code_execution *state,
                                              const struct hpclang_rpl_code_step_uses *uses);

struct hpclang_rpl_execution_io
{
    struct hpclang_rpl_reader native, code, data;
    hpclang_rpl_execution_observe_fn observe;
    hpclang_rpl_code_token_fn token;
    hpclang_rpl_execution_visit_fn visit;
    void *context;
};

enum hpclang_rpl_execution_stop
{
    HPCLANG_RPL_EXECUTION_BOUNDARY = 1,
    HPCLANG_RPL_EXECUTION_BUDGET = 2
};

/* Capture the negotiated RPL5 entry and CPU state, then collect a bounded
 * sequence of supported instructions with all source reads observed. The
 * caller qualifies firmware/layout and reader mappings; native must refer to
 * boot->saturn_state. Exclude interrupts, native execution and mapping/state
 * changes throughout capture/collection and later guard validation/commit.
 * Code/data tokens and native_origin_base occupy disjoint qualified namespaces.
 *
 * state is caller-owned scratch (prefer heap storage), disjoint from io, boot,
 * steps and callback storage. steps counts evaluated instructions. Valid calls
 * initialize steps to zero; errors may leave partial scratch/observations,
 * which must not authorize a write. Readers and callbacks are read-only.
 * Return BOUNDARY, BUDGET or a negative span/callback error; never success zero.
 * Budget exhaustion and unsupported instructions do not establish closure.
 * No native writes, allocation, bank switching or implicit mutation permission. */
int hpclang_rpl_execution_collect(const struct hpclang_boot_info *boot,
                                  const struct hpclang_rpl_execution_io *io,
                                  uint32_t native_origin_base, uint32_t budget,
                                  struct hpclang_rpl_code_execution *state, uint32_t *steps);

#endif
