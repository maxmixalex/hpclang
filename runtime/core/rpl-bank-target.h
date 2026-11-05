#ifndef HPCLANG_RPL_BANK_TARGET_H
#define HPCLANG_RPL_BANK_TARGET_H
#include "rpl-bank-plan.h"
#include "rpl-stock-view.h"

struct hpclang_rpl_bank_target
{
    struct hpclang_rpl_bank_plan plan;
    struct hpclang_rpl_reference_graph_node node;
};
struct hpclang_rpl_bank_target_workspace
{
    struct hpclang_rpl_bank_workspace planning;
    struct hpclang_rpl_bank_plan selected;
};
struct hpclang_rpl_bank_target_view
{
    struct hpclang_rpl_stock_view *view;
    const struct hpclang_rpl_bank_target *target;
};
/* Reader for subsequent traversal of the prepared target, bounded by its exact
 * object span. The graph caller observes these reads under node.mapping. */
int hpclang_rpl_bank_target_read(void *context, uint32_t address, uint8_t *value);
/* Resolve only the data target of a nonzero-switch ACPTR under the reviewed
 * P=0 convention. Sources must be the three Stock callbacks over this view;
 * retain all bank-plan qualifications and exclusive lifetime. The caller has
 * observed/qualified the source edge and supplies a fresh target mapping ID.
 * Payload parser reads are observed under that ID; plan reads retain source
 * IDs. Keep the returned plan alive and unchanged while traversing its node.
 * Separate EXECUTION/OPAQUE obligations are never discharged here. Zero means
 * a bounded data target, not complete reachability or mutation authorization.
 * Error preserves output and invalidates partial observations. Workspace is
 * caller-owned scratch, disjoint from inputs/output; no allocation or writes
 * to native state. Mutable Flash data uses a separate bounded reader and its
 * values remain dependencies, not authenticated code. MMIO is unavailable. */
int hpclang_rpl_bank_target_prepare(struct hpclang_rpl_stock_view *view,
                                    const struct hpclang_rpl_bank_sources *sources,
                                    const struct hpclang_rpl_reference *edge, uint32_t mapping,
                                    struct hpclang_rpl_bank_target_workspace *workspace,
                                    struct hpclang_rpl_bank_target *target,
                                    hpclang_rpl_graph_dependency_fn dependency, void *context);
#endif
