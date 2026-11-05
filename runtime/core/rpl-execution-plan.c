#include "rpl-execution.h"

static int reject(struct hpclang_rpl_reference_edit_builder *builder, int error)
{
    builder->count = 0;
    builder->error = error;
    return error;
}

int hpclang_rpl_execution_plan_uses(struct hpclang_rpl_reference_edit_builder *builder,
                                    const struct hpclang_rpl_code_step_uses *uses,
                                    const uint32_t *replacements,
                                    hpclang_rpl_code_origin_resolve_fn resolve, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!uses || !resolve || uses->count > 4 || (uses->count && !replacements) ||
        builder->count > builder->capacity || (builder->capacity && !builder->edits))
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    for (uint32_t i = 0; i < builder->count; ++i)
        if (builder->edits[i].width != 1)
            return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    for (uint32_t i = 0; i < uses->count; ++i) {
        const struct hpclang_rpl_code_address_use *use = &uses->addresses[i];
        if (use->kind < HPCLANG_RPL_CODE_USE_LOAD_ADDRESS ||
            use->kind > HPCLANG_RPL_CODE_USE_RETURN_TARGET)
            return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
        if (use->adjustment > 0xfffff || use->value > 0xfffff || replacements[i] > 0xfffff)
            return reject(builder, HPCLANG_RPL_SPAN_BOUNDS);
        struct hpclang_rpl_reference_edit edits[5];
        unsigned count;
        int result = hpclang_rpl_code_origins_plan(
            use->origins, (use->value - use->adjustment) & 0xfffffu,
            (replacements[i] - use->adjustment) & 0xfffffu, resolve, context, edits, &count);
        if (result)
            return reject(builder, result);
        for (unsigned j = 0; j < count; ++j)
            for (uint32_t digit = 0; digit < edits[j].width; ++digit) {
                const struct hpclang_rpl_reference_edit cell = {
                    edits[j].mapping, edits[j].field + digit,
                    (edits[j].expected >> (4 * digit)) & 15, (edits[j].value >> (4 * digit)) & 15,
                    1};
                result = hpclang_rpl_reference_edits_add(builder, &cell);
                if (result)
                    return result;
            }
    }
    return 0;
}
