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
    hpclang_rpl_code_derive_fn derive; /* Optional caller-owned expression arena. */
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
 * which must not authorize a write. Readers and callbacks never modify target state.
 * Return BOUNDARY, BUDGET or a negative span/callback error; never success zero.
 * Budget exhaustion and unsupported instructions do not establish closure.
 * No native writes, allocation, bank switching or implicit mutation permission. */
int hpclang_rpl_execution_collect(const struct hpclang_boot_info *boot,
                                  const struct hpclang_rpl_execution_io *io,
                                  uint32_t native_origin_base, uint32_t budget,
                                  struct hpclang_rpl_code_execution *state, uint32_t *steps);

/* Same collection with caller-owned simulated stores. The store callback and
 * code/data readers/token resolver share io->context-managed effect storage.
 * Observe scratch reads as derived dependencies, not unchanged original memory.
 * Retain the original sources/overwritten cells separately for validation.
 * On any error discard all effect storage and partial observations. No target
 * writes occur; the caller qualifies aliasing, mappings and store permissions. */
int hpclang_rpl_execution_collect_effects(const struct hpclang_boot_info *boot,
                                          const struct hpclang_rpl_execution_io *io,
                                          uint32_t native_origin_base, uint32_t budget,
                                          struct hpclang_rpl_code_execution *state, uint32_t *steps,
                                          hpclang_rpl_code_store_fn store);

/* Add all consumed address uses from one evaluated step to an initialized
 * edit builder. replacements has uses->count caller-qualified addresses;
 * include unchanged values, because their sources may alias other repairs.
 * Resolve every source digit, including unchanged ones; unknown origins reject.
 * Invert each use's qualified modular adjustment before planning source edits.
 * The caller guards the arithmetic instructions and proves the source change
 * preserves all other uses/effects; an invertible expression alone is not that proof.
 * Normalize each source into a one-nibble edit so consistent partial overlaps
 * across uses/steps merge and conflicting requirements invalidate the batch.
 * Use this builder exclusively for normalized one-nibble edits. Resolvers
 * normalize physical aliases and retain guards/source identity independently.
 * All errors latch builder->error and clear count; restart with edits_begin.
 * Inputs/callback state and builder storage must be disjoint and stable.
 * No allocation or native writes. This plans supplied address uses only:
 * direct branches, derived/unknown sources, entry PC and completeness remain
 * separate obligations. Caller proves replacement semantics and excludes source
 * changes through later conversion/validation/commit. Never infer permission
 * from a successful empty/partial batch or omit a use that fails planning. */
int hpclang_rpl_execution_plan_uses(struct hpclang_rpl_reference_edit_builder *builder,
                                    const struct hpclang_rpl_code_step_uses *uses,
                                    const uint32_t *replacements,
                                    hpclang_rpl_code_origin_resolve_fn resolve, void *context);

/* Plan both sources of a caller-proven selected-field subtraction. Values are
 * low 20-bit values; the selected field must lie within these five nibbles.
 * Verify original result/borrow against both operands, and proposed result
 * against both caller-qualified replacements. Preserve borrow so this repair
 * cannot silently change subsequent carry-dependent control flow. Resolve all
 * left origins and selected right origins, including unchanged digits; unused
 * right digits need no origins and cannot be changed. Right-side modular
 * adjustments are not supported. Merge with the SAME
 * normalized builder used for other consumed values. Conflicting aliases,
 * stale/unknown sources or any error invalidate the whole batch.
 *
 * The caller records operands BEFORE execution, guards the instruction/P/mode,
 * and proves this relation supplies the consumed value. No inference from a
 * numeric match, automatic expression tracking, operand selection, or complete
 * coverage is provided. Other uses/effects of both repaired operands remain
 * independent obligations. Inputs/storage must be stable and disjoint. */
int hpclang_rpl_execution_plan_subtraction(
    struct hpclang_rpl_reference_edit_builder *builder,
    const struct hpclang_rpl_execution_subtraction *expression, uint32_t left, uint32_t right,
    uint32_t replacement, hpclang_rpl_code_origin_resolve_fn resolve, void *context);

#endif
