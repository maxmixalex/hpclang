#ifndef HPCLANG_RPL_BANK_PLAN_H
#define HPCLANG_RPL_BANK_PLAN_H
#include "rpl-bank-effects.h"
#include "rpl-code.h"
#include "rpl-rom-profile.h"

struct hpclang_rpl_bank_sources
{
    const struct hpclang_rpl_reader *saturn, *native, *rom;
    const struct hpclang_rpl_rom_profile *profile;
    uint32_t saturn_mapping, native_mapping, rom_mapping;
    uint32_t code_first, code_end, configurations;
};
struct hpclang_rpl_bank_plan
{
    struct hpclang_rpl_code_bank_entry program;
    uint32_t bank, configuration;
    struct hpclang_rpl_rom_bank geometry;
    struct hpclang_rpl_bank_effects effects;
};
struct hpclang_rpl_bank_workspace
{
    struct hpclang_rpl_bank_effects prior;
    struct hpclang_rpl_rom_bank banks[16];
    struct hpclang_rpl_bank_plan next;
};

struct hpclang_rpl_bank_transition
{
    struct hpclang_rpl_bank_plan mapping;
    struct hpclang_rpl_bank_cpu_effects cpu;
};
struct hpclang_rpl_bank_transition_workspace
{
    struct hpclang_rpl_bank_workspace planning;
    struct hpclang_rpl_bank_plan selected;
};
/* Compose mapping and normal-return CPU predictions from the same exclusive
 * source snapshot and P context. CPU reads use sources->native_mapping in the
 * shared dependency stream. Reject pending Saturn interrupts and mismatched P.
 * Publish neither result until both succeed; error invalidates all partial
 * dependencies. Workspace is caller-owned and disjoint from inputs/output.
 * Retain every bank-plan qualification. The continuation still requires its
 * own mapping/ownership/code checks; success does not make an EXECUTION edge
 * terminal or authorize mutation. No allocation or native writes. */
int hpclang_rpl_bank_transition_prepare(const struct hpclang_rpl_bank_sources *sources,
                                        uint32_t entry, unsigned p,
                                        struct hpclang_rpl_bank_transition_workspace *workspace,
                                        struct hpclang_rpl_bank_transition *transition,
                                        hpclang_rpl_graph_dependency_fn dependency, void *context);
/* Compose P=0 code inspection, unique configuration lookup, device routing,
 * ROM geometry, complete bank-effect capture/prediction and physical mapping
 * preservation of the inspected switch body. All observations
 * carry the supplied distinct mapping IDs. Profile must come from successful
 * immutable-ROM identification; callers qualify native state, code mapping,
 * configuration table and exclusive lifetime. Prediction does not execute code
 * or classify this EXECUTION edge as terminal. Interrupts, return roots and
 * complete CONFIG semantics still require separate proof.
 * Zero publishes a complete plan; negative span errors preserve output and
 * invalidate partial dependency observations. Caller-owned workspace is scratch
 * and must not overlap inputs/output. No allocation or writes to native state. */
int hpclang_rpl_bank_plan_prepare(const struct hpclang_rpl_bank_sources *sources, uint32_t entry,
                                  struct hpclang_rpl_bank_workspace *workspace,
                                  struct hpclang_rpl_bank_plan *plan,
                                  hpclang_rpl_graph_dependency_fn dependency, void *context);
/* Explicit reviewed P=0 entry or P=1 exit prediction, with the same qualification
 * and failure contract. P=1 selects the exit program's configuration, not an
 * arbitrary caller bank. Other P values are rejected. The caller must qualify
 * the actual incoming P; this function does not read or change CPU registers. */
int hpclang_rpl_bank_plan_prepare_p(const struct hpclang_rpl_bank_sources *sources, uint32_t entry,
                                    unsigned p, struct hpclang_rpl_bank_workspace *workspace,
                                    struct hpclang_rpl_bank_plan *plan,
                                    hpclang_rpl_graph_dependency_fn dependency, void *context);
#endif
