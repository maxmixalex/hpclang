#include "rpl-bank-target.h"
#include "rpl-stock-flash-data.h"

struct payload_reader
{
    struct hpclang_rpl_stock_mapped_view mapped;
    uint32_t first, end, mapping;
    hpclang_rpl_graph_dependency_fn dependency;
    void *context;
    int error;
};

static int mapped_read(struct hpclang_rpl_stock_mapped_view *mapped, uint32_t address,
                       uint8_t *value)
{
    uint32_t page = mapped->pages[address >> 12];
    if (!(page & 0x7ffu) && page >= 0x140000u && page <= 0x1ff800u)
        return hpclang_rpl_stock_flash_data_read(mapped->view, page * 2u + (address & 0xfffu),
                                                 value);
    return hpclang_rpl_stock_view_mapped(mapped, address, value);
}

int hpclang_rpl_bank_target_read(void *context, uint32_t address, uint8_t *value)
{
    const struct hpclang_rpl_bank_target_view *view = context;
    if (!view || !view->target || !value || address >= 0x100000u)
        return 0;
    const struct hpclang_rpl_reference_node *node = &view->target->node.object;
    if (address < node->address || address - node->address >= node->limit)
        return 0;
    struct hpclang_rpl_stock_mapped_view mapped = {view->view,
                                                   view->target->plan.effects.map.pages};
    return mapped_read(&mapped, address, value);
}

static int payload_read(void *context, uint32_t address, uint8_t *value)
{
    struct payload_reader *reader = context;
    if (reader->error)
        return 0;
    if (address < reader->first || address >= reader->end) {
        reader->error = HPCLANG_RPL_SPAN_BOUNDS;
        return 0;
    }
    if (!mapped_read(&reader->mapped, address, value))
        return 0;
    reader->error = reader->dependency(reader->context, reader->mapping, address, *value);
    if (reader->error > 0)
        reader->error = HPCLANG_RPL_SPAN_MALFORMED;
    return !reader->error;
}

int hpclang_rpl_bank_target_prepare(struct hpclang_rpl_stock_view *view,
                                    const struct hpclang_rpl_bank_sources *sources,
                                    const struct hpclang_rpl_reference *edge, uint32_t mapping,
                                    struct hpclang_rpl_bank_target_workspace *workspace,
                                    struct hpclang_rpl_bank_target *target,
                                    hpclang_rpl_graph_dependency_fn dependency, void *context)
{
    if (!view || !sources || !edge || !workspace || !target || !dependency || !sources->saturn ||
        !sources->native || !sources->rom)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (edge->kind != HPCLANG_RPL_REFERENCE_ACPTR || !edge->auxiliary || edge->extent ||
        mapping == sources->saturn_mapping || mapping == sources->native_mapping ||
        mapping == sources->rom_mapping || sources->saturn->context != view ||
        sources->native->context != view || sources->rom->context != view ||
        sources->saturn->read_nibble != hpclang_rpl_stock_view_saturn ||
        sources->native->read_nibble != hpclang_rpl_stock_view_native ||
        sources->rom->read_nibble != hpclang_rpl_stock_view_rom)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    int result = hpclang_rpl_bank_plan_prepare(sources, edge->auxiliary, &workspace->planning,
                                               &workspace->selected, dependency, context);
    if (result)
        return result;
    const struct hpclang_rpl_bank_plan *plan = &workspace->selected;
    if (!plan->geometry.bytes || plan->geometry.bytes > 0x20000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t end = 0x40000u + plan->geometry.bytes * 2u;
    if (edge->target < 0x40000u || edge->target >= end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct payload_reader observed = {
        {view, plan->effects.map.pages}, edge->target, end, mapping, dependency, context, 0};
    const struct hpclang_rpl_reader reader = {payload_read, &observed};
    uint32_t span;
    result = hpclang_rpl_object_span(&reader, edge->target, end - edge->target, &span);
    if (observed.error)
        return observed.error;
    if (result)
        return result;
    target->plan = *plan;
    target->node = (struct hpclang_rpl_reference_graph_node){mapping, {edge->target, span, 0}};
    return 0;
}
