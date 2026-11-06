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
        if (use->generated)
            return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
        if (use->kind < HPCLANG_RPL_CODE_USE_LOAD_ADDRESS ||
            use->kind > HPCLANG_RPL_CODE_USE_STORE_ADDRESS)
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

static int payload_digit(struct hpclang_rpl_reference_edit_builder *builder, uint32_t token,
                         uint8_t expected, uint8_t replacement,
                         hpclang_rpl_code_origin_resolve_fn resolve,
                         hpclang_rpl_execution_payload_fn derived, void *context)
{
    if (!token)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_code_origin_cell cell;
    int result = resolve(context, token, &cell);
    if (result == HPCLANG_RPL_SPAN_UNSUPPORTED && derived) {
        result = derived(context, builder, token, expected, replacement);
        if (!result && builder->error)
            result = builder->error;
        if (!result && builder->count > builder->capacity)
            result = HPCLANG_RPL_SPAN_MALFORMED;
        return result > 0 ? HPCLANG_RPL_SPAN_MALFORMED : result;
    }
    if (result)
        return result > 0 ? HPCLANG_RPL_SPAN_MALFORMED : result;
    if (cell.expected != expected)
        return HPCLANG_RPL_SPAN_MALFORMED;
    const struct hpclang_rpl_reference_edit edit = {cell.mapping, cell.field, expected, replacement,
                                                    1};
    return hpclang_rpl_reference_edits_add(builder, &edit);
}

int hpclang_rpl_execution_plan_payload(struct hpclang_rpl_reference_edit_builder *builder,
                                       const struct hpclang_rpl_code_store *store,
                                       uint64_t replacement,
                                       hpclang_rpl_code_origin_resolve_fn resolve,
                                       hpclang_rpl_execution_payload_fn derived, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!store || !resolve || builder->count > builder->capacity ||
        (builder->capacity && !builder->edits))
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    if (store->first >= 16 || !store->nibbles || store->nibbles > 16 - store->first ||
        store->adjustment > 0xfffff)
        return reject(builder, HPCLANG_RPL_SPAN_BOUNDS);
    for (uint32_t i = 0; i < builder->count; ++i)
        if (builder->edits[i].width != 1)
            return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    int adjusted = store->adjustment && store->first < 5;
    if (adjusted) {
        uint32_t old = ((uint32_t)store->value - store->adjustment) & 0xfffff;
        uint32_t next = ((uint32_t)replacement - store->adjustment) & 0xfffff;
        for (unsigned i = 0; i < 5; ++i) {
            int result = payload_digit(builder, store->origins[i], (old >> (4 * i)) & 15,
                                       (next >> (4 * i)) & 15, resolve, derived, context);
            if (result)
                return reject(builder, result);
        }
    }
    for (unsigned i = store->first; i < store->first + store->nibbles; ++i) {
        if (adjusted && i < 5)
            continue;
        int result = payload_digit(builder, store->origins[i], (store->value >> (4 * i)) & 15,
                                   (replacement >> (4 * i)) & 15, resolve, derived, context);
        if (result)
            return reject(builder, result);
    }
    return 0;
}

static int subtract_value(const struct hpclang_rpl_execution_subtraction *expression, uint32_t left,
                          uint32_t right, uint32_t *value, uint32_t *carry)
{
    unsigned radix = expression->decimal ? 10 : 16, borrow = 0;
    uint32_t result = left;
    for (unsigned i = expression->first; i < expression->first + expression->nibbles; ++i) {
        unsigned a = (left >> (4 * i)) & 15, c = (right >> (4 * i)) & 15;
        if (a >= radix || c >= radix)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        int digit = (int)a - (int)c - (int)borrow;
        borrow = digit < 0;
        if (borrow)
            digit += radix;
        result = (result & ~(15u << (4 * i))) | ((uint32_t)digit << (4 * i));
    }
    *value = result;
    *carry = borrow;
    return 0;
}

int hpclang_rpl_execution_plan_subtraction(
    struct hpclang_rpl_reference_edit_builder *builder,
    const struct hpclang_rpl_execution_subtraction *expression, uint32_t left, uint32_t right,
    uint32_t replacement, hpclang_rpl_code_origin_resolve_fn resolve, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!expression || !resolve || expression->decimal > 1 || expression->carry > 1)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    if (expression->first >= 5 || !expression->nibbles ||
        expression->nibbles > 5 - expression->first || expression->left.value > 0xfffff ||
        expression->right.value > 0xfffff || expression->value > 0xfffff || left > 0xfffff ||
        right > 0xfffff || replacement > 0xfffff)
        return reject(builder, HPCLANG_RPL_SPAN_BOUNDS);
    uint32_t value, carry;
    int result =
        subtract_value(expression, expression->left.value, expression->right.value, &value, &carry);
    if (result)
        return reject(builder, result);
    if (value != expression->value || carry != expression->carry)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    result = subtract_value(expression, left, right, &value, &carry);
    if (result)
        return reject(builder, result);
    if (value != replacement)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    if (carry != expression->carry)
        return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
    if (expression->right.adjustment)
        return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
    uint32_t mask = ((1u << (4 * expression->nibbles)) - 1u) << (4 * expression->first);
    if ((right ^ expression->right.value) & ~mask)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    struct hpclang_rpl_code_step_uses uses = {.count = 2};
    const struct hpclang_rpl_execution_value *operands[] = {&expression->left, &expression->right};
    for (unsigned i = 0; i < 2; ++i) {
        uses.addresses[i].kind = HPCLANG_RPL_CODE_USE_LOAD_ADDRESS;
        uses.addresses[i].value = operands[i]->value;
        uses.addresses[i].adjustment = operands[i]->adjustment;
        for (unsigned digit = 0; digit < 5; ++digit)
            uses.addresses[i].origins[digit] = operands[i]->origins[digit];
    }
    /* Pad the five-digit source planner with the already-required left source
     * cells. Unused right bits are not dependencies of a partial subtraction. */
    uint32_t left_source = (expression->left.value - expression->left.adjustment) & 0xfffffu;
    uint32_t left_replacement = (left - expression->left.adjustment) & 0xfffffu;
    for (unsigned digit = 0; digit < 5; ++digit)
        if (!(mask & (15u << (4 * digit))))
            uses.addresses[1].origins[digit] = expression->left.origins[digit];
    uses.addresses[1].value = (left_source & ~mask) | (expression->right.value & mask);
    const uint32_t replacements[] = {left, (left_replacement & ~mask) | (right & mask)};
    return hpclang_rpl_execution_plan_uses(builder, &uses, replacements, resolve, context);
}
