#include "rpl-code.h"

static int read_word(const struct hpclang_rpl_reader *reader, uint32_t at, unsigned width,
                     uint32_t *value);

static uint64_t data_mask(unsigned nibbles)
{
    return nibbles == 16 ? UINT64_MAX : (UINT64_C(1) << (4 * nibbles)) - 1;
}

static int scan_word(const struct hpclang_rpl_reader *data, uint32_t address, uint32_t limit,
                     uint32_t *reads, uint32_t *value, uint32_t kind,
                     hpclang_rpl_code_scan_read_fn observe, void *context)
{
    if (*reads >= limit || address > 0xffffb)
        return HPCLANG_RPL_SPAN_BOUNDS;
    int result = read_word(data, address, 5, value);
    if (!result && observe) {
        const struct hpclang_rpl_code_scan_read read = {kind, address, *value};
        result = observe(context, &read);
        if (result > 0)
            result = HPCLANG_RPL_SPAN_MALFORMED;
    }
    if (!result)
        ++*reads;
    return result;
}

int hpclang_rpl_code_native_scan_observed(const struct hpclang_rpl_reader *instructions,
                                          const struct hpclang_rpl_reader *data, uint32_t limit,
                                          const struct hpclang_rpl_code_execution *state,
                                          uint32_t max_reads, hpclang_rpl_code_scan_read_fn observe,
                                          void *context,
                                          struct hpclang_rpl_code_native_scan *output)
{
    if (!instructions || !instructions->read_nibble || !data || !data->read_nibble || !state ||
        !output)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    uint32_t pc = state->control.pc;
    if (pc > 0xffffc || limit < 4 || limit > 0x100000 - pc || !max_reads)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (state->values.registers[HPCLANG_RPL_CODE_P])
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (state->values.registers[HPCLANG_RPL_CODE_D0] > 0xfffff ||
        state->values.registers[HPCLANG_RPL_CODE_D1] > 0xfffff || state->control.cursor >= 8 ||
        state->control.carry > 1 || state->values.carry != state->control.carry ||
        state->values.decimal > 1)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t opcode;
    int result = read_word(instructions, pc, 4, &opcode);
    if (result)
        return result;
    if (opcode != 0xab18)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_code_native_scan next = {
        .b = state->values.registers[HPCLANG_RPL_CODE_B],
        .instruction = pc,
        .d0 = (uint32_t)state->values.registers[HPCLANG_RPL_CODE_D0],
        .d1 = (uint32_t)state->values.registers[HPCLANG_RPL_CODE_D1],
        .saved_b = (uint32_t)state->values.registers[HPCLANG_RPL_CODE_B],
        .saved_d1 = (uint32_t)state->values.registers[HPCLANG_RPL_CODE_D1],
        .cursor = state->control.cursor,
        .carry = state->control.carry};
    uint32_t depth = 0, pointer = (uint32_t)state->values.registers[HPCLANG_RPL_CODE_A] & 0xfffff;
    unsigned from_a = (state->status & 2) != 0;
    for (;;) {
        if (!from_a) {
            /* Native increments the full word. Do not admit an intermediate
             * D0 outside this evaluator's canonical 20-bit contract. */
            if (next.d0 > 0xffffa)
                return HPCLANG_RPL_SPAN_BOUNDS;
            result = scan_word(data, next.d0, max_reads, &next.reads, &pointer,
                               HPCLANG_RPL_CODE_SCAN_POINTER, observe, context);
            if (result)
                return result;
            next.d0 += 5;
        }
        from_a = 0;
        uint32_t tag;
        result = scan_word(data, pointer, max_reads, &next.reads, &tag, HPCLANG_RPL_CODE_SCAN_TAG,
                           observe, context);
        if (result)
            return result;
        if (tag == 0x28fc) {
            if (pointer < 5)
                return HPCLANG_RPL_SPAN_BOUNDS;
            result = scan_word(data, pointer - 5, max_reads, &next.reads, &next.target,
                               HPCLANG_RPL_CODE_SCAN_TARGET, observe, context);
            if (result)
                return result;
            if (next.target != 0x3077) {
                next.b = (next.b & UINT64_C(0xffffffff00000000)) | depth;
                next.d1 = pointer;
                *output = next;
                return 0;
            }
            if (depth == UINT32_MAX)
                return HPCLANG_RPL_SPAN_BOUNDS;
            ++depth;
            continue;
        }
        if (tag == 0x3130) {
            if (!depth) {
                if (next.d0 < 5)
                    return HPCLANG_RPL_SPAN_BOUNDS;
                next.d0 -= 5;
                break;
            }
            --depth;
        }
        if (!depth)
            break;
    }
    next.cursor = (next.cursor + 7) & 7;
    next.target = state->control.stack[next.cursor];
    if (next.target > 0xfffff)
        return HPCLANG_RPL_SPAN_MALFORMED;
    next.carry = 0;
    next.returns = 1;
    *output = next;
    return 0;
}

int hpclang_rpl_code_native_scan_inspect(const struct hpclang_rpl_reader *instructions,
                                         const struct hpclang_rpl_reader *data, uint32_t limit,
                                         const struct hpclang_rpl_code_execution *state,
                                         uint32_t max_reads,
                                         struct hpclang_rpl_code_native_scan *output)
{
    return hpclang_rpl_code_native_scan_observed(instructions, data, limit, state, max_reads, 0, 0,
                                                 output);
}

int hpclang_rpl_code_predicate_inspect(const struct hpclang_rpl_reader *reader,
                                       uint32_t instruction, uint32_t limit,
                                       const struct hpclang_rpl_code_values *values,
                                       struct hpclang_rpl_code_predicate *predicate)
{
    if (!reader || !reader->read_nibble || !values || !predicate)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000 || !limit || limit > 0x100000 - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 1, &opcode);
    if (result)
        return result;
    if (opcode != 9 && opcode != 8)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (opcode == 8) {
        if (limit < 2)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t kind;
        result = read_word(reader, instruction + 1, 1, &kind);
        if (result)
            return result;
        if (kind != 10 && kind != 11)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    if (limit < 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t encoded;
    result = read_word(reader, instruction + 1, 4, &encoded);
    if (result)
        return result;
    if (values->registers[HPCLANG_RPL_CODE_P] > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    unsigned field = encoded & 7, operation = (encoded >> 4) & 15;
    unsigned p = (unsigned)values->registers[HPCLANG_RPL_CODE_P];
    static const unsigned firsts[8] = {0, 0, 2, 0, 15, 3, 0, 0};
    static const unsigned widths[8] = {1, 1, 1, 3, 1, 12, 2, 16};
    static const unsigned lefts[4] = {0, 1, 2, 3}, rights[4] = {1, 2, 0, 2};
    struct hpclang_rpl_code_predicate next = {.instruction = instruction,
                                              .first = opcode == 8 ? 0
                                                       : field     ? firsts[field]
                                                                   : p,
                                              .nibbles = opcode == 8  ? 5
                                                         : field == 1 ? p + 1
                                                                      : widths[field],
                                              .left = lefts[operation & 3],
                                              .right = rights[operation & 3],
                                              .displacement = encoded >> 8};
    if (opcode == 8 ? (encoded & 15) == 11 : (encoded & 8) != 0)
        next.relation = HPCLANG_RPL_CODE_GREATER + operation / 4;
    else {
        next.relation = (operation & 4) ? HPCLANG_RPL_CODE_UNEQUAL : HPCLANG_RPL_CODE_EQUAL;
        if (operation & 8)
            next.right = HPCLANG_RPL_CODE_CONSTANT;
    }
    next.left_value = (values->registers[next.left] >> (4 * next.first)) & data_mask(next.nibbles);
    next.right_value =
        next.right == HPCLANG_RPL_CODE_CONSTANT
            ? 0
            : (values->registers[next.right] >> (4 * next.first)) & data_mask(next.nibbles);
    switch (next.relation) {
    case HPCLANG_RPL_CODE_EQUAL:
        next.taken = next.left_value == next.right_value;
        break;
    case HPCLANG_RPL_CODE_UNEQUAL:
        next.taken = next.left_value != next.right_value;
        break;
    case HPCLANG_RPL_CODE_GREATER:
        next.taken = next.left_value > next.right_value;
        break;
    case HPCLANG_RPL_CODE_LESS:
        next.taken = next.left_value < next.right_value;
        break;
    case HPCLANG_RPL_CODE_GREATER_EQUAL:
        next.taken = next.left_value >= next.right_value;
        break;
    default:
        next.taken = next.left_value <= next.right_value;
        break;
    }
    *predicate = next;
    return 0;
}

int hpclang_rpl_code_sum_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                 uint32_t limit, const struct hpclang_rpl_code_values *values,
                                 struct hpclang_rpl_code_sum *sum)
{
    if (!reader || !reader->read_nibble || !values || !sum)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000 || !limit || limit > 0x100000 - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int status = read_word(reader, instruction, 1, &opcode);
    if (status)
        return status;
    if (opcode != 12 && opcode != 10)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned length = opcode == 12 ? 2 : 3;
    if (limit < length)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t tail;
    status = read_word(reader, instruction + 1, length - 1, &tail);
    if (status)
        return status;
    unsigned field = opcode == 12 ? 8 : tail & 15;
    unsigned operation = opcode == 12 ? tail : tail >> 4;
    if (operation >= 12 || (opcode == 10 && field >= 8))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (values->registers[HPCLANG_RPL_CODE_P] > 15 || values->decimal > 1)
        return HPCLANG_RPL_SPAN_MALFORMED;
    static const unsigned lefts[12] = {0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 0, 2};
    static const unsigned rights[12] = {1, 2, 0, 2, 0, 1, 2, 3, 0, 1, 2, 3};
    static const unsigned firsts[9] = {0, 0, 2, 0, 15, 3, 0, 0, 0};
    static const unsigned widths[9] = {1, 1, 1, 3, 1, 12, 2, 16, 5};
    unsigned p = (unsigned)values->registers[HPCLANG_RPL_CODE_P];
    struct hpclang_rpl_code_sum next = {.instruction = instruction,
                                        .length = length,
                                        .left = lefts[operation],
                                        .right = rights[operation],
                                        .destination = lefts[operation],
                                        .first = field ? firsts[field] : p,
                                        .nibbles = field == 1 ? p + 1 : widths[field],
                                        .decimal = values->decimal,
                                        .p = p};
    next.left_value = values->registers[next.left];
    next.right_value = values->registers[next.right];
    next.result = next.left_value;
    unsigned radix = next.decimal ? 10 : 16;
    for (unsigned i = next.first; i < next.first + next.nibbles; ++i) {
        unsigned left = (next.left_value >> (4 * i)) & 15;
        unsigned right = (next.right_value >> (4 * i)) & 15;
        if (left >= radix || right >= radix)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        unsigned digit = left + right + next.carry;
        next.carry = digit >= radix;
        if (next.carry)
            digit -= radix;
        next.result = (next.result & ~(UINT64_C(15) << (4 * i))) | ((uint64_t)digit << (4 * i));
    }
    *sum = next;
    return 0;
}

int hpclang_rpl_code_unit_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                  uint32_t limit, const struct hpclang_rpl_code_values *values,
                                  struct hpclang_rpl_code_unit *unit)
{
    if (!reader || !reader->read_nibble || !values || !unit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000 || !limit || limit > 0x100000 - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 1, &opcode);
    if (result)
        return result;
    if (opcode != 8 && opcode != 10 && opcode != 11)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (limit < 3)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t fields;
    result = read_word(reader, instruction + 1, 2, &fields);
    if (result)
        return result;
    unsigned field = fields & 15, operation = fields >> 4, decrement = opcode == 10;
    unsigned length = 3, amount = 1, reg;
    if (opcode == 8) {
        if (fields != 0x81)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (limit < 6)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(reader, instruction + 3, 3, &fields);
        if (result)
            return result;
        field = fields & 15;
        operation = (fields >> 4) & 15;
        if ((field >= 8 && field != 15) || (operation & 7) > 3)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        decrement = operation >> 3;
        reg = operation & 3;
        amount = (fields >> 8) + 1;
        length = 6;
    } else {
        if (field >= 8 || (decrement ? operation < 12 : operation < 4 || operation > 7))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        reg = operation - (decrement ? 12 : 4);
    }
    if (values->registers[HPCLANG_RPL_CODE_P] > 15 || values->decimal > 1)
        return HPCLANG_RPL_SPAN_MALFORMED;
    static const unsigned firsts[8] = {0, 0, 2, 0, 15, 3, 0, 0},
                          widths[8] = {1, 1, 1, 3, 1, 12, 2, 16};
    unsigned p = (unsigned)values->registers[HPCLANG_RPL_CODE_P];
    struct hpclang_rpl_code_unit next = {.instruction = instruction,
                                         .length = length,
                                         .reg = reg,
                                         .first = field == 15 ? 0
                                                  : field     ? firsts[field]
                                                              : p,
                                         .nibbles = field == 15  ? 5
                                                    : field == 1 ? p + 1
                                                                 : widths[field],
                                         .decrement = decrement,
                                         .decimal = values->decimal,
                                         .carry = amount,
                                         .p = p};
    next.before = next.after = values->registers[next.reg];
    /* 818frk uses binary arithmetic even when Saturn decimal mode is set. */
    unsigned radix = opcode != 8 && next.decimal ? 10 : 16;
    for (unsigned i = next.first; i < next.first + next.nibbles; ++i) {
        unsigned digit = (next.before >> (4 * i)) & 15;
        if (digit >= radix)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (decrement) {
            unsigned borrow = digit < next.carry;
            digit += radix * borrow;
            digit -= next.carry;
            next.carry = borrow;
        } else {
            digit += next.carry;
            next.carry = digit >= radix;
            if (next.carry)
                digit -= radix;
        }
        next.after = (next.after & ~(UINT64_C(15) << (4 * i))) | ((uint64_t)digit << (4 * i));
    }
    *unit = next;
    return 0;
}

int hpclang_rpl_code_logical_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                     uint32_t limit, const struct hpclang_rpl_code_values *values,
                                     struct hpclang_rpl_code_logical *logical)
{
    if (!reader || !reader->read_nibble || !values || !logical)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000 || !limit || limit > 0x100000 - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 1, &opcode);
    if (result)
        return result;
    if (opcode)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (limit < 2)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = read_word(reader, instruction + 1, 1, &opcode);
    if (result)
        return result;
    if (opcode != 14)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (limit < 4)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = read_word(reader, instruction + 2, 2, &opcode);
    if (result)
        return result;
    unsigned field = opcode & 15, operation = opcode >> 4;
    if (field > 7 && field != 15)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (values->registers[HPCLANG_RPL_CODE_P] > 15 || values->carry > 1 || values->decimal > 1)
        return HPCLANG_RPL_SPAN_MALFORMED;
    static const unsigned lefts[8] = {0, 1, 2, 3, 1, 2, 0, 2};
    static const unsigned rights[8] = {1, 2, 0, 2, 0, 1, 2, 3};
    static const unsigned firsts[8] = {0, 0, 2, 0, 15, 3, 0, 0};
    static const unsigned widths[8] = {1, 1, 1, 3, 1, 12, 2, 16};
    unsigned p = (unsigned)values->registers[HPCLANG_RPL_CODE_P];
    struct hpclang_rpl_code_logical next = {.instruction = instruction,
                                            .length = 4,
                                            .left = lefts[operation & 7],
                                            .right = rights[operation & 7],
                                            .first = field == 15 ? 0
                                                     : field     ? firsts[field]
                                                                 : p,
                                            .nibbles = field == 15  ? 5
                                                       : field == 1 ? p + 1
                                                                    : widths[field],
                                            .disjunction = operation >> 3,
                                            .p = p};
    next.left_value = values->registers[next.left];
    next.right_value = values->registers[next.right];
    uint64_t mask = data_mask(next.nibbles) << (4 * next.first);
    uint64_t combined =
        next.disjunction ? next.left_value | next.right_value : next.left_value & next.right_value;
    next.result = (next.left_value & ~mask) | (combined & mask);
    *logical = next;
    return 0;
}

int hpclang_rpl_code_bitshift_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                      uint32_t limit, const struct hpclang_rpl_code_values *values,
                                      struct hpclang_rpl_code_bitshift *shift)
{
    if (!reader || !reader->read_nibble || !values || !shift)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000 || !limit || limit > 0x100000 - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 1, &opcode);
    if (result)
        return result;
    if (opcode != 8)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (limit < 2)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = read_word(reader, instruction + 1, 1, &opcode);
    if (result)
        return result;
    if (opcode != 1)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (limit < 3)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = read_word(reader, instruction + 2, 1, &opcode);
    if (result)
        return result;
    unsigned field = 7, reg, length = 3;
    if (opcode >= 12) {
        reg = opcode - 12;
    } else if (opcode == 9) {
        if (limit < 5)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t tail;
        result = read_word(reader, instruction + 3, 2, &tail);
        if (result)
            return result;
        field = tail & 15;
        reg = tail >> 4;
        length = 5;
        if (reg > 3 || (field > 7 && field != 15))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    if (values->registers[HPCLANG_RPL_CODE_P] > 15 || values->carry > 1 || values->decimal > 1)
        return HPCLANG_RPL_SPAN_MALFORMED;
    unsigned p = (unsigned)values->registers[HPCLANG_RPL_CODE_P];
    static const unsigned firsts[8] = {0, 0, 2, 0, 15, 3, 0, 0};
    static const unsigned widths[8] = {1, 1, 1, 3, 1, 12, 2, 16};
    struct hpclang_rpl_code_bitshift next = {.instruction = instruction,
                                             .length = length,
                                             .reg = reg,
                                             .first = field == 15 ? 0
                                                      : field     ? firsts[field]
                                                                  : p,
                                             .nibbles = field == 15  ? 5
                                                        : field == 1 ? p + 1
                                                                     : widths[field],
                                             .p = p,
                                             .before = values->registers[reg]};
    uint64_t mask = data_mask(next.nibbles), selected = (next.before >> (4 * next.first)) & mask;
    next.after =
        (next.before & ~(mask << (4 * next.first))) | ((selected >> 1) << (4 * next.first));
    next.lost_bit = selected & 1;
    *shift = next;
    return 0;
}

static void origin_overwrite(struct hpclang_rpl_code_origins *state, unsigned reg, uint32_t digits)
{
    if (!(digits & 31u))
        return;
    if (state->adjustment[reg])
        for (unsigned digit = 0; digit < 5; ++digit)
            state->registers[reg][digit] = 0;
    state->adjustment[reg] = 0;
}

static int data_step(const struct hpclang_rpl_reader *instructions,
                     const struct hpclang_rpl_reader *data, uint32_t instruction, uint32_t limit,
                     struct hpclang_rpl_code_values *values,
                     struct hpclang_rpl_code_origins *origins, hpclang_rpl_code_token_fn token,
                     void *context, uint32_t *length, hpclang_rpl_code_derive_fn derive,
                     hpclang_rpl_code_constant_fn constant, hpclang_rpl_code_unit_fn unit,
                     hpclang_rpl_code_sum_fn sum, hpclang_rpl_code_logical_fn logical)
{
    if (!instructions || !instructions->read_nibble || !values || !origins || !token || !length)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (values->registers[HPCLANG_RPL_CODE_P] > 15 || values->carry > 1 || values->decimal > 1 ||
        values->registers[HPCLANG_RPL_CODE_D0] > 0xfffff ||
        values->registers[HPCLANG_RPL_CODE_D1] > 0xfffff)
        return HPCLANG_RPL_SPAN_MALFORMED;
    for (unsigned reg = 0; reg <= HPCLANG_RPL_CODE_D1; ++reg)
        if (origins->adjustment[reg] > 0xfffff)
            return HPCLANG_RPL_SPAN_MALFORMED;
    unsigned p = (unsigned)values->registers[HPCLANG_RPL_CODE_P];
    struct hpclang_rpl_code_values next = *values;
    struct hpclang_rpl_code_origins sources = *origins;
    struct hpclang_rpl_code_literal literal;
    struct hpclang_rpl_code_assignment assignment;
    struct hpclang_rpl_code_exchange exchange;
    struct hpclang_rpl_code_memory memory;
    struct hpclang_rpl_code_address_adjust adjust;
    uint32_t tokens[16] = {0}, size;
    int result = hpclang_rpl_code_literal_inspect(instructions, instruction, limit, p, &literal);
    if (!result) {
        for (unsigned i = 0; i < literal.nibbles; ++i) {
            unsigned digit = literal.destination == HPCLANG_RPL_CODE_D0 ||
                                     literal.destination == HPCLANG_RPL_CODE_D1
                                 ? i
                                 : (p + i) & 15u;
            result = token(context, 0, literal.field + i,
                           (uint8_t)((literal.value >> (4 * digit)) & 15), &tokens[i]);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        }
        next.registers[literal.destination] =
            (next.registers[literal.destination] & ~literal.mask) | literal.value;
        result = hpclang_rpl_code_origins_literal(&sources, &literal, p, tokens, literal.nibbles);
        size = literal.length;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    result = hpclang_rpl_code_assignment_inspect(instructions, instruction, limit, p, &assignment);
    if (!result) {
        uint64_t value = assignment.source == HPCLANG_RPL_CODE_CONSTANT ||
                                 assignment.source == HPCLANG_RPL_CODE_NEXT_PC
                             ? assignment.constant
                             : next.registers[assignment.source];
        uint64_t mask = data_mask(assignment.nibbles);
        value = (value >> (4 * assignment.source_first)) & mask;
        next.registers[assignment.destination] = (next.registers[assignment.destination] &
                                                  ~(mask << (4 * assignment.destination_first))) |
                                                 (value << (4 * assignment.destination_first));
        result = hpclang_rpl_code_origins_assign(&sources, &assignment, tokens);
        if (!result && assignment.source == HPCLANG_RPL_CODE_CONSTANT && constant) {
            const struct hpclang_rpl_code_constant fact = {
                assignment, p, origins->registers[HPCLANG_RPL_CODE_P][0],
                origins->adjustment[HPCLANG_RPL_CODE_P]};
            result = constant(context, &fact, tokens);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            for (unsigned i = 0; i < assignment.nibbles; ++i) {
                if (!tokens[i])
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
                sources.registers[assignment.destination][assignment.destination_first + i] =
                    tokens[i];
            }
        }
        size = assignment.length;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    struct hpclang_rpl_code_logical_source combination = {0};
    result = hpclang_rpl_code_logical_inspect(instructions, instruction, limit, &next,
                                              &combination.logical);
    if (!result) {
        combination.p_origin = origins->registers[HPCLANG_RPL_CODE_P][0];
        combination.p_adjustment = origins->adjustment[HPCLANG_RPL_CODE_P];
        const unsigned regs[2] = {combination.logical.left, combination.logical.right};
        for (unsigned operand = 0; operand < 2; ++operand) {
            combination.adjustment[operand] = origins->adjustment[regs[operand]];
            for (unsigned digit = 0; digit < 16; ++digit)
                combination.origins[operand][digit] = origins->registers[regs[operand]][digit];
        }
        if (logical && (result = logical(context, &combination, tokens)))
            return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        unsigned reg = combination.logical.left, first = combination.logical.first;
        unsigned count = combination.logical.nibbles;
        origin_overwrite(&sources, reg, ((1u << count) - 1) << first);
        for (unsigned i = 0; i < count; ++i) {
            if (logical && !tokens[i])
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            sources.registers[reg][first + i] = logical ? tokens[i] : 0;
        }
        next.registers[reg] = combination.logical.result;
        size = combination.logical.length;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    struct hpclang_rpl_code_sum_source addition = {0};
    result = hpclang_rpl_code_sum_inspect(instructions, instruction, limit, &next, &addition.sum);
    if (!result) {
        addition.p_origin = origins->registers[HPCLANG_RPL_CODE_P][0];
        addition.p_adjustment = origins->adjustment[HPCLANG_RPL_CODE_P];
        unsigned registers[2] = {addition.sum.left, addition.sum.right};
        for (unsigned operand = 0; operand < 2; ++operand) {
            addition.adjustment[operand] = origins->adjustment[registers[operand]];
            for (unsigned i = 0; i < 16; ++i)
                addition.origins[operand][i] = origins->registers[registers[operand]][i];
        }
        if (sum) {
            result = sum(context, &addition, tokens);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        }
        unsigned reg = addition.sum.destination;
        origin_overwrite(&sources, reg, ((1u << addition.sum.nibbles) - 1) << addition.sum.first);
        for (unsigned i = 0; i < addition.sum.nibbles; ++i) {
            if (sum && !tokens[i])
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            sources.registers[reg][addition.sum.first + i] = sum ? tokens[i] : 0;
        }
        next.registers[reg] = addition.sum.result;
        next.carry = addition.sum.carry;
        size = addition.sum.length;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    struct hpclang_rpl_code_unit arithmetic;
    result = hpclang_rpl_code_unit_inspect(instructions, instruction, limit, &next, &arithmetic);
    if (!result) {
        arithmetic.p_origin = origins->registers[HPCLANG_RPL_CODE_P][0];
        arithmetic.p_adjustment = origins->adjustment[HPCLANG_RPL_CODE_P];
        arithmetic.adjustment = origins->adjustment[arithmetic.reg];
        for (unsigned i = 0; i < 16; ++i)
            arithmetic.origins[i] = origins->registers[arithmetic.reg][i];
        if (unit) {
            result = unit(context, &arithmetic, tokens);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        }
        origin_overwrite(&sources, arithmetic.reg,
                         ((1u << arithmetic.nibbles) - 1) << arithmetic.first);
        for (unsigned i = 0; i < arithmetic.nibbles; ++i) {
            if (unit && !tokens[i])
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            sources.registers[arithmetic.reg][arithmetic.first + i] = unit ? tokens[i] : 0;
        }
        next.registers[arithmetic.reg] = arithmetic.after;
        next.carry = arithmetic.carry;
        size = arithmetic.length;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    result = hpclang_rpl_code_exchange_inspect(instructions, instruction, limit, p, &exchange);
    if (!result) {
        uint64_t mask = data_mask(exchange.nibbles) << (4 * exchange.first);
        uint64_t left = next.registers[exchange.left], right = next.registers[exchange.right];
        next.registers[exchange.left] = (left & ~mask) | (right & mask);
        next.registers[exchange.right] = (right & ~mask) | (left & mask);
        result = hpclang_rpl_code_origins_exchange(&sources, &exchange);
        size = exchange.length;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    result = hpclang_rpl_code_memory_inspect(instructions, instruction, limit, p, &memory);
    if (!result) {
        if (!memory.load)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (!data || !data->read_nibble)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        uint32_t address = (uint32_t)next.registers[memory.address];
        if (address > 0x100000u - memory.nibbles)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint64_t value = 0;
        for (unsigned i = 0; i < memory.nibbles; ++i) {
            uint8_t nibble;
            if (!data->read_nibble(data->context, address + i, &nibble))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            if (nibble > 15)
                return HPCLANG_RPL_SPAN_MALFORMED;
            result = token(context, 1, address + i, nibble, &tokens[i]);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            value |= (uint64_t)nibble << (4 * i);
        }
        uint64_t mask = data_mask(memory.nibbles) << (4 * memory.first);
        next.registers[memory.data] =
            (next.registers[memory.data] & ~mask) | (value << (4 * memory.first));
        result = hpclang_rpl_code_origins_memory(&sources, &memory, tokens, memory.nibbles);
        size = memory.length;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    result = hpclang_rpl_code_address_adjust_inspect(instructions, instruction, limit, &adjust);
    if (result == HPCLANG_RPL_SPAN_UNSUPPORTED) {
        uint32_t opcode;
        result = read_word(instructions, instruction, 2, &opcode);
        if (result)
            return result;
        size = 2;
        if (opcode == 0xae || ((opcode & 15u) == 11 && (opcode >> 4) < 8)) {
            /* EA / BfA: A=A-C in the address / selected field. Computed
             * result digits require an expression, never original-cell tokens. */
            unsigned address_field = opcode == 0xae;
            if (!address_field) {
                uint32_t operation;
                if (limit < 3)
                    return HPCLANG_RPL_SPAN_BOUNDS;
                result = read_word(instructions, instruction + 2, 1, &operation);
                if (result)
                    return result;
                if (operation != 10)
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
            }
            static const unsigned firsts[] = {0, 0, 2, 0, 15, 3, 0, 0};
            static const unsigned widths[] = {1, 1, 1, 3, 1, 12, 2, 16};
            unsigned field = address_field ? 0 : opcode >> 4;
            unsigned first = field == 0 ? p : firsts[field];
            unsigned width = field == 1 ? p + 1 : widths[field];
            if (address_field) {
                first = 0;
                width = 5;
            }
            uint64_t a = next.registers[HPCLANG_RPL_CODE_A];
            uint64_t c = next.registers[HPCLANG_RPL_CODE_C];
            unsigned borrow = 0, radix = next.decimal ? 10 : 16;
            uint32_t written = ((1u << width) - 1u) << first;
            origin_overwrite(&sources, HPCLANG_RPL_CODE_A, written);
            for (unsigned i = first; i < first + width; ++i) {
                unsigned left = (a >> (4 * i)) & 15u;
                unsigned right = (c >> (4 * i)) & 15u;
                if (left >= radix || right >= radix)
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
                int digit = (int)left - (int)right - (int)borrow;
                borrow = digit < 0;
                if (borrow)
                    digit += radix;
                a = (a & ~(UINT64_C(15) << (4 * i))) | ((uint64_t)digit << (4 * i));
                sources.registers[HPCLANG_RPL_CODE_A][i] = 0;
            }
            next.registers[HPCLANG_RPL_CODE_A] = a;
            next.carry = borrow;
            if (derive && first + width <= 5) {
                struct hpclang_rpl_execution_subtraction expression = {.first = first,
                                                                       .nibbles = width,
                                                                       .decimal = next.decimal,
                                                                       .value =
                                                                           (uint32_t)a & 0xfffffu,
                                                                       .carry = borrow};
                struct hpclang_rpl_execution_value *operands[] = {&expression.left,
                                                                  &expression.right};
                for (unsigned operand = 0; operand < 2; ++operand) {
                    unsigned reg = operand ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A;
                    operands[operand]->value = (uint32_t)values->registers[reg] & 0xfffffu;
                    operands[operand]->adjustment = origins->adjustment[reg];
                    for (unsigned digit = 0; digit < 5; ++digit)
                        operands[operand]->origins[digit] = origins->registers[reg][digit];
                }
                result = derive(context, instruction, &expression, tokens);
                if (result)
                    return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
                for (unsigned digit = 0; digit < 5; ++digit) {
                    if (!tokens[digit])
                        return HPCLANG_RPL_SPAN_MALFORMED;
                    sources.registers[HPCLANG_RPL_CODE_A][digit] = tokens[digit];
                }
                sources.adjustment[HPCLANG_RPL_CODE_A] = 0;
            }
            size = address_field ? 2 : 3;
        } else if (opcode == 0x40 || opcode == 0x50) {
            next.decimal = opcode == 0x50;
        } else if (((opcode & 15u) == 14 && (opcode >> 4) >= 4 && (opcode >> 4) <= 7) ||
                   ((opcode & 15u) == 12 && (opcode >> 4) >= 12)) {
            /* E4..E7 / CC..CF: increment/decrement A..D's address field.
             * Hex arithmetic retains source + modular offset, including
             * expression sources. BCD needs a different relation; leave its
             * computed digits unknown rather than invent a binary offset. */
            unsigned decrement = (opcode & 15u) == 12;
            unsigned reg = (opcode >> 4) - (decrement ? 12u : 4u);
            uint32_t old = (uint32_t)next.registers[reg] & 0xfffffu;
            uint32_t value = 0, carry = 1;
            for (unsigned i = 0; i < 5; ++i) {
                unsigned digit = (old >> (4 * i)) & 15u;
                if (next.decimal && digit > 9)
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
                unsigned radix = next.decimal ? 10u : 16u;
                if (decrement) {
                    unsigned borrow = digit < carry;
                    digit = digit + (borrow ? radix : 0) - carry;
                    carry = borrow;
                } else {
                    digit += carry;
                    carry = digit >= radix;
                    if (carry)
                        digit = 0;
                }
                value |= digit << (4 * i);
                if (next.decimal)
                    sources.registers[reg][i] = 0;
            }
            next.registers[reg] = (next.registers[reg] & ~UINT64_C(0xfffff)) | value;
            next.carry = carry;
            sources.adjustment[reg] =
                next.decimal ? 0
                             : (sources.adjustment[reg] + (decrement ? 0xfffffu : 1u)) & 0xfffffu;
        } else {
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
        goto done;
    }
    if (result)
        return result;
    int64_t address = (int64_t)next.registers[adjust.address] + adjust.delta;
    next.registers[adjust.address] = (uint32_t)address & 0xfffffu;
    next.carry = address < 0 || address > 0xfffff;
    result = hpclang_rpl_code_origins_adjust(&sources, &adjust);
    size = adjust.length;
done:
    if (result)
        return result;
    *values = next;
    *origins = sources;
    *length = size;
    return 0;
}

int hpclang_rpl_code_data_step(const struct hpclang_rpl_reader *instructions,
                               const struct hpclang_rpl_reader *data, uint32_t instruction,
                               uint32_t limit, struct hpclang_rpl_code_values *values,
                               struct hpclang_rpl_code_origins *origins,
                               hpclang_rpl_code_token_fn token, void *context, uint32_t *length)
{
    return data_step(instructions, data, instruction, limit, values, origins, token, context,
                     length, 0, 0, 0, 0, 0);
}

static void execution_use(struct hpclang_rpl_code_step_uses *uses, unsigned kind, uint32_t value,
                          const uint32_t *origins, uint32_t adjustment)
{
    struct hpclang_rpl_code_address_use *use = &uses->addresses[uses->count++];
    use->kind = kind;
    use->value = value;
    use->adjustment = adjustment;
    use->generated = 0;
    for (unsigned i = 0; i < 5; ++i)
        use->origins[i] = origins[i];
}

static int native_word_observe(const struct hpclang_rpl_code_callbacks *callbacks, void *context,
                               const struct hpclang_rpl_code_execution *before,
                               struct hpclang_rpl_code_execution *next, uint32_t reg, int32_t delta)
{
    if (!callbacks || !callbacks->native_word)
        return 0;
    struct hpclang_rpl_code_native_word word = {.instruction = before->control.pc,
                                                .reg = reg,
                                                .before = (uint32_t)before->values.registers[reg],
                                                .after = (uint32_t)next->values.registers[reg],
                                                .adjustment = before->origins.adjustment[reg],
                                                .delta = delta};
    if (word.adjustment > 0xfffff || word.after != word.before + (uint32_t)delta)
        return HPCLANG_RPL_SPAN_MALFORMED;
    for (unsigned digit = 0; digit < 8; ++digit)
        word.origins[digit] = before->origins.registers[reg][digit];
    uint32_t tokens[8] = {0};
    int result = callbacks->native_word(context, &word, tokens);
    if (result)
        return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
    for (unsigned digit = reg == HPCLANG_RPL_CODE_B ? 5 : 0; digit < 8; ++digit) {
        if (!tokens[digit])
            return HPCLANG_RPL_SPAN_MALFORMED;
        next->origins.registers[reg][digit] = tokens[digit];
    }
    return 0;
}

int hpclang_rpl_code_execution_step_observed(const struct hpclang_rpl_reader *instructions,
                                             const struct hpclang_rpl_reader *data, uint32_t limit,
                                             struct hpclang_rpl_code_execution *state,
                                             hpclang_rpl_code_token_fn token,
                                             const struct hpclang_rpl_code_callbacks *callbacks,
                                             void *context, struct hpclang_rpl_code_step_uses *uses)
{
    const hpclang_rpl_code_derive_fn derive = callbacks ? callbacks->derive : 0;
    const hpclang_rpl_code_store_fn store = callbacks ? callbacks->store : 0;
    const hpclang_rpl_code_config_fn config = callbacks ? callbacks->config : 0;
    const hpclang_rpl_code_predicate_fn predicate = callbacks ? callbacks->predicate : 0;

    if (!state || !uses || !token)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (state->values.carry != state->control.carry || state->control.cursor >= 8)
        return HPCLANG_RPL_SPAN_MALFORMED;
    for (unsigned slot = 0; slot < 8; ++slot)
        if (state->control.stack_generated[slot] > 0x100000 ||
            state->stack_adjustment[slot] > 0xfffff ||
            (state->stack_adjustment[slot] && state->control.stack_generated[slot]))
            return HPCLANG_RPL_SPAN_MALFORMED;
    struct hpclang_rpl_code_execution next = *state;
    struct hpclang_rpl_code_step_uses observed = {0};
    uint32_t pc = next.control.pc, length;
    int result = data_step(instructions, data, pc, limit, &next.values, &next.origins, token,
                           context, &length, derive, callbacks ? callbacks->constant : 0,
                           callbacks ? callbacks->unit : 0, callbacks ? callbacks->sum : 0,
                           callbacks ? callbacks->logical : 0);
    if (!result) {
        struct hpclang_rpl_code_memory memory;
        int load = hpclang_rpl_code_memory_inspect(
            instructions, pc, limit, (unsigned)state->values.registers[HPCLANG_RPL_CODE_P],
            &memory);
        if (!load)
            execution_use(&observed, HPCLANG_RPL_CODE_USE_LOAD_ADDRESS,
                          (uint32_t)state->values.registers[memory.address],
                          state->origins.registers[memory.address],
                          state->origins.adjustment[memory.address]);
        else if (load != HPCLANG_RPL_SPAN_UNSUPPORTED)
            return load;
        next.control.pc = (pc + length) & 0xfffffu;
        next.control.carry = next.values.carry;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    if (store) {
        struct hpclang_rpl_code_memory memory;
        result = hpclang_rpl_code_memory_inspect(
            instructions, pc, limit, (unsigned)state->values.registers[HPCLANG_RPL_CODE_P],
            &memory);
        if (!result && !memory.load) {
            uint32_t address = (uint32_t)state->values.registers[memory.address];
            if (address > 0x100000u - memory.nibbles)
                return HPCLANG_RPL_SPAN_BOUNDS;
            execution_use(&observed, HPCLANG_RPL_CODE_USE_STORE_ADDRESS, address,
                          state->origins.registers[memory.address],
                          state->origins.adjustment[memory.address]);
            struct hpclang_rpl_code_store effect = {.instruction = pc,
                                                    .first = memory.first,
                                                    .nibbles = memory.nibbles,
                                                    .address = observed.addresses[0],
                                                    .value = state->values.registers[memory.data],
                                                    .adjustment =
                                                        state->origins.adjustment[memory.data]};
            for (unsigned digit = 0; digit < 16; ++digit)
                effect.origins[digit] = state->origins.registers[memory.data][digit];
            next.control.pc = (pc + memory.length) & 0xfffffu;
            result = store(context, &effect);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            goto done;
        }
        if (result && result != HPCLANG_RPL_SPAN_UNSUPPORTED)
            return result;
    }
    uint32_t opcode;
    result = read_word(instructions, pc, 2, &opcode);
    if (result)
        return result;
    unsigned status_test = opcode == 0x68 || opcode == 0x78;
    if (status_test || (opcode & 15) == 9 || opcode == 0xa8 || opcode == 0xb8) {
        struct hpclang_rpl_code_predicate condition = {0};
        if (status_test) {
            if (limit < 5)
                return HPCLANG_RPL_SPAN_BOUNDS;
            uint32_t tail;
            result = read_word(instructions, pc + 2, 3, &tail);
            if (result)
                return result;
            condition.taken = ((next.status >> (tail & 15)) & 1) == (opcode == 0x78);
            condition.displacement = tail >> 4;
        } else {
            if (!predicate)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            result = hpclang_rpl_code_predicate_inspect(instructions, pc, limit, &next.values,
                                                        &condition);
            if (result)
                return result;
        }
        next.values.carry = next.control.carry = condition.taken;
        if (condition.displacement) {
            result = hpclang_rpl_code_branch_inspect(instructions, pc, limit, &observed.branch);
            if (result)
                return result;
            observed.has_branch = 1;
            next.control.pc = condition.taken ? observed.branch.target : (pc + 5) & 0xfffff;
        } else if (!condition.taken) {
            next.control.pc = (pc + 5) & 0xfffff;
        } else {
            unsigned slot = (next.control.cursor + 7) & 7;
            uint32_t target = next.control.stack[slot],
                     generated = next.control.stack_generated[slot];
            if (target > 0xfffff)
                return HPCLANG_RPL_SPAN_MALFORMED;
            for (unsigned digit = 0; digit < 5; ++digit)
                if (generated && next.control.stack_origins[slot][digit])
                    return HPCLANG_RPL_SPAN_MALFORMED;
            execution_use(&observed, HPCLANG_RPL_CODE_USE_RETURN_TARGET, target,
                          next.control.stack_origins[slot], next.stack_adjustment[slot]);
            observed.addresses[0].generated = generated;
            next.control.cursor = slot;
            next.control.pc = target;
            next.control.stack[slot] = next.control.stack_generated[slot] =
                next.stack_adjustment[slot] = 0;
            for (unsigned digit = 0; digit < 5; ++digit)
                next.control.stack_origins[slot][digit] = 0;
        }
        if (!status_test) {
            result = predicate(context, &condition, state);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        }
        goto done;
    }
    if (opcode == 0x08 && limit >= 3) {
        uint32_t operation;
        result = read_word(instructions, pc + 2, 1, &operation);
        if (result)
            return result;
        if (operation == 4 || operation == 5) {
            hpclang_rpl_code_config_fn action = operation == 5 ? config
                                                : callbacks    ? callbacks->unconfig
                                                               : 0;
            if (!action)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            struct hpclang_rpl_code_configuration effect = {
                .instruction = pc,
                .value = (uint32_t)next.values.registers[HPCLANG_RPL_CODE_C] & 0xfffff,
                .adjustment = next.origins.adjustment[HPCLANG_RPL_CODE_C]};
            for (unsigned digit = 0; digit < 5; ++digit)
                effect.origins[digit] = next.origins.registers[HPCLANG_RPL_CODE_C][digit];
            result = action(context, &effect);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            next.control.pc = (pc + 3) & 0xfffff;
            goto done;
        }
    }
    if (opcode == 0x18) {
        struct hpclang_rpl_code_bitshift_source fact = {0};
        result =
            hpclang_rpl_code_bitshift_inspect(instructions, pc, limit, &state->values, &fact.shift);
        if (!result) {
            unsigned reg = fact.shift.reg, first = fact.shift.first, count = fact.shift.nibbles;
            fact.p_origin = state->origins.registers[HPCLANG_RPL_CODE_P][0];
            fact.p_adjustment = state->origins.adjustment[HPCLANG_RPL_CODE_P];
            fact.adjustment = state->origins.adjustment[reg];
            for (unsigned i = 0; i < 16; ++i)
                fact.origins[i] = state->origins.registers[reg][i];
            uint32_t tokens[16] = {0};
            hpclang_rpl_code_bitshift_fn observe = callbacks ? callbacks->bitshift : 0;
            if (observe && (result = observe(context, &fact, tokens)))
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            uint32_t affected = ((1u << count) - 1) << first;
            if (fact.adjustment && first < 5)
                affected |= 31u;
            origin_overwrite(&next.origins, reg, affected);
            for (unsigned i = 0; i < 16; ++i) {
                if (!(affected & (1u << i)))
                    continue;
                if (observe && !tokens[i])
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
                next.origins.registers[reg][i] = observe ? tokens[i] : 0;
            }
            next.values.registers[reg] = fact.shift.after;
            next.hst |= fact.shift.lost_bit << 1;
            next.control.pc = (pc + fact.shift.length) & 0xfffff;
            goto done;
        }
        if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
            return result;
    }
    if (opcode == 0x18 && limit >= 3) {
        uint32_t operation;
        result = read_word(instructions, pc + 2, 1, &operation);
        if (result)
            return result;
        if (operation < 8) {
            unsigned reg = operation & 3, right = operation >> 2;
            uint64_t value = next.values.registers[reg];
            uint32_t origins[16];
            for (unsigned i = 0; i < 16; ++i)
                origins[i] =
                    next.origins.adjustment[reg] && i < 5 ? 0 : next.origins.registers[reg][i];
            /* Rotation is a digit permutation, not a 20-bit affine operation.
             * Preserve exact sources; invalidate digits still represented by
             * a nonzero low-address adjustment before moving their positions. */
            next.origins.adjustment[reg] = 0;
            for (unsigned i = 0; i < 16; ++i)
                next.origins.registers[reg][i] = origins[(i + (right ? 1 : 15)) & 15];
            next.values.registers[reg] =
                right ? (value >> 4) | (value << 60) : (value << 4) | (value >> 60);
            /* The qualified HP49g+/HP50g native interpreters preserve HST
             * for both directions, unlike hardware Saturn's documented SB
             * update on right wrap. Do not manufacture that side effect. */
            next.control.pc = (pc + 3) & 0xfffff;
            goto done;
        }
    }
    if (opcode == 0x48 || opcode == 0x58) {
        uint32_t bit;
        if (limit < 3)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(instructions, pc + 2, 1, &bit);
        if (result)
            return result;
        if (opcode == 0x48)
            next.status &= ~(UINT32_C(1) << bit);
        else
            next.status |= UINT32_C(1) << bit;
        next.control.pc = (pc + 3) & 0xfffff;
        goto done;
    }
    if (opcode == 0x28) {
        uint32_t mask;
        if (limit < 3)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(instructions, pc + 2, 1, &mask);
        if (result)
            return result;
        next.hst &= ~mask;
        next.control.pc = (pc + 3) & 0xfffff;
        goto done;
    }
    if (opcode == 0x60 || opcode == 0x70) {
        unsigned slot = next.control.cursor;
        uint32_t *reg = next.origins.registers[HPCLANG_RPL_CODE_C];
        if (opcode == 0x60) {
            next.control.stack_generated[slot] = 0;
            next.control.stack[slot] =
                (uint32_t)next.values.registers[HPCLANG_RPL_CODE_C] & 0xfffffu;
            for (unsigned i = 0; i < 5; ++i)
                next.control.stack_origins[slot][i] = reg[i];
            next.stack_adjustment[slot] = next.origins.adjustment[HPCLANG_RPL_CODE_C];
            next.control.cursor = (slot + 1) & 7u;
        } else {
            slot = (slot + 7) & 7u;
            next.values.registers[HPCLANG_RPL_CODE_C] =
                (next.values.registers[HPCLANG_RPL_CODE_C] & ~UINT64_C(0xfffff)) |
                (next.control.stack[slot] & 0xfffffu);
            for (unsigned i = 0; i < 5; ++i) {
                reg[i] = next.control.stack_origins[slot][i];
                next.control.stack_origins[slot][i] = 0;
            }
            next.control.stack[slot] = 0;
            next.control.stack_generated[slot] = 0;
            next.control.cursor = slot;
            next.origins.adjustment[HPCLANG_RPL_CODE_C] = next.stack_adjustment[slot];
            next.stack_adjustment[slot] = 0;
        }
        next.control.pc = (pc + 2) & 0xfffffu;
        goto done;
    }
    if (opcode == 0x18) {
        uint32_t extended;
        if (limit < 4)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(instructions, pc, 4, &extended);
        if (result)
            return result;
        if (extended == 0xab18 && callbacks && callbacks->native_scan) {
            struct hpclang_rpl_code_native_scan effect;
            result = hpclang_rpl_code_native_scan_inspect(
                instructions, data, limit, state, callbacks->native_scan_max_reads, &effect);
            if (result != HPCLANG_RPL_SPAN_UNSUPPORTED) {
                if (result)
                    return result;
                if (next.origins.adjustment[HPCLANG_RPL_CODE_D0] > 0xfffff)
                    return HPCLANG_RPL_SPAN_MALFORMED;
                struct hpclang_rpl_code_scan_origins origins = {0};
                result = callbacks->native_scan(context, &effect, state, &origins);
                if (result)
                    return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
                next.origins.adjustment[HPCLANG_RPL_CODE_D0] =
                    (next.origins.adjustment[HPCLANG_RPL_CODE_D0] + effect.d0 -
                     (uint32_t)next.values.registers[HPCLANG_RPL_CODE_D0]) &
                    0xfffff;
                next.values.registers[HPCLANG_RPL_CODE_D0] = effect.d0;
                if (effect.returns) {
                    unsigned slot = effect.cursor;
                    uint32_t generated = next.control.stack_generated[slot];
                    for (unsigned digit = 0; digit < 5; ++digit)
                        if (generated && next.control.stack_origins[slot][digit])
                            return HPCLANG_RPL_SPAN_MALFORMED;
                    execution_use(&observed, HPCLANG_RPL_CODE_USE_RETURN_TARGET, effect.target,
                                  next.control.stack_origins[slot], next.stack_adjustment[slot]);
                    observed.addresses[0].generated = generated;
                    next.control.stack[slot] = next.control.stack_generated[slot] =
                        next.stack_adjustment[slot] = 0;
                    for (unsigned digit = 0; digit < 5; ++digit)
                        next.control.stack_origins[slot][digit] = 0;
                } else {
                    if (origins.d1_adjustment > 0xfffff || origins.target_adjustment > 0xfffff)
                        return HPCLANG_RPL_SPAN_MALFORMED;
                    for (unsigned digit = 0; digit < 8; ++digit) {
                        if (!origins.b[digit])
                            return HPCLANG_RPL_SPAN_MALFORMED;
                        next.origins.registers[HPCLANG_RPL_CODE_B][digit] = origins.b[digit];
                    }
                    for (unsigned digit = 0; digit < 16; ++digit) {
                        if (digit < 5 && (!origins.d1[digit] || !origins.target[digit]))
                            return HPCLANG_RPL_SPAN_MALFORMED;
                        next.origins.registers[HPCLANG_RPL_CODE_D1][digit] =
                            digit < 5 ? origins.d1[digit] : 0;
                    }
                    next.values.registers[HPCLANG_RPL_CODE_B] = effect.b;
                    next.values.registers[HPCLANG_RPL_CODE_D1] = effect.d1;
                    next.origins.adjustment[HPCLANG_RPL_CODE_B] = 0;
                    next.origins.adjustment[HPCLANG_RPL_CODE_D1] = origins.d1_adjustment;
                    execution_use(&observed, HPCLANG_RPL_CODE_USE_MEMORY_TARGET, effect.target,
                                  origins.target, origins.target_adjustment);
                }
                next.control.pc = effect.target;
                next.control.cursor = effect.cursor;
                next.control.carry = next.values.carry = effect.carry;
                goto done;
            }
        }
        if (extended == 0xeb18) {
            /* Qualified HP49g+/HP50g ROM 81BE. The store precedes both
             * dependent reads, so the callback must update the scratch view. */
            if (next.values.carry) {
                const uint32_t unknown[5] = {0};
                next.control.pc = 0x2e15;
                /* This target lives in the native ROM profile, not in the
                 * instruction's Saturn operand cells. Do not invent origins. */
                execution_use(&observed, HPCLANG_RPL_CODE_USE_MEMORY_TARGET, next.control.pc,
                              unknown, 0);
                goto done;
            }
            if (!store)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            if (!data || !data->read_nibble)
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            uint32_t b = (uint32_t)next.values.registers[HPCLANG_RPL_CODE_B];
            uint32_t d0 = ((uint32_t)next.values.registers[HPCLANG_RPL_CODE_A] + 5) & 0xfffff;
            if ((b & 0xfffff) > 0xffffb || d0 > 0xffffa ||
                next.values.registers[HPCLANG_RPL_CODE_D0] > 0xfffff)
                return HPCLANG_RPL_SPAN_BOUNDS;
            execution_use(&observed, HPCLANG_RPL_CODE_USE_STORE_ADDRESS, b & 0xfffff,
                          next.origins.registers[HPCLANG_RPL_CODE_B],
                          next.origins.adjustment[HPCLANG_RPL_CODE_B]);
            struct hpclang_rpl_code_store effect = {
                .instruction = pc,
                .nibbles = 5,
                .address = observed.addresses[0],
                .value = next.values.registers[HPCLANG_RPL_CODE_D0],
                .adjustment = next.origins.adjustment[HPCLANG_RPL_CODE_D0]};
            for (unsigned digit = 0; digit < 16; ++digit)
                effect.origins[digit] = next.origins.registers[HPCLANG_RPL_CODE_D0][digit];
            result = store(context, &effect);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            next.values.registers[HPCLANG_RPL_CODE_B] =
                (next.values.registers[HPCLANG_RPL_CODE_B] & ~UINT64_C(0xffffffff)) |
                (uint32_t)(b + 5);
            next.origins.adjustment[HPCLANG_RPL_CODE_B] =
                (next.origins.adjustment[HPCLANG_RPL_CODE_B] + 5) & 0xfffff;
            for (unsigned digit = 5; digit < 8; ++digit)
                next.origins.registers[HPCLANG_RPL_CODE_B][digit] = 0;
            result = native_word_observe(callbacks, context, state, &next, HPCLANG_RPL_CODE_B, 5);
            if (result)
                return result;
            next.values.registers[HPCLANG_RPL_CODE_D0] = d0;
            for (unsigned digit = 0; digit < 5; ++digit)
                next.origins.registers[HPCLANG_RPL_CODE_D0][digit] =
                    next.origins.registers[HPCLANG_RPL_CODE_A][digit];
            next.origins.adjustment[HPCLANG_RPL_CODE_D0] =
                (next.origins.adjustment[HPCLANG_RPL_CODE_A] + 5) & 0xfffff;
        }
        if (extended == 0x1b18 || extended == 0xeb18) {
            /* Native-ROM 81B1: A.A=read5(D0), PC=read5(A.A), D0+=5.
             * The ROM increments the full native D0 word; reject a result
             * outside the canonical 20-bit state instead of wrapping it. */
            if (!data || !data->read_nibble)
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            uint32_t d0 = (uint32_t)next.values.registers[HPCLANG_RPL_CODE_D0];
            if (d0 > 0xffffa)
                return HPCLANG_RPL_SPAN_BOUNDS;
            uint32_t address = d0, sources[5];
            for (unsigned digit = 0; digit < 5; ++digit)
                sources[digit] = next.origins.registers[HPCLANG_RPL_CODE_D0][digit];
            for (unsigned load = 0; load < 2; ++load) {
                execution_use(&observed, HPCLANG_RPL_CODE_USE_LOAD_ADDRESS, address, sources,
                              load ? 0 : next.origins.adjustment[HPCLANG_RPL_CODE_D0]);
                if (address > 0xffffb)
                    return HPCLANG_RPL_SPAN_BOUNDS;
                uint32_t value;
                result = read_word(data, address, 5, &value);
                if (result)
                    return result;
                for (unsigned digit = 0; digit < 5; ++digit) {
                    result = token(context, 1, address + digit, (value >> (4 * digit)) & 15,
                                   &sources[digit]);
                    if (result)
                        return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
                }
                if (!load) {
                    next.values.registers[HPCLANG_RPL_CODE_A] =
                        (next.values.registers[HPCLANG_RPL_CODE_A] & ~UINT64_C(0xfffff)) | value;
                    for (unsigned digit = 0; digit < 5; ++digit)
                        next.origins.registers[HPCLANG_RPL_CODE_A][digit] = sources[digit];
                    next.origins.adjustment[HPCLANG_RPL_CODE_A] = 0;
                }
                address = value;
            }
            execution_use(&observed, HPCLANG_RPL_CODE_USE_MEMORY_TARGET, address, sources, 0);
            next.values.registers[HPCLANG_RPL_CODE_D0] = d0 + 5;
            next.origins.adjustment[HPCLANG_RPL_CODE_D0] =
                (next.origins.adjustment[HPCLANG_RPL_CODE_D0] + 5) & 0xfffff;
            next.control.pc = address;
            goto done;
        }
        if (extended == 0xdb18) {
            /* Qualified native-ROM 81BD: three dependent reads, not a
             * fallthrough. Retain B's source and accumulated 20-bit offset;
             * the computed digits are not directly stored source digits. */
            if (!data || !data->read_nibble)
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            uint32_t b = (uint32_t)next.values.registers[HPCLANG_RPL_CODE_B] - 5u;
            uint32_t d = (uint32_t)next.values.registers[HPCLANG_RPL_CODE_D] + 1u;
            uint32_t address = b & 0xfffffu, sources[5], pointer_sources[5], pointer = 0;
            uint32_t adjustment = (next.origins.adjustment[HPCLANG_RPL_CODE_B] - 5u) & 0xfffffu;
            for (unsigned digit = 0; digit < 5; ++digit)
                sources[digit] = next.origins.registers[HPCLANG_RPL_CODE_B][digit];
            for (unsigned load = 0; load < 3; ++load) {
                execution_use(&observed, HPCLANG_RPL_CODE_USE_LOAD_ADDRESS, address, sources,
                              load ? 0 : adjustment);
                if (address > 0xffffbu)
                    return HPCLANG_RPL_SPAN_BOUNDS;
                uint32_t value;
                result = read_word(data, address, 5, &value);
                if (result)
                    return result;
                for (unsigned digit = 0; digit < 5; ++digit) {
                    result = token(context, 1, address + digit, (value >> (4 * digit)) & 15,
                                   &sources[digit]);
                    if (result)
                        return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
                }
                if (load == 0) {
                    pointer = value;
                    if (pointer > 0xffffau)
                        return HPCLANG_RPL_SPAN_BOUNDS;
                    for (unsigned digit = 0; digit < 5; ++digit)
                        pointer_sources[digit] = sources[digit];
                } else if (load == 1) {
                    next.values.registers[HPCLANG_RPL_CODE_A] =
                        (next.values.registers[HPCLANG_RPL_CODE_A] & ~UINT64_C(0xfffff)) | value;
                    for (unsigned digit = 0; digit < 5; ++digit)
                        next.origins.registers[HPCLANG_RPL_CODE_A][digit] = sources[digit];
                }
                address = value;
            }
            execution_use(&observed, HPCLANG_RPL_CODE_USE_MEMORY_TARGET, address, sources, 0);
            next.values.registers[HPCLANG_RPL_CODE_B] =
                (next.values.registers[HPCLANG_RPL_CODE_B] & ~UINT64_C(0xffffffff)) | b;
            next.values.registers[HPCLANG_RPL_CODE_D] =
                (next.values.registers[HPCLANG_RPL_CODE_D] & ~UINT64_C(0xffffffff)) | d;
            next.values.registers[HPCLANG_RPL_CODE_D0] = pointer + 5;
            next.origins.adjustment[HPCLANG_RPL_CODE_A] = 0;
            next.origins.adjustment[HPCLANG_RPL_CODE_B] = adjustment;
            next.origins.adjustment[HPCLANG_RPL_CODE_D] = 0;
            next.origins.adjustment[HPCLANG_RPL_CODE_D0] = 5;
            for (unsigned digit = 0; digit < 8; ++digit) {
                if (digit >= 5)
                    next.origins.registers[HPCLANG_RPL_CODE_B][digit] = 0;
                next.origins.registers[HPCLANG_RPL_CODE_D][digit] = 0;
            }
            result = native_word_observe(callbacks, context, state, &next, HPCLANG_RPL_CODE_B, -5);
            if (!result)
                result =
                    native_word_observe(callbacks, context, state, &next, HPCLANG_RPL_CODE_D, 1);
            if (result)
                return result;
            for (unsigned digit = 0; digit < 5; ++digit)
                next.origins.registers[HPCLANG_RPL_CODE_D0][digit] = pointer_sources[digit];
            next.control.pc = address;
            goto done;
        }
    }
    struct hpclang_rpl_code_pc_exchange exchange;
    result = HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (opcode == 0x18)
        result = hpclang_rpl_code_pc_exchange_inspect(
            instructions, pc, limit, (uint32_t)next.values.registers[HPCLANG_RPL_CODE_A],
            (uint32_t)next.values.registers[HPCLANG_RPL_CODE_C], &exchange);
    if (!result) {
        unsigned reg = exchange.capture.destination;
        execution_use(&observed, HPCLANG_RPL_CODE_USE_REGISTER_TARGET, exchange.target,
                      next.origins.registers[reg], next.origins.adjustment[reg]);
        next.values.registers[reg] =
            (next.values.registers[reg] & ~UINT64_C(0xfffff)) | exchange.capture.constant;
        for (unsigned i = 0; i < 5; ++i)
            next.origins.registers[reg][i] = 0;
        next.origins.adjustment[reg] = 0;
        next.control.pc = exchange.target;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    struct hpclang_rpl_code_indirect jump;
    if (opcode == 0x18 || opcode == 0x08)
        result = hpclang_rpl_code_indirect_inspect(
            instructions, data, pc, limit, (uint32_t)next.values.registers[HPCLANG_RPL_CODE_A],
            (uint32_t)next.values.registers[HPCLANG_RPL_CODE_C], &jump);
    if (!result) {
        unsigned reg = jump.register_c ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A;
        execution_use(&observed, HPCLANG_RPL_CODE_USE_REGISTER_TARGET, jump.register_value,
                      next.origins.registers[reg], next.origins.adjustment[reg]);
        if (jump.via_memory) {
            uint32_t origins[5] = {0};
            for (unsigned i = 0; i < 5; ++i) {
                result = token(context, 1, jump.register_value + i,
                               (uint8_t)((jump.target >> (4 * i)) & 15u), &origins[i]);
                if (result)
                    return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            }
            execution_use(&observed, HPCLANG_RPL_CODE_USE_MEMORY_TARGET, jump.target, origins, 0);
        }
        next.control.pc = jump.target;
        goto done;
    }
    if (result != HPCLANG_RPL_SPAN_UNSUPPORTED)
        return result;
    struct hpclang_rpl_code_return_use returned;
    result = hpclang_rpl_code_control_step(instructions, limit, &next.control, &returned);
    if (result)
        return result;
    next.values.carry = next.control.carry;
    if (returned.present) {
        execution_use(&observed, HPCLANG_RPL_CODE_USE_RETURN_TARGET, returned.target,
                      returned.origins, next.stack_adjustment[next.control.cursor]);
        next.stack_adjustment[next.control.cursor] = 0;
        observed.addresses[observed.count - 1].generated = returned.generated;
    } else {
        result = hpclang_rpl_code_branch_inspect(instructions, pc, limit, &observed.branch);
        if (result && result != HPCLANG_RPL_SPAN_UNSUPPORTED)
            return result;
        observed.has_branch = result == 0;
        if (observed.has_branch && observed.branch.call)
            next.stack_adjustment[state->control.cursor] = 0;
    }
done:
    *state = next;
    *uses = observed;
    return 0;
}

int hpclang_rpl_code_origins_plan(const uint32_t *tokens, uint32_t current, uint32_t replacement,
                                  hpclang_rpl_code_origin_resolve_fn resolve, void *context,
                                  struct hpclang_rpl_reference_edit *edits, unsigned *count)
{
    if (!tokens || !resolve || !edits || !count)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (current >= 0x100000u || replacement >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_reference_edit plan[5];
    unsigned used = 0;
    for (unsigned i = 0; i < 5; ++i) {
        if (!tokens[i])
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        struct hpclang_rpl_code_origin_cell cell = {0};
        int result = resolve(context, tokens[i], &cell);
        if (result)
            return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        if (cell.field >= 0x100000u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t expected = (current >> (4 * i)) & 15u;
        uint32_t value = (replacement >> (4 * i)) & 15u;
        if (cell.expected != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        unsigned at = 0;
        while (at < used && (plan[at].mapping < cell.mapping ||
                             (plan[at].mapping == cell.mapping && plan[at].field < cell.field)))
            ++at;
        if (at < used && plan[at].mapping == cell.mapping && plan[at].field == cell.field) {
            if (plan[at].expected != expected || plan[at].value != value)
                return HPCLANG_RPL_SPAN_MALFORMED;
            continue;
        }
        for (unsigned j = used; j > at; --j)
            plan[j] = plan[j - 1];
        plan[at] =
            (struct hpclang_rpl_reference_edit){cell.mapping, cell.field, expected, value, 1};
        ++used;
    }
    unsigned packed = 0;
    for (unsigned i = 0; i < used; ++i) {
        if (packed && plan[packed - 1].mapping == plan[i].mapping &&
            plan[packed - 1].field + plan[packed - 1].width == plan[i].field) {
            struct hpclang_rpl_reference_edit *previous = &plan[packed - 1];
            previous->expected |= plan[i].expected << (4 * previous->width);
            previous->value |= plan[i].value << (4 * previous->width);
            ++previous->width;
        } else {
            plan[packed++] = plan[i];
        }
    }
    for (unsigned i = 0; i < packed; ++i)
        edits[i] = plan[i];
    *count = packed;
    return 0;
}

struct instruction_cursor
{
    const struct hpclang_rpl_reader *reader;
    uint32_t at, limit, used;
    uint8_t code[21]; /* LA can contain sixteen immediate nibbles. */
};

static int instruction_take(struct instruction_cursor *cursor, unsigned count)
{
    if (count > cursor->limit || count > sizeof cursor->code)
        return HPCLANG_RPL_SPAN_BOUNDS;
    while (cursor->used < count) {
        uint8_t nibble;
        if (!cursor->reader->read_nibble(cursor->reader->context, cursor->at + cursor->used,
                                         &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        cursor->code[cursor->used++] = nibble;
    }
    return 0;
}

static int instruction_size(struct instruction_cursor *cursor, unsigned *size)
{
    int result = instruction_take(cursor, 2);
    if (result)
        return result;
    const uint8_t *op = cursor->code;
    switch (op[0]) {
    case 0:
        *size = op[1] == 14 ? 4 : 2;
        return 0;
    case 1:
        if (op[1] <= 2) {
            result = instruction_take(cursor, 3);
            if (result)
                return result;
            if ((op[2] & 7) > 4)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
        switch (op[1]) {
        case 5:
        case 9:
        case 13:
            *size = 4;
            break;
        case 10:
        case 14:
            *size = 6;
            break;
        case 11:
        case 15:
            *size = 7;
            break;
        default:
            *size = 3;
            break;
        }
        return 0;
    case 2:
        *size = 2;
        return 0;
    case 3:
        *size = op[1] + 3;
        return 0;
    case 4:
    case 5:
        *size = 3;
        return 0;
    case 6:
    case 7:
        *size = 4;
        return 0;
    case 9:
        *size = 5;
        return 0;
    case 10:
    case 11:
        *size = 3;
        return 0;
    case 12:
    case 13:
    case 14:
    case 15:
        *size = 2;
        return 0;
    default:
        break; /* 8: extended instructions. */
    }
    if (op[1] >= 12) {
        *size = (op[1] & 1) ? 7 : 6;
        return 0;
    }
    if (op[1] >= 2) {
        *size = (op[1] == 2 || op[1] == 4 || op[1] == 5) ? 3 : 5;
        return 0;
    }
    result = instruction_take(cursor, 3);
    if (result)
        return result;
    if (op[1] == 1) {
        if (op[2] < 8 || op[2] >= 12) {
            *size = 3;
            return 0;
        }
        *size = op[2] == 11 ? 4 : op[2] == 9 ? 5 : 6;
        result = instruction_take(cursor, *size);
        if (result)
            return result;
        if ((op[2] == 8 && (op[4] & 7) > 3) || (op[2] == 9 && op[4] > 3) ||
            (op[2] == 10 && (op[4] > 2 || (op[5] & 7) > 4)) ||
            (op[2] == 11 && (op[3] < 2 || op[3] > 7)))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        return 0;
    }
    if (op[2] == 11) {
        /* Saturn+ facts: Eric Rechlin's newopcodes.txt and the pinned ROM
         * 80B26 wrapper described below. Unknown selectors remain unsupported. */
        result = instruction_take(cursor, 5);
        if (result)
            return result;
        if (op[4] == 8) {
            result = instruction_take(cursor, 7);
            if (result)
                return result;
            unsigned custom = op[6] >= 8 && op[6] <= 14;
            unsigned pair = op[5];
            unsigned cross = pair == 3 || pair == 7 || pair == 12 || pair == 13;
            if ((op[3] == 0 &&
                 (cross || (custom && pair != 5 && pair != 10 && pair != 14 && pair != 15))) ||
                (op[3] == 1 && (cross || custom)) || op[3] == 3 || op[3] == 4 || op[3] == 5 ||
                op[3] == 8 || op[3] == 9 || op[3] == 10) {
                *size = 7;
                return 0;
            }
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
        unsigned service = (op[3] << 4) | op[4];
        switch (service) {
        case 0x00:
        case 0x01:
        case 0x03:
        case 0x05:
        case 0x06:
        case 0x11:
        case 0x13:
        case 0x15:
        case 0x16:
        case 0x21:
        case 0x23:
        /* HP49g+/HP50g: service table 0x41440, native selector 0x62,
         * wrapper 0x30ee4. Receives PC+5, saves that continuation separately
         * from helper 0x306a0's data arguments, and returns it unchanged.
         * This proves framing only, not helper effects or relocation safety. */
        case 0x26:
        case 0x31:
        case 0x33:
        case 0x41:
        case 0x43:
        case 0x50:
        case 0x51:
        case 0x60:
        case 0x65:
        case 0x70:
        case 0x71:
        case 0x80:
        case 0x92:
        case 0xa0:
        case 0xa2:
        case 0xb0:
        case 0xb2:
        case 0xc2:
        case 0xfe:
        case 0xff:
            *size = 5;
            return 0;
        case 0x82:
            *size = 6;
            return 0;
        default:
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
    }
    if (op[2] != 8) {
        *size = op[2] == 12 || op[2] == 13 || op[2] == 15 ? 4 : 3;
        return 0;
    }
    result = instruction_take(cursor, 4);
    if (result)
        return result;
    switch (op[3]) {
    case 1:
    case 2:
        result = instruction_take(cursor, 5);
        if (result)
            return result;
        if (op[3] == 1 && op[4])
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        *size = op[3] == 1 ? 5 : op[4] + 6;
        return 0;
    case 4:
    case 5:
    case 8:
    case 9:
        *size = 5;
        return 0;
    case 6:
    case 7:
    case 10:
    case 11:
        *size = 7;
        return 0;
    default:
        *size = 4;
        return 0;
    }
}

int hpclang_rpl_code_extent(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                            uint32_t limit, uint32_t *length)
{
    if (!reader || !reader->read_nibble || !length)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result)
        return result;
    result = instruction_take(&cursor, size);
    if (result)
        return result;
    *length = size;
    return 0;
}

int hpclang_rpl_code_visit(const struct hpclang_rpl_reader *reader, uint32_t first, uint32_t length,
                           hpclang_rpl_code_visit_fn visit, void *context)
{
    if (!reader || !reader->read_nibble || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (first >= 0x100000u || length > 0x100000u - first)
        return HPCLANG_RPL_SPAN_BOUNDS;
    while (length) {
        uint32_t size;
        int result = hpclang_rpl_code_extent(reader, first, length, &size);
        if (result)
            return result;
        result = visit(context, first, size);
        if (result)
            return result;
        first += size;
        length -= size;
    }
    return 0;
}

int hpclang_rpl_code_literal_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                     uint32_t limit, unsigned p,
                                     struct hpclang_rpl_code_literal *literal)
{
    if (!reader || !reader->read_nibble || !literal)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result || (result = instruction_take(&cursor, size)))
        return result;
    const uint8_t *op = cursor.code;
    unsigned prefix;
    enum hpclang_rpl_code_register destination;
    if (op[0] == 3) {
        prefix = 2;
        destination = HPCLANG_RPL_CODE_C;
    } else if (op[0] == 8 && op[1] == 0 && op[2] == 8 && op[3] == 2) {
        prefix = 5;
        destination = HPCLANG_RPL_CODE_A;
    } else if (op[0] == 1 && (op[1] == 9 || op[1] == 10 || op[1] == 11 || op[1] == 13 ||
                              op[1] == 14 || op[1] == 15)) {
        prefix = 2;
        destination = op[1] < 12 ? HPCLANG_RPL_CODE_D0 : HPCLANG_RPL_CODE_D1;
        p = 0; /* Address literals always replace low nibbles, independent of P. */
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    struct hpclang_rpl_code_literal next = {.instruction = instruction,
                                            .length = size,
                                            .field = instruction + prefix,
                                            .destination = destination,
                                            .nibbles = size - prefix};
    for (unsigned i = 0; i < next.nibbles; ++i) {
        unsigned shift = 4 * ((p + i) & 15);
        next.mask |= UINT64_C(15) << shift;
        next.value |= (uint64_t)op[prefix + i] << shift;
    }
    *literal = next;
    return 0;
}

int hpclang_rpl_code_assignment_inspect(const struct hpclang_rpl_reader *reader,
                                        uint32_t instruction, uint32_t limit, unsigned p,
                                        struct hpclang_rpl_code_assignment *assignment)
{
    if (!reader || !reader->read_nibble || !assignment)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result || (result = instruction_take(&cursor, size)))
        return result;
    const uint8_t *op = cursor.code;
    struct hpclang_rpl_code_assignment next = {.instruction = instruction, .length = size};
    unsigned field = 7;
    if (op[0] == 8 && op[1] == 1 && op[2] == 11 && (op[3] == 4 || op[3] == 5)) {
        if (instruction > 0xffffbu)
            return HPCLANG_RPL_SPAN_BOUNDS;
        next.source = HPCLANG_RPL_CODE_NEXT_PC;
        next.destination = op[3] == 4 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C;
        next.nibbles = 5;
        next.constant = instruction + 4;
    } else if (op[0] == 1 && op[1] == 3 && !(op[2] & 2)) {
        next.source = (op[2] & 4) ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A;
        next.destination = (op[2] & 1) ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0;
        next.nibbles = (op[2] & 8) ? 4 : 5;
    } else if (op[0] == 2) {
        next.source = HPCLANG_RPL_CODE_CONSTANT;
        next.destination = HPCLANG_RPL_CODE_P;
        next.constant = op[1];
        next.nibbles = 1;
    } else if (op[0] == 8 && op[1] == 0 && (op[2] == 12 || op[2] == 13)) {
        next.source = op[2] == 12 ? HPCLANG_RPL_CODE_P : HPCLANG_RPL_CODE_C;
        next.destination = op[2] == 12 ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_P;
        next.source_first = op[2] == 13 ? op[3] : 0;
        next.destination_first = op[2] == 12 ? op[3] : 0;
        next.nibbles = 1;
    } else if ((op[0] == 1 && op[1] < 2) ||
               (op[0] == 8 && op[1] == 1 && op[2] == 10 && op[4] < 2)) {
        unsigned selector = op[0] == 1 ? op[2] : op[5];
        unsigned from_saved = op[0] == 1 ? op[1] : op[4];
        enum hpclang_rpl_code_register working =
            selector < 8 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C;
        enum hpclang_rpl_code_register saved = HPCLANG_RPL_CODE_R0 + (selector & 7);
        next.source = from_saved ? saved : working;
        next.destination = from_saved ? working : saved;
        field = op[0] == 1 ? 7 : op[3];
    } else if (op[0] == 13 || (op[0] == 10 && op[1] >= 8)) {
        unsigned selector = op[0] == 13 ? op[1] : op[2];
        if (selector >= 12)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        field = op[0] == 13 ? 15 : op[1] - 8;
        if (selector < 4) {
            next.source = HPCLANG_RPL_CODE_CONSTANT;
            next.destination = (enum hpclang_rpl_code_register)selector;
        } else {
            static const uint8_t destinations[] = {0, 1, 2, 3, 1, 2, 0, 2};
            static const uint8_t sources[] = {1, 2, 0, 2, 0, 1, 2, 3};
            next.source = (enum hpclang_rpl_code_register)sources[selector - 4];
            next.destination = (enum hpclang_rpl_code_register)destinations[selector - 4];
        }
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    if (!next.nibbles) {
        static const unsigned first[] = {0, 0, 2, 0, 15, 3, 0, 0};
        static const unsigned width[] = {1, 1, 1, 3, 1, 12, 2, 16};
        if (field >= 8 && field != 15)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        next.destination_first = field == 15 ? 0 : field == 0 ? p : first[field];
        next.source_first = next.source == HPCLANG_RPL_CODE_CONSTANT ? 0 : next.destination_first;
        next.nibbles = field == 15 ? 5 : field == 1 ? p + 1 : width[field];
    }
    *assignment = next;
    return 0;
}

int hpclang_rpl_code_memory_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                    uint32_t limit, unsigned p,
                                    struct hpclang_rpl_code_memory *memory)
{
    if (!reader || !reader->read_nibble || !memory)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (!result)
        result = instruction_take(&cursor, size);
    if (result)
        return result;
    const uint8_t *op = cursor.code;
    if (op[0] != 1 || (op[1] != 4 && op[1] != 5))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned selector = op[2];
    struct hpclang_rpl_code_memory next = {
        .instruction = instruction,
        .length = size,
        .data = selector & 4 ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A,
        .address = selector & 1 ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0,
        .load = (selector & 2) != 0,
    };
    if (op[1] == 5 && selector >= 8) {
        next.nibbles = op[3] + 1u;
    } else {
        unsigned field = op[1] == 4 ? (selector < 8 ? 15 : 6) : op[3];
        static const unsigned first[] = {0, 0, 2, 0, 15, 3, 0, 0};
        static const unsigned width[] = {1, 1, 1, 3, 1, 12, 2, 16};
        if (field >= 8 && field != 15)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        next.first = field == 15 ? 0 : field == 0 ? p : first[field];
        next.nibbles = field == 15 ? 5 : field == 1 ? p + 1 : width[field];
    }
    *memory = next;
    return 0;
}

int hpclang_rpl_code_address_adjust_inspect(const struct hpclang_rpl_reader *reader,
                                            uint32_t instruction, uint32_t limit,
                                            struct hpclang_rpl_code_address_adjust *adjust)
{
    if (!reader || !reader->read_nibble || !adjust)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    int result = instruction_take(&cursor, 2);
    if (result)
        return result;
    unsigned opcode = cursor.code[1];
    if (cursor.code[0] != 1 || (opcode != 6 && opcode != 7 && opcode != 8 && opcode != 12))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if ((result = instruction_take(&cursor, 3)))
        return result;
    struct hpclang_rpl_code_address_adjust next = {
        .instruction = instruction,
        .length = 3,
        .address = opcode == 6 || opcode == 8 ? HPCLANG_RPL_CODE_D0 : HPCLANG_RPL_CODE_D1,
        .delta = ((int32_t)cursor.code[2] + 1) * (opcode < 8 ? 1 : -1),
    };
    *adjust = next;
    return 0;
}

static unsigned origin_register_width(enum hpclang_rpl_code_register reg)
{
    if ((unsigned)reg <= HPCLANG_RPL_CODE_R4)
        return 16;
    if (reg == HPCLANG_RPL_CODE_P)
        return 1;
    if (reg == HPCLANG_RPL_CODE_D0 || reg == HPCLANG_RPL_CODE_D1)
        return 5;
    return 0;
}

static int origin_field_valid(enum hpclang_rpl_code_register reg, unsigned first, unsigned count)
{
    unsigned width = origin_register_width(reg);
    return count && first < width && count <= width - first;
}

int hpclang_rpl_code_origins_literal(struct hpclang_rpl_code_origins *state,
                                     const struct hpclang_rpl_code_literal *literal, unsigned p,
                                     const uint32_t *encoded, unsigned count)
{
    if (!state || !literal || !encoded)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (p > 15 || !count || count > 16 || count != literal->nibbles)
        return HPCLANG_RPL_SPAN_MALFORMED;
    unsigned first;
    if (literal->destination == HPCLANG_RPL_CODE_A || literal->destination == HPCLANG_RPL_CODE_C) {
        first = p;
    } else if ((literal->destination == HPCLANG_RPL_CODE_D0 ||
                literal->destination == HPCLANG_RPL_CODE_D1) &&
               (count == 2 || count == 4 || count == 5)) {
        first = 0;
    } else {
        return HPCLANG_RPL_SPAN_MALFORMED;
    }
    uint64_t mask = 0;
    for (unsigned i = 0; i < count; ++i)
        mask |= UINT64_C(15) << (4 * ((first + i) & 15));
    if (literal->mask != mask || (literal->value & ~mask))
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t source[16];
    uint32_t digits = 0;
    for (unsigned i = 0; i < count; ++i)
        source[i] = encoded[i];
    for (unsigned i = 0; i < count; ++i)
        digits |= 1u << ((first + i) & 15);
    origin_overwrite(state, literal->destination, digits);
    for (unsigned i = 0; i < count; ++i)
        state->registers[literal->destination][(first + i) & 15] = source[i];
    return 0;
}

int hpclang_rpl_code_origins_assign(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_assignment *assignment,
                                    const uint32_t *next_pc)
{
    if (!state || !assignment)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    unsigned count = assignment->nibbles;
    if (!origin_field_valid(assignment->destination, assignment->destination_first, count))
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (assignment->source == HPCLANG_RPL_CODE_NEXT_PC) {
        if (assignment->source_first || assignment->destination_first || count != 5 ||
            (assignment->destination != HPCLANG_RPL_CODE_A &&
             assignment->destination != HPCLANG_RPL_CODE_C))
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (!next_pc)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
    } else if (assignment->source == HPCLANG_RPL_CODE_CONSTANT) {
        if (assignment->source_first)
            return HPCLANG_RPL_SPAN_MALFORMED;
    } else if (!origin_field_valid(assignment->source, assignment->source_first, count)) {
        return HPCLANG_RPL_SPAN_MALFORMED;
    }
    uint32_t source[16];
    uint32_t adjustment = assignment->source == HPCLANG_RPL_CODE_CONSTANT ||
                                  assignment->source == HPCLANG_RPL_CODE_NEXT_PC
                              ? 0
                              : state->adjustment[assignment->source];
    int whole = !assignment->source_first && !assignment->destination_first && count >= 5;
    for (unsigned i = 0; i < count; ++i)
        source[i] = assignment->source == HPCLANG_RPL_CODE_CONSTANT  ? 0
                    : assignment->source == HPCLANG_RPL_CODE_NEXT_PC ? next_pc[i]
                    : adjustment && !whole && assignment->source_first + i < 5
                        ? 0
                        : state->registers[assignment->source][assignment->source_first + i];
    origin_overwrite(state, assignment->destination,
                     ((1u << count) - 1) << assignment->destination_first);
    for (unsigned i = 0; i < count; ++i)
        state->registers[assignment->destination][assignment->destination_first + i] = source[i];
    if (whole)
        state->adjustment[assignment->destination] = adjustment;
    return 0;
}

int hpclang_rpl_code_origins_exchange(struct hpclang_rpl_code_origins *state,
                                      const struct hpclang_rpl_code_exchange *exchange)
{
    if (!state || !exchange)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!origin_field_valid(exchange->left, exchange->first, exchange->nibbles) ||
        !origin_field_valid(exchange->right, exchange->first, exchange->nibbles))
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t left[16], right[16];
    uint32_t left_adjust = state->adjustment[exchange->left];
    uint32_t right_adjust = state->adjustment[exchange->right];
    int whole = !exchange->first && exchange->nibbles >= 5;
    for (unsigned i = 0; i < exchange->nibbles; ++i) {
        left[i] = left_adjust && !whole && exchange->first + i < 5
                      ? 0
                      : state->registers[exchange->left][exchange->first + i];
        right[i] = right_adjust && !whole && exchange->first + i < 5
                       ? 0
                       : state->registers[exchange->right][exchange->first + i];
    }
    uint32_t digits = ((1u << exchange->nibbles) - 1) << exchange->first;
    origin_overwrite(state, exchange->left, digits);
    origin_overwrite(state, exchange->right, digits);
    for (unsigned i = 0; i < exchange->nibbles; ++i) {
        state->registers[exchange->left][exchange->first + i] = right[i];
        state->registers[exchange->right][exchange->first + i] = left[i];
    }
    if (whole) {
        state->adjustment[exchange->left] = right_adjust;
        state->adjustment[exchange->right] = left_adjust;
    }
    return 0;
}

int hpclang_rpl_code_origins_memory(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_memory *transfer,
                                    uint32_t *memory, unsigned count)
{
    if (!state || !transfer || !memory)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if ((transfer->data != HPCLANG_RPL_CODE_A && transfer->data != HPCLANG_RPL_CODE_C) ||
        (transfer->address != HPCLANG_RPL_CODE_D0 && transfer->address != HPCLANG_RPL_CODE_D1) ||
        transfer->load > 1 || transfer->nibbles != count ||
        !origin_field_valid(transfer->data, transfer->first, count))
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t source[16];
    uint32_t *registers = state->registers[transfer->data] + transfer->first;
    for (unsigned i = 0; i < count; ++i)
        source[i] = transfer->load                                                 ? memory[i]
                    : state->adjustment[transfer->data] && transfer->first + i < 5 ? 0
                                                                                   : registers[i];
    if (transfer->load)
        origin_overwrite(state, transfer->data, ((1u << count) - 1) << transfer->first);
    for (unsigned i = 0; i < count; ++i)
        if (transfer->load)
            registers[i] = source[i];
        else
            memory[i] = source[i];
    return 0;
}

int hpclang_rpl_code_origins_adjust(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_address_adjust *adjust)
{
    if (!state || !adjust)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if ((adjust->address != HPCLANG_RPL_CODE_D0 && adjust->address != HPCLANG_RPL_CODE_D1) ||
        !adjust->delta || adjust->delta < -16 || adjust->delta > 16)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (state->adjustment[adjust->address] > 0xfffffu)
        return HPCLANG_RPL_SPAN_MALFORMED;
    state->adjustment[adjust->address] =
        (state->adjustment[adjust->address] + (uint32_t)adjust->delta) & 0xfffffu;
    return 0;
}

int hpclang_rpl_code_exchange_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                      uint32_t limit, unsigned p,
                                      struct hpclang_rpl_code_exchange *exchange)
{
    if (!reader || !reader->read_nibble || !exchange)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result || (result = instruction_take(&cursor, size)))
        return result;
    const uint8_t *op = cursor.code;
    struct hpclang_rpl_code_exchange next = {.instruction = instruction, .length = size};
    unsigned field;
    if (op[0] == 1 && op[1] == 3 && (op[2] & 2)) {
        next.left = (op[2] & 4) ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A;
        next.right = (op[2] & 1) ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0;
        next.nibbles = (op[2] & 8) ? 4 : 5;
        *exchange = next;
        return 0;
    }
    if ((op[0] == 1 && op[1] == 2) || (op[0] == 8 && op[1] == 1 && op[2] == 10 && op[4] == 2)) {
        unsigned selector = op[0] == 1 ? op[2] : op[5];
        next.left = selector < 8 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C;
        next.right = HPCLANG_RPL_CODE_R0 + (selector & 7);
        field = op[0] == 1 ? 7 : op[3];
    } else if (op[0] == 13 || (op[0] == 10 && op[1] >= 8)) {
        unsigned selector = op[0] == 13 ? op[1] : op[2];
        if (selector < 12)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        static const uint8_t left[] = {0, 1, 0, 2};
        static const uint8_t right[] = {1, 2, 2, 3};
        next.left = (enum hpclang_rpl_code_register)left[selector - 12];
        next.right = (enum hpclang_rpl_code_register)right[selector - 12];
        field = op[0] == 13 ? 15 : op[1] - 8;
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    static const unsigned first[] = {0, 0, 2, 0, 15, 3, 0, 0};
    static const unsigned width[] = {1, 1, 1, 3, 1, 12, 2, 16};
    if (field >= 8 && field != 15)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    next.first = field == 15 ? 0 : field == 0 ? p : first[field];
    next.nibbles = field == 15 ? 5 : field == 1 ? p + 1 : width[field];
    *exchange = next;
    return 0;
}

static int read_word(const struct hpclang_rpl_reader *reader, uint32_t at, unsigned width,
                     uint32_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, at + i, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        *value |= (uint32_t)nibble << (4 * i);
    }
    return 0;
}

int hpclang_rpl_code_control_step(const struct hpclang_rpl_reader *instructions, uint32_t limit,
                                  struct hpclang_rpl_code_control *state,
                                  struct hpclang_rpl_code_return_use *use)
{
    if (!instructions || !instructions->read_nibble || !state || !use)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (state->carry > 1 || state->cursor >= 8)
        return HPCLANG_RPL_SPAN_MALFORMED;
    for (unsigned slot = 0; slot < 8; ++slot)
        if (state->stack_generated[slot] > 0x100000u)
            return HPCLANG_RPL_SPAN_MALFORMED;
    if (state->pc > 0xffffe || limit < 2 || limit > 0x100000u - state->pc)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(instructions, state->pc, 2, &opcode);
    if (result)
        return result;
    struct hpclang_rpl_code_control next = *state;
    struct hpclang_rpl_code_return_use consumed = {0};
    int pop = opcode == 0x10 || opcode == 0x20 || opcode == 0x30;
    if (pop) {
        if (opcode != 0x10)
            next.carry = opcode == 0x20;
    } else if ((opcode & 15u) == 4 || (opcode & 15u) == 5) {
        if (limit < 3)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t displacement;
        result = read_word(instructions, state->pc + 1, 2, &displacement);
        if (result)
            return result;
        if (state->carry != ((opcode & 15u) == 4)) {
            next.pc = (state->pc + 3) & 0xfffffu;
        } else if (!displacement) {
            pop = 1;
        } else {
            int32_t delta =
                displacement < 128 ? (int32_t)displacement : (int32_t)displacement - 256;
            next.pc = (uint32_t)((int32_t)state->pc + 1 + delta) & 0xfffffu;
        }
    } else {
        /* Other predicates need state not represented by this contract. */
        if ((opcode & 15u) != 6 && (opcode & 15u) != 7 && opcode != 0xc8 && opcode != 0xd8 &&
            opcode != 0xe8 && opcode != 0xf8)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        struct hpclang_rpl_code_branch branch;
        result = hpclang_rpl_code_branch_inspect(instructions, state->pc, limit, &branch);
        if (result)
            return result;
        if (branch.call) {
            next.stack[next.cursor] = (state->pc + branch.length) & 0xfffffu;
            next.stack_generated[next.cursor] = state->pc + 1;
            for (unsigned i = 0; i < 5; ++i)
                next.stack_origins[next.cursor][i] = 0;
            next.cursor = (next.cursor + 1) & 7u;
        }
        next.pc = branch.target;
    }
    if (pop) {
        next.cursor = (next.cursor + 7) & 7u;
        if (next.stack[next.cursor] > 0xfffff)
            return HPCLANG_RPL_SPAN_MALFORMED;
        next.pc = next.stack[next.cursor];
        consumed.present = 1;
        consumed.target = next.pc;
        consumed.generated = next.stack_generated[next.cursor];
        for (unsigned i = 0; i < 5; ++i) {
            if (consumed.generated && next.stack_origins[next.cursor][i])
                return HPCLANG_RPL_SPAN_MALFORMED;
            consumed.origins[i] = next.stack_origins[next.cursor][i];
            next.stack_origins[next.cursor][i] = 0;
        }
        next.stack[next.cursor] = 0;
        next.stack_generated[next.cursor] = 0;
    }
    *state = next;
    *use = consumed;
    return 0;
}

int hpclang_rpl_code_pc_exchange_inspect(const struct hpclang_rpl_reader *reader,
                                         uint32_t instruction, uint32_t limit, uint32_t a_low,
                                         uint32_t c_low,
                                         struct hpclang_rpl_code_pc_exchange *exchange)
{
    if (!reader || !reader->read_nibble || !exchange)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction > 0xffffbu || limit < 4 || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 4, &opcode);
    if (result)
        return result;
    if (opcode != 0x6b18u && opcode != 0x7b18u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    int selects_c = opcode == 0x7b18u;
    struct hpclang_rpl_code_pc_exchange next = {
        .capture = {.instruction = instruction,
                    .length = 4,
                    .source = HPCLANG_RPL_CODE_NEXT_PC,
                    .destination = selects_c ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A,
                    .nibbles = 5,
                    .constant = instruction + 4},
        .target = (selects_c ? c_low : a_low) & 0xfffffu};
    *exchange = next;
    return 0;
}

int hpclang_rpl_code_indirect_inspect(const struct hpclang_rpl_reader *instructions,
                                      const struct hpclang_rpl_reader *pointers,
                                      uint32_t instruction, uint32_t limit, uint32_t a_low,
                                      uint32_t c_low, struct hpclang_rpl_code_indirect *transfer)
{
    if (!instructions || !instructions->read_nibble || !transfer)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || limit < 4 || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(instructions, instruction, 4, &opcode);
    if (result)
        return result;
    struct hpclang_rpl_code_indirect next = {0};
    next.instruction = instruction;
    switch (opcode) {
    case 0x2b18:
        break; /* PC=A */
    case 0x3b18:
        next.register_c = 1;
        break;
    case 0xc808:
        next.via_memory = 1;
        break;
    case 0xe808:
        next.register_c = next.via_memory = 1;
        break;
    default:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    next.register_value = (next.register_c ? c_low : a_low) & 0xfffffu;
    next.target = next.register_value;
    if (next.via_memory) {
        if (!pointers || !pointers->read_nibble)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (next.register_value > 0xffffbu)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(pointers, next.register_value, 5, &next.target);
        if (result)
            return result;
    }
    *transfer = next;
    return 0;
}

int hpclang_rpl_code_branch_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                    uint32_t limit, struct hpclang_rpl_code_branch *branch)
{
    if (!reader || !reader->read_nibble || !branch)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 1, &opcode);
    if (result)
        return result;
    struct hpclang_rpl_code_branch next = {0};
    next.instruction = instruction;
    next.field = instruction + 1;
    if (opcode == 4 || opcode == 5) {
        next.width = 2;
        next.conditional = 1;
    } else if (opcode == 6 || opcode == 7) {
        next.width = 3;
        next.call = opcode == 7;
    } else if (opcode == 9) {
        next.field = instruction + 3;
        next.width = 2;
        next.conditional = 1;
    } else if (opcode == 8) {
        if (limit < 2)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(reader, instruction + 1, 1, &opcode);
        if (result)
            return result;
        if (opcode >= 12) {
            ++next.field;
            next.width = (opcode & 1) ? 5 : 4;
            next.call = opcode >= 14;
        } else if (opcode == 3 || (opcode >= 6 && opcode <= 11)) {
            next.field = instruction + 3;
            next.width = 2;
            next.conditional = 1;
        } else if (opcode == 0) {
            if (limit < 4)
                return HPCLANG_RPL_SPAN_BOUNDS;
            result = read_word(reader, instruction + 2, 2, &opcode);
            if (result)
                return result;
            if (opcode != 0x68 && opcode != 0x78 && opcode != 0xa8 && opcode != 0xb8)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            next.field = instruction + 5;
            next.width = 2;
            next.conditional = 1;
        } else {
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    next.length = next.field - instruction + next.width;
    if (next.length > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;
    /* Predicate operands are part of the proven instruction, including fields
     * and bit indices. Validate them even though relocation preserves them. */
    uint32_t prefix;
    result = read_word(reader, instruction, next.field - instruction, &prefix);
    if (result)
        return result;
    result = read_word(reader, next.field, next.width, &next.encoded);
    if (result)
        return result;
    if (next.conditional && !next.encoded)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (next.width == 5) {
        next.target = next.encoded;
        *branch = next;
        return 0;
    }
    next.base = (next.field + (next.call ? next.width : 0)) & 0xfffffu;
    uint32_t range = 1u << (4 * next.width);
    int32_t displacement = (int32_t)next.encoded;
    if (next.encoded >= range / 2)
        displacement -= (int32_t)range;
    next.target = (uint32_t)((int32_t)next.base + displacement) & 0xfffffu;
    *branch = next;
    return 0;
}

struct bank_entry_reader
{
    const struct hpclang_rpl_reader *reader;
    uint32_t first, end;
    hpclang_rpl_reference_dependency_fn dependency;
    void *context;
    int error;
};

static int bank_entry_read(void *context, uint32_t address, uint8_t *value)
{
    struct bank_entry_reader *state = context;
    if (state->error)
        return 0;
    if (address < state->first || address >= state->end) {
        state->error = HPCLANG_RPL_SPAN_BOUNDS;
        return 0;
    }
    if (!state->reader->read_nibble(state->reader->context, address, value))
        return 0;
    if (*value <= 15u) {
        state->error = state->dependency(state->context, address, *value);
        if (state->error > 0)
            state->error = HPCLANG_RPL_SPAN_MALFORMED;
    }
    return !state->error;
}

static int bank_entry_inspect(const struct hpclang_rpl_reader *reader, uint32_t end, uint32_t entry,
                              unsigned p, struct hpclang_rpl_code_bank_entry *view)
{
    struct hpclang_rpl_code_literal literal;
    int result = hpclang_rpl_code_literal_inspect(reader, entry, end - entry, 0, &literal);
    if (result)
        return result;
    if (literal.destination != HPCLANG_RPL_CODE_D0 || literal.nibbles != 5)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (literal.value > 0xffffbu || end - entry < 11u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    result = read_word(reader, entry + 7, 1, &opcode);
    if (result)
        return result;
    if (opcode != 6)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_code_branch branch;
    result = hpclang_rpl_code_branch_inspect(reader, entry + 7, 4, &branch);
    if (result)
        return result;
    uint32_t dispatch = branch.target;
    if (dispatch >= end || end - dispatch < 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t selected = dispatch;
    unsigned compared;
    for (compared = 0; compared < 16; ++compared) {
        if (selected >= end || end - selected < 5)
            return HPCLANG_RPL_SPAN_BOUNDS;
        /* Each untaken ?P= test falls through without changing P. Capture
         * its complete encoding, including the unselected displacement. */
        result = read_word(reader, selected, 5, &opcode);
        if (result)
            return result;
        if ((opcode & 0xffu) != 0x98u)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (((opcode >> 8u) & 15u) == p)
            break;
        selected += 5;
    }
    if (compared == 16)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    result = hpclang_rpl_code_branch_inspect(reader, selected, 5, &branch);
    if (result)
        return result;
    uint32_t body_address = branch.target;
    if (p == 1) {
        if (body_address >= end || end - body_address < 7u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        /* The exit path replaces the entry's selector. It does not promise
         * restoration of the caller's previous bank. */
        result =
            hpclang_rpl_code_literal_inspect(reader, body_address, end - body_address, p, &literal);
        if (result)
            return result;
        if (literal.destination != HPCLANG_RPL_CODE_D0 || literal.nibbles != 5)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (literal.value > 0xffffbu || end - body_address < 7u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        body_address += 7;
    }
    static const uint8_t body[] = {
        2, 0, 3, 4, 0, 0, 0, 15, 15, 8, 0, 5, 1, 4, 6, 1, 3, 4, 8, 0, 5, 1, 4, 12, 8, 0, 4, 0, 3,
    };
    if (body_address >= end || sizeof body > end - body_address)
        return HPCLANG_RPL_SPAN_BOUNDS;
    for (unsigned i = 0; i < sizeof body; ++i) {
        result = read_word(reader, body_address + i, 1, &opcode);
        if (result)
            return result;
        if (opcode != body[i])
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    *view = (struct hpclang_rpl_code_bank_entry){(uint32_t)literal.value, dispatch, body_address};
    return 0;
}

int hpclang_rpl_code_bank_entry_inspect(const struct hpclang_rpl_reader *reader, uint32_t first,
                                        uint32_t end, uint32_t entry, unsigned p,
                                        struct hpclang_rpl_code_bank_entry *view,
                                        hpclang_rpl_reference_dependency_fn dependency,
                                        void *context)
{
    if (!reader || !reader->read_nibble || !view || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (first >= end || end > 0x100000u || entry < first || entry >= end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (p > 1)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct bank_entry_reader state = {reader, first, end, dependency, context, 0};
    const struct hpclang_rpl_reader observed = {bank_entry_read, &state};
    struct hpclang_rpl_code_bank_entry next;
    int result = bank_entry_inspect(&observed, end, entry, p, &next);
    if (state.error)
        return state.error;
    if (!result)
        *view = next;
    return result;
}

static int plan_shift(const struct hpclang_rpl_reader *reader, uint32_t instruction, uint32_t limit,
                      const struct hpclang_rpl_reference_mappings *mappings, uint32_t first,
                      uint32_t end, int32_t delta, struct hpclang_rpl_code_patch *patch,
                      int pointer)
{
    if (!mappings || !patch)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (first >= end || end > 0x100000u || (int64_t)first + delta < 0 ||
        (int64_t)end + delta > 0x100000)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_code_branch branch;
    int result;
    if (pointer) {
        if (!reader || !reader->read_nibble)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (instruction >= 0x100000u || limit < 7 || limit > 0x100000u - instruction)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t opcode, target;
        result = read_word(reader, instruction, 2, &opcode);
        if (result)
            return result;
        if (opcode != 0xb1 && opcode != 0xf1)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        result = read_word(reader, instruction + 2, 5, &target);
        branch = (struct hpclang_rpl_code_branch){
            instruction, 7, instruction + 2, 5, 0, target, target, 0, 0};
    } else {
        result = hpclang_rpl_code_branch_inspect(reader, instruction, limit, &branch);
    }
    if (result)
        return result;
    int source_moves = 0;
    if (mappings->field == mappings->moving && instruction < end &&
        first < instruction + branch.length) {
        if (instruction < first || instruction + branch.length > end)
            return HPCLANG_RPL_SPAN_BOUNDS;
        source_moves = 1;
    }
    int target_moves =
        mappings->target == mappings->moving && branch.target >= first && branch.target < end;
    uint32_t base = (uint32_t)((int64_t)branch.base + (source_moves ? delta : 0)) & 0xfffffu;
    uint32_t target = (uint32_t)((int64_t)branch.target + (target_moves ? delta : 0));
    if (branch.width == 5) {
        if (target == branch.encoded)
            return 0;
        *patch = (struct hpclang_rpl_code_patch){branch.field, 5, branch.encoded, target};
        return 1;
    }
    uint32_t distance = (target - base) & 0xfffffu;
    uint32_t range = 1u << (4 * branch.width);
    if (distance >= range / 2 && distance < 0x100000u - range / 2)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t value = distance & (range - 1);
    if (branch.conditional && !value)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (value == branch.encoded)
        return 0;
    *patch = (struct hpclang_rpl_code_patch){branch.field, branch.width, branch.encoded, value};
    return 1;
}

int hpclang_rpl_code_branch_plan_shift(const struct hpclang_rpl_reader *reader,
                                       uint32_t instruction, uint32_t limit,
                                       const struct hpclang_rpl_reference_mappings *mappings,
                                       uint32_t first, uint32_t end, int32_t delta,
                                       struct hpclang_rpl_code_patch *patch)
{
    return plan_shift(reader, instruction, limit, mappings, first, end, delta, patch, 0);
}

int hpclang_rpl_code_pointer_plan_shift(const struct hpclang_rpl_reader *reader,
                                        uint32_t instruction, uint32_t limit,
                                        const struct hpclang_rpl_reference_mappings *mappings,
                                        uint32_t first, uint32_t end, int32_t delta,
                                        struct hpclang_rpl_code_patch *patch)
{
    return plan_shift(reader, instruction, limit, mappings, first, end, delta, patch, 1);
}

int hpclang_rpl_code_generated_return_plan(const struct hpclang_rpl_reader *instructions,
                                           const struct hpclang_rpl_code_address_use *use,
                                           const struct hpclang_rpl_reference_mappings *mappings,
                                           uint32_t first, uint32_t end, int32_t delta,
                                           uint32_t replacement,
                                           struct hpclang_rpl_code_patch *patch)
{
    if (!instructions || !instructions->read_nibble || !use || !mappings || !patch)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!use->generated)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (use->kind != HPCLANG_RPL_CODE_USE_RETURN_TARGET || use->adjustment)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (use->generated > 0x100000u || use->value > 0xfffff || replacement > 0xfffff)
        return HPCLANG_RPL_SPAN_BOUNDS;
    for (unsigned digit = 0; digit < 5; ++digit)
        if (use->origins[digit])
            return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t instruction = use->generated - 1;
    struct hpclang_rpl_code_branch call;
    int result =
        hpclang_rpl_code_branch_inspect(instructions, instruction, 0x100000u - instruction, &call);
    if (result)
        return result;
    if (!call.call || ((instruction + call.length) & 0xfffffu) != use->value)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct hpclang_rpl_code_patch operand;
    result = hpclang_rpl_code_branch_plan_shift(instructions, instruction, 0x100000u - instruction,
                                                mappings, first, end, delta, &operand);
    if (result < 0)
        return result;
    int moves = mappings->field == mappings->moving && instruction >= first && instruction < end;
    uint32_t fallthrough =
        (uint32_t)((int64_t)instruction + call.length + (moves ? delta : 0)) & 0xfffffu;
    if (replacement != fallthrough)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (result)
        *patch = operand;
    return result;
}

int hpclang_rpl_code_execution_step_derived(const struct hpclang_rpl_reader *instructions,
                                            const struct hpclang_rpl_reader *data, uint32_t limit,
                                            struct hpclang_rpl_code_execution *state,
                                            hpclang_rpl_code_token_fn token,
                                            hpclang_rpl_code_derive_fn derive, void *context,
                                            struct hpclang_rpl_code_step_uses *uses)
{
    return hpclang_rpl_code_execution_step_effects(instructions, data, limit, state, token, derive,
                                                   0, context, uses);
}

int hpclang_rpl_code_execution_step(const struct hpclang_rpl_reader *instructions,
                                    const struct hpclang_rpl_reader *data, uint32_t limit,
                                    struct hpclang_rpl_code_execution *state,
                                    hpclang_rpl_code_token_fn token, void *context,
                                    struct hpclang_rpl_code_step_uses *uses)
{
    return hpclang_rpl_code_execution_step_derived(instructions, data, limit, state, token, 0,
                                                   context, uses);
}

int hpclang_rpl_code_execution_step_effects(const struct hpclang_rpl_reader *instructions,
                                            const struct hpclang_rpl_reader *data, uint32_t limit,
                                            struct hpclang_rpl_code_execution *state,
                                            hpclang_rpl_code_token_fn token,
                                            hpclang_rpl_code_derive_fn derive,
                                            hpclang_rpl_code_store_fn store, void *context,
                                            struct hpclang_rpl_code_step_uses *uses)
{
    return hpclang_rpl_code_execution_step_native(instructions, data, limit, state, token, derive,
                                                  store, 0, context, uses);
}

int hpclang_rpl_code_execution_step_native(
    const struct hpclang_rpl_reader *instructions, const struct hpclang_rpl_reader *data,
    uint32_t limit, struct hpclang_rpl_code_execution *state, hpclang_rpl_code_token_fn token,
    hpclang_rpl_code_derive_fn derive, hpclang_rpl_code_store_fn store,
    hpclang_rpl_code_config_fn config, void *context, struct hpclang_rpl_code_step_uses *uses)
{
    return hpclang_rpl_code_execution_step_predicates(instructions, data, limit, state, token,
                                                      derive, store, config, 0, context, uses);
}

int hpclang_rpl_code_execution_step_predicates(
    const struct hpclang_rpl_reader *instructions, const struct hpclang_rpl_reader *data,
    uint32_t limit, struct hpclang_rpl_code_execution *state, hpclang_rpl_code_token_fn token,
    hpclang_rpl_code_derive_fn derive, hpclang_rpl_code_store_fn store,
    hpclang_rpl_code_config_fn config, hpclang_rpl_code_predicate_fn predicate, void *context,
    struct hpclang_rpl_code_step_uses *uses)
{
    const struct hpclang_rpl_code_callbacks callbacks = {
        .derive = derive, .store = store, .config = config, .predicate = predicate};
    return hpclang_rpl_code_execution_step_observed(instructions, data, limit, state, token,
                                                    &callbacks, context, uses);
}
