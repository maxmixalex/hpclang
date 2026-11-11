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
        builder->packed > 1 || builder->count > builder->capacity ||
        (builder->capacity && !builder->edits))
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    for (uint32_t i = 0; i < builder->count; ++i)
        if (!builder->edits[i].width || builder->edits[i].width > (builder->packed ? 5u : 1u))
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
    if (!store || !resolve || builder->packed > 1 || builder->count > builder->capacity ||
        (builder->capacity && !builder->edits))
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    if (store->first >= 16 || !store->nibbles || store->nibbles > 16 - store->first ||
        store->adjustment > 0xfffff)
        return reject(builder, HPCLANG_RPL_SPAN_BOUNDS);
    for (uint32_t i = 0; i < builder->count; ++i)
        if (!builder->edits[i].width || builder->edits[i].width > (builder->packed ? 5u : 1u))
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

static unsigned predicate_result(unsigned relation, uint64_t left, uint64_t right)
{
    switch (relation) {
    case HPCLANG_RPL_CODE_EQUAL:
        return left == right;
    case HPCLANG_RPL_CODE_UNEQUAL:
        return left != right;
    case HPCLANG_RPL_CODE_GREATER:
        return left > right;
    case HPCLANG_RPL_CODE_LESS:
        return left < right;
    case HPCLANG_RPL_CODE_GREATER_EQUAL:
        return left >= right;
    default:
        return left <= right;
    }
}

int hpclang_rpl_execution_plan_predicate(struct hpclang_rpl_reference_edit_builder *builder,
                                         const struct hpclang_rpl_code_predicate *predicate,
                                         const struct hpclang_rpl_code_execution *before,
                                         const uint64_t replacements[2],
                                         hpclang_rpl_code_origin_resolve_fn resolve,
                                         hpclang_rpl_execution_payload_fn derived, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!predicate || !before || !replacements || !resolve || predicate->first >= 16 ||
        !predicate->nibbles || predicate->nibbles > 16 - predicate->first ||
        predicate->left > HPCLANG_RPL_CODE_D ||
        (predicate->right > HPCLANG_RPL_CODE_D && predicate->right != HPCLANG_RPL_CODE_CONSTANT) ||
        predicate->relation > HPCLANG_RPL_CODE_LESS_EQUAL || predicate->taken > 1 ||
        predicate->displacement > 255 || predicate->instruction > 0xffffb)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    unsigned regs[2] = {predicate->left, predicate->right};
    uint64_t old[2], next[2];
    uint64_t mask =
        predicate->nibbles == 16 ? UINT64_MAX : (UINT64_C(1) << (4 * predicate->nibbles)) - 1;
    for (unsigned i = 0; i < 2; ++i) {
        if (regs[i] == HPCLANG_RPL_CODE_CONSTANT) {
            if (replacements[i])
                return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
            old[i] = next[i] = 0;
        } else {
            old[i] = (before->values.registers[regs[i]] >> (4 * predicate->first)) & mask;
            next[i] = (replacements[i] >> (4 * predicate->first)) & mask;
        }
    }
    if (old[0] != predicate->left_value || old[1] != predicate->right_value ||
        predicate_result(predicate->relation, old[0], old[1]) != predicate->taken)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    if (predicate_result(predicate->relation, next[0], next[1]) != predicate->taken)
        return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
    for (unsigned i = 0; i < 2; ++i) {
        if (regs[i] == HPCLANG_RPL_CODE_CONSTANT)
            continue;
        struct hpclang_rpl_code_store operand = {.first = predicate->first,
                                                 .nibbles = predicate->nibbles,
                                                 .value = before->values.registers[regs[i]],
                                                 .adjustment = before->origins.adjustment[regs[i]]};
        for (unsigned digit = 0; digit < 16; ++digit)
            operand.origins[digit] = before->origins.registers[regs[i]][digit];
        int result = hpclang_rpl_execution_plan_payload(builder, &operand, replacements[i], resolve,
                                                        derived, context);
        if (result)
            return result;
    }
    return 0;
}

int hpclang_rpl_execution_plan_constant(struct hpclang_rpl_reference_edit_builder *builder,
                                        const struct hpclang_rpl_code_constant *constant,
                                        unsigned digit, uint8_t expected, uint8_t replacement,
                                        const struct hpclang_rpl_reader *instructions,
                                        hpclang_rpl_code_token_fn token,
                                        hpclang_rpl_code_origin_resolve_fn resolve,
                                        hpclang_rpl_execution_payload_fn derived, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!constant || !instructions || !instructions->read_nibble || !token || !resolve ||
        constant->assignment.instruction >= 0x100000 || constant->p > 15 || constant->p_adjustment)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    const struct hpclang_rpl_code_assignment *fact = &constant->assignment;
    struct hpclang_rpl_code_assignment original, alternate;
    int result = hpclang_rpl_code_assignment_inspect(
        instructions, fact->instruction, 0x100000 - fact->instruction, constant->p, &original);
    if (result)
        return reject(builder, result);
    if (original.source != HPCLANG_RPL_CODE_CONSTANT || original.length != fact->length ||
        original.source != fact->source || original.destination != fact->destination ||
        original.source_first != fact->source_first ||
        original.destination_first != fact->destination_first ||
        original.nibbles != fact->nibbles || original.constant != fact->constant ||
        digit >= original.nibbles)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    uint8_t value = ((uint64_t)original.constant >> (4 * digit)) & 15;
    if (expected != value || replacement != value)
        return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
    result = hpclang_rpl_code_assignment_inspect(
        instructions, fact->instruction, 0x100000 - fact->instruction, constant->p ^ 1, &alternate);
    if (result)
        return reject(builder, result);
    if (alternate.destination_first != original.destination_first ||
        alternate.nibbles != original.nibbles) {
        result = payload_digit(builder, constant->p_origin, constant->p, constant->p, resolve,
                               derived, context);
        if (result)
            return reject(builder, result);
    }
    for (unsigned i = 0; i < original.length; ++i) {
        uint8_t nibble;
        uint32_t origin;
        if (!instructions->read_nibble(instructions->context, fact->instruction + i, &nibble))
            return reject(builder, HPCLANG_RPL_SPAN_UNAVAILABLE);
        if (nibble > 15)
            return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
        result = token(context, 0, fact->instruction + i, nibble, &origin);
        if (result)
            return reject(builder, result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED);
        result = payload_digit(builder, origin, nibble, nibble, resolve, derived, context);
        if (result)
            return reject(builder, result);
    }
    return 0;
}

int hpclang_rpl_execution_plan_native_word(struct hpclang_rpl_reference_edit_builder *builder,
                                           const struct hpclang_rpl_code_native_word *word,
                                           uint32_t input, uint32_t output,
                                           const struct hpclang_rpl_reader *instructions,
                                           hpclang_rpl_code_token_fn token,
                                           hpclang_rpl_code_origin_resolve_fn resolve,
                                           hpclang_rpl_execution_payload_fn derived, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!word || !instructions || !instructions->read_nibble || !token || !resolve ||
        word->instruction > 0xffffc || word->adjustment > 0xfffff ||
        word->after != word->before + (uint32_t)word->delta ||
        output != input + (uint32_t)word->delta)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    uint32_t opcode;
    if (word->reg == HPCLANG_RPL_CODE_B && word->delta == 5)
        opcode = 0xeb18;
    else if ((word->reg == HPCLANG_RPL_CODE_B && word->delta == -5) ||
             (word->reg == HPCLANG_RPL_CODE_D && word->delta == 1))
        opcode = 0xdb18;
    else
        return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
    for (unsigned digit = 0; digit < 4; ++digit) {
        uint8_t value;
        uint32_t origin;
        if (!instructions->read_nibble(instructions->context, word->instruction + digit, &value))
            return reject(builder, HPCLANG_RPL_SPAN_UNAVAILABLE);
        if (value != ((opcode >> (4 * digit)) & 15))
            return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
        int result = token(context, 0, word->instruction + digit, value, &origin);
        if (result)
            return reject(builder, result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED);
        result = payload_digit(builder, origin, value, value, resolve, derived, context);
        if (result)
            return reject(builder, result);
    }
    struct hpclang_rpl_code_store operand = {
        .nibbles = 8, .value = word->before, .adjustment = word->adjustment};
    for (unsigned digit = 0; digit < 8; ++digit)
        operand.origins[digit] = word->origins[digit];
    return hpclang_rpl_execution_plan_payload(builder, &operand, input, resolve, derived, context);
}

int hpclang_rpl_execution_plan_unit(struct hpclang_rpl_reference_edit_builder *builder,
                                    const struct hpclang_rpl_code_unit *unit, uint64_t input,
                                    uint64_t output, const struct hpclang_rpl_reader *instructions,
                                    hpclang_rpl_code_token_fn token,
                                    hpclang_rpl_code_origin_resolve_fn resolve,
                                    hpclang_rpl_execution_payload_fn derived, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!unit || !instructions || !instructions->read_nibble || !token || !resolve ||
        unit->reg > HPCLANG_RPL_CODE_D || unit->instruction >= 0x100000 || unit->p_adjustment)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    struct hpclang_rpl_code_values values = {.decimal = unit->decimal};
    values.registers[unit->reg] = unit->before;
    values.registers[HPCLANG_RPL_CODE_P] = unit->p;
    struct hpclang_rpl_code_unit original, next, alternate;
    int result = hpclang_rpl_code_unit_inspect(instructions, unit->instruction,
                                               0x100000 - unit->instruction, &values, &original);
    if (result)
        return reject(builder, result);
    if (original.length != unit->length || original.reg != unit->reg ||
        original.first != unit->first || original.nibbles != unit->nibbles ||
        original.decrement != unit->decrement || original.after != unit->after ||
        original.carry != unit->carry)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    values.registers[unit->reg] = input;
    result = hpclang_rpl_code_unit_inspect(instructions, unit->instruction,
                                           0x100000 - unit->instruction, &values, &next);
    if (result)
        return reject(builder, result);
    if (next.after != output)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    if (next.carry != original.carry)
        return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
    values.registers[HPCLANG_RPL_CODE_P] ^= 1;
    /* A zero input is valid in both radices and lets field selection be inspected
     * independently of unrelated noncanonical digits outside the original field. */
    values.registers[unit->reg] = 0;
    result = hpclang_rpl_code_unit_inspect(instructions, unit->instruction,
                                           0x100000 - unit->instruction, &values, &alternate);
    if (result)
        return reject(builder, result);
    if (alternate.first != original.first || alternate.nibbles != original.nibbles) {
        result =
            payload_digit(builder, unit->p_origin, unit->p, unit->p, resolve, derived, context);
        if (result)
            return reject(builder, result);
    }
    for (unsigned i = 0; i < original.length; ++i) {
        uint8_t nibble;
        uint32_t origin;
        if (!instructions->read_nibble(instructions->context, unit->instruction + i, &nibble))
            return reject(builder, HPCLANG_RPL_SPAN_UNAVAILABLE);
        if (nibble > 15)
            return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
        result = token(context, 0, unit->instruction + i, nibble, &origin);
        if (result)
            return reject(builder, result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED);
        result = payload_digit(builder, origin, nibble, nibble, resolve, derived, context);
        if (result)
            return reject(builder, result);
    }
    struct hpclang_rpl_code_store operand = {.first = unit->first,
                                             .nibbles = unit->nibbles,
                                             .value = unit->before,
                                             .adjustment = unit->adjustment};
    for (unsigned i = 0; i < 16; ++i)
        operand.origins[i] = unit->origins[i];
    return hpclang_rpl_execution_plan_payload(builder, &operand, input, resolve, derived, context);
}

int hpclang_rpl_execution_plan_sum(struct hpclang_rpl_reference_edit_builder *builder,
                                   const struct hpclang_rpl_code_sum_source *fact,
                                   const uint64_t inputs[2], uint64_t output,
                                   const struct hpclang_rpl_reader *instructions,
                                   hpclang_rpl_code_token_fn token,
                                   hpclang_rpl_code_origin_resolve_fn resolve,
                                   hpclang_rpl_execution_payload_fn derived, void *context)
{
    if (!builder)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (builder->error)
        return builder->error;
    if (!fact || !inputs || !instructions || !instructions->read_nibble || !token || !resolve)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    const struct hpclang_rpl_code_sum *sum = &fact->sum;
    if (sum->left > HPCLANG_RPL_CODE_D || sum->right > HPCLANG_RPL_CODE_D ||
        sum->instruction >= 0x100000 || fact->p_adjustment ||
        (sum->left == sum->right &&
         (sum->left_value != sum->right_value || inputs[0] != inputs[1])))
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    struct hpclang_rpl_code_values values = {.decimal = sum->decimal};
    values.registers[sum->left] = sum->left_value;
    values.registers[sum->right] = sum->right_value;
    values.registers[HPCLANG_RPL_CODE_P] = sum->p;
    struct hpclang_rpl_code_sum original, next, alternate;
    int result = hpclang_rpl_code_sum_inspect(instructions, sum->instruction,
                                              0x100000 - sum->instruction, &values, &original);
    if (result)
        return reject(builder, result);
    if (original.left != sum->left || original.right != sum->right ||
        original.destination != sum->destination || original.first != sum->first ||
        original.nibbles != sum->nibbles || original.length != sum->length ||
        original.result != sum->result || original.carry != sum->carry)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    values.registers[sum->left] = inputs[0];
    values.registers[sum->right] = inputs[1];
    result = hpclang_rpl_code_sum_inspect(instructions, sum->instruction,
                                          0x100000 - sum->instruction, &values, &next);
    if (result)
        return reject(builder, result);
    if (next.result != output)
        return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
    if (next.carry != original.carry)
        return reject(builder, HPCLANG_RPL_SPAN_UNSUPPORTED);
    values.registers[sum->left] = values.registers[sum->right] = 0;
    values.registers[HPCLANG_RPL_CODE_P] ^= 1;
    result = hpclang_rpl_code_sum_inspect(instructions, sum->instruction,
                                          0x100000 - sum->instruction, &values, &alternate);
    if (result)
        return reject(builder, result);
    if (alternate.first != original.first || alternate.nibbles != original.nibbles) {
        result = payload_digit(builder, fact->p_origin, sum->p, sum->p, resolve, derived, context);
        if (result)
            return reject(builder, result);
    }
    for (unsigned i = 0; i < sum->length; ++i) {
        uint8_t nibble;
        uint32_t origin;
        if (!instructions->read_nibble(instructions->context, sum->instruction + i, &nibble))
            return reject(builder, HPCLANG_RPL_SPAN_UNAVAILABLE);
        if (nibble > 15)
            return reject(builder, HPCLANG_RPL_SPAN_MALFORMED);
        result = token(context, 0, sum->instruction + i, nibble, &origin);
        if (result)
            return reject(builder, result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED);
        result = payload_digit(builder, origin, nibble, nibble, resolve, derived, context);
        if (result)
            return reject(builder, result);
    }
    for (unsigned side = 0; side < 2; ++side) {
        struct hpclang_rpl_code_store operand = {.first = sum->first,
                                                 .nibbles = sum->nibbles,
                                                 .value = side ? sum->right_value : sum->left_value,
                                                 .adjustment = fact->adjustment[side]};
        for (unsigned i = 0; i < 16; ++i)
            operand.origins[i] = fact->origins[side][i];
        result = hpclang_rpl_execution_plan_payload(builder, &operand, inputs[side], resolve,
                                                    derived, context);
        if (result)
            return reject(builder, result);
    }
    return 0;
}
