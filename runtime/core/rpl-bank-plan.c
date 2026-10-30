#include "rpl-bank-plan.h"
#include "rpl-execution-map.h"

struct observation
{
    hpclang_rpl_graph_dependency_fn dependency;
    void *context;
    uint32_t mapping;
};
static int observe(void *context, uint32_t address, uint8_t value)
{
    const struct observation *observation = context;
    return observation->dependency(observation->context, observation->mapping, address, value);
}

int hpclang_rpl_bank_plan_prepare(const struct hpclang_rpl_bank_sources *sources, uint32_t entry,
                                  struct hpclang_rpl_bank_workspace *workspace,
                                  struct hpclang_rpl_bank_plan *plan,
                                  hpclang_rpl_graph_dependency_fn dependency, void *context)
{
    if (!sources || !workspace || !plan || !dependency || !sources->profile || !sources->saturn ||
        !sources->saturn->read_nibble || !sources->native || !sources->native->read_nibble ||
        !sources->rom || !sources->rom->read_nibble)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (sources->saturn_mapping == sources->native_mapping ||
        sources->saturn_mapping == sources->rom_mapping ||
        sources->native_mapping == sources->rom_mapping)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct observation saturn = {dependency, context, sources->saturn_mapping};
    struct observation native = {dependency, context, sources->native_mapping};
    struct observation rom = {dependency, context, sources->rom_mapping};
    struct hpclang_rpl_bank_plan *next = &workspace->next;
    int result =
        hpclang_rpl_code_bank_entry_inspect(sources->saturn, sources->code_first, sources->code_end,
                                            entry, 0, &next->program, observe, &saturn);
    if (result)
        return result;
    result = hpclang_rpl_flashptr_bank_guarded(sources->saturn, next->program.selector,
                                               sources->configurations, 0, &next->bank, observe,
                                               &saturn);
    if (result != 1)
        return result < 0 ? result : HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t configuration = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint32_t at = next->program.selector + i;
        uint8_t value;
        if (!sources->saturn->read_nibble(sources->saturn->context, at, &value))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (value > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result = observe(&saturn, at, value);
        if (result)
            return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        configuration |= (uint32_t)value << (4u * i);
    }
    /* Table identity and the reviewed hardware control bits must agree. */
    if (((configuration >> 1) & 15u) != next->bank)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    next->configuration = configuration;
    struct hpclang_rpl_stock_bank_route route;
    result = hpclang_rpl_stock_bank_route_capture(sources->native, &route, observe, &native);
    if (result)
        return result;
    result = hpclang_rpl_stock_bank_route_check(&route, next->program.selector, configuration);
    if (result)
        return result;
    result = hpclang_rpl_rom_bank_capture_guarded(
        sources->rom, sources->native, sources->profile->geometry_operands,
        sources->profile->geometry_bases, sources->profile->flash_bytes, workspace->banks, observe,
        &rom, observe, &native);
    if (result)
        return result;
    result = hpclang_rpl_bank_effects_capture(sources->native, &workspace->prior, observe, &native);
    if (result)
        return result;
    result = hpclang_rpl_bank_effects_select(&workspace->prior, workspace->banks,
                                             (configuration >> 5) & 3u, next->bank, &next->effects);
    if (result)
        return result;
    /* CONFIG must not remap the inspected body before its return. */
    result = hpclang_rpl_execution_mapping_preserved(
        workspace->prior.map.pages, next->effects.map.pages, next->program.body, 29);
    if (result)
        return result;
    next->geometry = workspace->banks[next->bank];
    *plan = *next;
    return 0;
}
