#include "rpl-execution.h"
#include <assert.h>
#include <string.h>

struct plan_fixture
{
    uint8_t memory[128];
    unsigned calls, fail;
    int error;
};

static int read_cell(void *context, uint32_t at, uint8_t *value)
{
    struct plan_fixture *f = context;
    if (at >= sizeof f->memory)
        return 0;
    *value = f->memory[at];
    return 1;
}

static int source_token(void *context, unsigned data, uint32_t at, uint8_t value, uint32_t *token)
{
    (void)context;
    (void)data;
    (void)value;
    *token = at + 1;
    return 0;
}

static int resolve_cell(void *context, uint32_t token, struct hpclang_rpl_code_origin_cell *cell)
{
    struct plan_fixture *f = context;
    if (++f->calls == f->fail)
        return f->error;
    assert(token && token <= sizeof f->memory);
    *cell = (struct hpclang_rpl_code_origin_cell){1, token - 1, f->memory[token - 1]};
    return 0;
}

static void put(struct plan_fixture *f, unsigned at, uint32_t value)
{
    for (unsigned i = 0; i < 5; ++i)
        f->memory[at + i] = (value >> (4 * i)) & 15;
}

static void packed_payload_plans(void)
{
    struct plan_fixture f = {0};
    struct hpclang_rpl_code_store store = {.nibbles = 12};
    uint64_t replacement = 0;
    for (unsigned digit = 0; digit < 12; ++digit) {
        f.memory[32 + digit] = digit;
        store.value |= (uint64_t)digit << (4 * digit);
        replacement |= (uint64_t)(15 - digit) << (4 * digit);
        store.origins[digit] = 33 + digit;
    }
    const struct plan_fixture original = f;
    struct hpclang_rpl_reference_edit single[12], packed[3];
    struct hpclang_rpl_reference_edit_builder ordinary, compact;
    assert(!hpclang_rpl_reference_edits_begin(&ordinary, single, 12));
    assert(!hpclang_rpl_reference_edits_begin_packed(&compact, packed, 3));
    assert(!hpclang_rpl_execution_plan_payload(&ordinary, &store, replacement, resolve_cell, NULL,
                                               &f));
    assert(
        !hpclang_rpl_execution_plan_payload(&compact, &store, replacement, resolve_cell, NULL, &f));
    assert(ordinary.count == 12 && compact.count == 3);
    unsigned cell = 0;
    for (unsigned record = 0; record < compact.count; ++record)
        for (unsigned digit = 0; digit < packed[record].width; ++digit, ++cell) {
            assert(cell < ordinary.count && single[cell].mapping == packed[record].mapping);
            assert(single[cell].field == packed[record].field + digit);
            assert(single[cell].expected == ((packed[record].expected >> (4 * digit)) & 15));
            assert(single[cell].value == ((packed[record].value >> (4 * digit)) & 15));
        }
    assert(cell == ordinary.count && !memcmp(f.memory, original.memory, sizeof f.memory));
    struct hpclang_rpl_code_step_uses uses = {.count = 1};
    uses.addresses[0].kind = HPCLANG_RPL_CODE_USE_LOAD_ADDRESS;
    uses.addresses[0].value = (uint32_t)store.value & 0xfffff;
    memcpy(uses.addresses[0].origins, store.origins, sizeof uses.addresses[0].origins);
    uint32_t address = (uint32_t)replacement & 0xfffff;
    assert(!hpclang_rpl_execution_plan_uses(&compact, &uses, &address, resolve_cell, &f));
    assert(compact.count == 3);
    address ^= 0x100;
    assert(hpclang_rpl_execution_plan_uses(&compact, &uses, &address, resolve_cell, &f) < 0);
    assert(!compact.count && compact.error);
    unsigned calls = f.calls;
    assert(hpclang_rpl_execution_plan_payload(&compact, &store, replacement, resolve_cell, NULL,
                                              &f) < 0);
    assert(f.calls == calls);
    assert(!hpclang_rpl_reference_edits_begin_packed(&compact, packed, 2));
    assert(hpclang_rpl_execution_plan_payload(&compact, &store, replacement, resolve_cell, NULL,
                                              &f) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!compact.count && compact.error);
}

static void subtraction_plans(void)
{
    static const uint32_t cases[][7] = {
        {0x2911, 0x18d0a, 0x2907, 0x3911, 0x18d0a, 0x3907, 0},
        {0x2911, 0x18d0a, 0x2907, 0x3913, 0x18d0c, 0x3907, 0},
        {0x2900, 0x18d0a, 0x29f6, 0x2a00, 0x18d0a, 0x2af6, 0},
        {0x2910, 0x1101, 0x2909, 0x3910, 0x1101, 0x3909, 1},
        {0x2911, 0x18d0a, 0x2907, 0x3911, 0x18d0a, 0x3907, 0},
    };
    for (unsigned n = 0; n < sizeof cases / sizeof cases[0]; ++n) {
        struct plan_fixture f = {0};
        uint32_t adjustment = n == 4 ? 5 : 0;
        put(&f, 32, cases[n][0] - adjustment);
        put(&f, 64, cases[n][1]);
        f.memory[0] = 11;
        f.memory[1] = 6;
        f.memory[2] = 10;
        struct hpclang_rpl_code_execution state = {0};
        state.values.registers[0] = cases[n][0];
        state.values.registers[2] = cases[n][1];
        state.values.decimal = cases[n][6];
        struct hpclang_rpl_execution_subtraction expression = {.left.value = cases[n][0],
                                                               .right.value = cases[n][1],
                                                               .nibbles = 2,
                                                               .decimal = cases[n][6],
                                                               .value = cases[n][2]};
        expression.left.adjustment = state.origins.adjustment[0] = adjustment;
        for (unsigned i = 0; i < 5; ++i) {
            expression.left.origins[i] = state.origins.registers[0][i] = 33 + i;
            expression.right.origins[i] = state.origins.registers[2][i] = 65 + i;
        }
        const struct hpclang_rpl_reader reader = {read_cell, &f};
        struct hpclang_rpl_code_step_uses uses;
        assert(!hpclang_rpl_code_execution_step(&reader, &reader, 128, &state, source_token, NULL,
                                                &uses));
        assert(state.values.registers[0] == expression.value);
        expression.carry = state.values.carry;
        struct hpclang_rpl_reference_edit edits[10];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 10));
        assert(!hpclang_rpl_execution_plan_subtraction(&builder, &expression, cases[n][3],
                                                       cases[n][4], cases[n][5], resolve_cell, &f));
        assert(builder.count == 7 && f.calls == 10);
        for (unsigned i = 0; i < builder.count; ++i) {
            assert(edits[i].width == 1 && f.memory[edits[i].field] == edits[i].expected);
            f.memory[edits[i].field] = edits[i].value;
        }
        state.control.pc = 0;
        for (unsigned operand = 0; operand < 2; ++operand) {
            uint32_t value = 0;
            for (unsigned i = 0; i < 5; ++i)
                value |= (uint32_t)f.memory[(operand ? 64 : 32) + i] << (4 * i);
            state.values.registers[operand ? 2 : 0] = value + (operand ? 0 : adjustment);
        }
        assert(!hpclang_rpl_code_execution_step(&reader, &reader, 128, &state, source_token, NULL,
                                                &uses));
        assert(state.values.registers[0] == cases[n][5] && state.values.carry == expression.carry);
        put(&f, 32, cases[n][0] - adjustment);
        put(&f, 64, cases[n][1]);
        for (unsigned i = 0; i < 7; ++i) {
            unsigned at = (i < 5 ? 32 : 64) + i % 5;
            f.memory[at] ^= 1;
            assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 10));
            assert(hpclang_rpl_execution_plan_subtraction(&builder, &expression, cases[n][3],
                                                          cases[n][4], cases[n][5], resolve_cell,
                                                          &f) == HPCLANG_RPL_SPAN_MALFORMED);
            assert(!builder.count && builder.error);
            f.memory[at] ^= 1;
        }
        for (unsigned mode = 0; mode < 8; ++mode) {
            struct hpclang_rpl_execution_subtraction bad = expression;
            if (mode == 0)
                bad.value ^= 1;
            if (mode == 1)
                bad.carry ^= 1;
            if (mode == 2)
                bad.left.origins[0] = 0;
            if (mode == 3)
                bad.right.origins[1] = 0;
            if (mode == 4)
                bad.nibbles = 6;
            if (mode == 5)
                bad.decimal = 2;
            if (mode == 6)
                bad.right.adjustment = 1;
            if (mode == 7)
                bad.first = 5;
            assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 10));
            assert(hpclang_rpl_execution_plan_subtraction(&builder, &bad, cases[n][3], cases[n][4],
                                                          cases[n][5], resolve_cell, &f) < 0);
            assert(!builder.count && builder.error);
        }
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 6));
        assert(hpclang_rpl_execution_plan_subtraction(&builder, &expression, cases[n][3],
                                                      cases[n][4], cases[n][5], resolve_cell,
                                                      &f) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!builder.count);
        for (unsigned i = 2; i < 5; ++i) {
            expression.right.origins[i] = 0;
            f.memory[64 + i] ^= 1;
        }
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 7));
        assert(!hpclang_rpl_execution_plan_subtraction(&builder, &expression, cases[n][3],
                                                       cases[n][4], cases[n][5], resolve_cell, &f));
        assert(builder.count == 7); /* Unused right digits are not dependencies. */
    }
    struct plan_fixture f = {0};
    put(&f, 32, 0x2911);
    struct hpclang_rpl_execution_subtraction alias = {
        .left.value = 0x2911, .right.value = 0x2911, .nibbles = 2, .value = 0x2900};
    for (unsigned i = 0; i < 5; ++i)
        alias.left.origins[i] = alias.right.origins[i] = 33 + i;
    struct hpclang_rpl_reference_edit edits[10];
    struct hpclang_rpl_reference_edit_builder builder;
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 10));
    assert(hpclang_rpl_execution_plan_subtraction(&builder, &alias, 0x3912, 0x2911, 0x3901,
                                                  resolve_cell, &f) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!builder.count);
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 10));
    assert(!hpclang_rpl_execution_plan_subtraction(&builder, &alias, 0x3911, 0x2911, 0x3900,
                                                   resolve_cell, &f));
    assert(builder.count == 5);
    assert(hpclang_rpl_execution_plan_subtraction(&builder, &alias, 0x3910, 0x2911, 0x39ff,
                                                  resolve_cell,
                                                  &f) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!builder.count); /* A borrow change must not change a later branch. */
}

struct payload_fixture
{
    struct plan_fixture physical;
    unsigned derived_calls;
    int derived_error;
};
static int payload_resolve(void *context, uint32_t token, struct hpclang_rpl_code_origin_cell *cell)
{
    struct payload_fixture *f = context;
    if (token >= 1000)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    return resolve_cell(&f->physical, token, cell);
}
static int payload_derived(void *context, struct hpclang_rpl_reference_edit_builder *builder,
                           uint32_t token, uint8_t expected, uint8_t replacement)
{
    struct payload_fixture *f = context;
    ++f->derived_calls;
    if (f->derived_error)
        return f->derived_error;
    if (token < 1000 || token >= 1005)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    /* A copied digit from (0xffffe + 5) mod 20 bits; retain the whole producer. */
    if (expected != ((3u >> (4 * (token - 1000))) & 15u))
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (replacement != expected)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const struct hpclang_rpl_code_store producer = {
        .nibbles = 5, .value = 3, .origins = {1, 2, 3, 4, 5}, .adjustment = 5};
    return hpclang_rpl_execution_plan_payload(builder, &producer, 3, payload_resolve, NULL, f);
}
static void payload_plans(void)
{
    struct payload_fixture f = {0};
    put(&f.physical, 0, 0xffffe);
    put(&f.physical, 10, 0x12345);
    struct hpclang_rpl_reference_edit edits[16];
    struct hpclang_rpl_reference_edit_builder builder;
    struct hpclang_rpl_code_store store = {
        .first = 0, .nibbles = 1, .value = 3, .origins = {1, 2, 3, 4, 5}, .adjustment = 5};
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 16));
    assert(!hpclang_rpl_execution_plan_payload(&builder, &store, 4, payload_resolve, NULL, &f));
    assert(builder.count == 5 && edits[0].expected == 14 && edits[0].value == 15);
    /* One selected adjusted digit still protects all five carry dependencies. */
    for (unsigned i = 1; i < 5; ++i)
        assert(edits[i].expected == 15 && edits[i].value == 15);
    store = (struct hpclang_rpl_code_store){.nibbles = 10, .value = UINT64_C(0x0000312345)};
    for (unsigned i = 0; i < 5; ++i) {
        store.origins[i] = 11 + i;
        store.origins[5 + i] = 1000 + i;
    }
    const struct hpclang_rpl_code_store original = store;
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 16));
    assert(!hpclang_rpl_execution_plan_payload(&builder, &store, store.value, payload_resolve,
                                               payload_derived, &f));
    assert(builder.count == 10 && f.derived_calls == 5 && !memcmp(&store, &original, sizeof store));
    /* The copied computed digits never become fake physical fields. */
    for (unsigned i = 0; i < builder.count; ++i)
        assert(edits[i].field < 15 && edits[i].width == 1);
    for (unsigned mode = 0; mode < 9; ++mode) {
        store = original;
        f.physical.calls = f.physical.fail = 0;
        f.derived_calls = 0;
        f.derived_error = 0;
        uint64_t next = store.value;
        if (mode == 0)
            store.origins[0] = 0;
        if (mode == 1)
            next ^= UINT64_C(1) << 20;
        if (mode == 2)
            f.derived_error = -73;
        if (mode == 3)
            f.derived_error = 73;
        if (mode == 4) {
            f.physical.fail = 1;
            f.physical.error = -74;
        }
        if (mode == 5) {
            f.physical.fail = 1;
            f.physical.error = 74;
        }
        if (mode == 6)
            store.origins[5] = 1999;
        if (mode == 7)
            store.value ^= 1;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 8 ? 9 : 16));
        int result = hpclang_rpl_execution_plan_payload(&builder, &store, next, payload_resolve,
                                                        payload_derived, &f);
        assert(result < 0 && builder.count == 0 && builder.error == result);
        if (mode == 0 || mode == 4 || mode == 5)
            assert(!f.derived_calls);
        unsigned calls = f.derived_calls;
        assert(hpclang_rpl_execution_plan_payload(&builder, &original, original.value,
                                                  payload_resolve, payload_derived, &f) == result);
        assert(calls == f.derived_calls);
    }
    f.physical.fail = 0;
    f.derived_error = 0;
    for (unsigned mode = 0; mode < 4; ++mode) {
        store = original;
        if (mode == 0)
            store.nibbles = 0;
        if (mode == 1)
            store.nibbles = 17;
        if (mode == 2)
            store.first = 16;
        if (mode == 3)
            store.adjustment = 0x100000;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 16));
        assert(hpclang_rpl_execution_plan_payload(&builder, &store, store.value, payload_resolve,
                                                  payload_derived, &f) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!builder.count);
    }
    const struct hpclang_rpl_code_store producer = {
        .nibbles = 5, .value = 3, .origins = {1, 2, 3, 4, 5}, .adjustment = 5};
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 16));
    assert(!hpclang_rpl_execution_plan_payload(&builder, &producer, 4, payload_resolve, NULL, &f));
    assert(hpclang_rpl_execution_plan_payload(&builder, &original, original.value, payload_resolve,
                                              payload_derived, &f) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!builder.count);
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 16));
    assert(hpclang_rpl_execution_plan_payload(&builder, &original, original.value, payload_resolve,
                                              NULL, &f) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!builder.count);
    store = original;
    store.first = 5;
    store.nibbles = 5;
    store.adjustment = 5;
    memset(store.origins, 0, 5 * sizeof(uint32_t));
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 16));
    assert(!hpclang_rpl_execution_plan_payload(&builder, &store, store.value, payload_resolve,
                                               payload_derived, &f));
    assert(builder.count == 5); /* An upper-only store does not consume low-field adjustment. */
}

static void predicate_plans(void)
{
    /* Full-word unsigned relations must preserve both truth and high-digit sources. */
    for (unsigned relation = 0; relation <= HPCLANG_RPL_CODE_LESS_EQUAL; ++relation) {
        struct plan_fixture f = {0};
        struct hpclang_rpl_code_execution state = {0};
        const uint64_t left = UINT64_C(0x8000000000000000), right = UINT64_C(0x7fffffffffffffff);
        state.values.registers[0] = left;
        state.values.registers[2] = right;
        for (unsigned i = 0; i < 16; ++i) {
            f.memory[32 + i] = (left >> (4 * i)) & 15;
            f.memory[64 + i] = (right >> (4 * i)) & 15;
            state.origins.registers[0][i] = 33 + i;
            state.origins.registers[2][i] = 65 + i;
        }
        const unsigned truths[] = {0, 1, 1, 0, 1, 0};
        struct hpclang_rpl_code_predicate predicate = {.left_value = left,
                                                       .right_value = right,
                                                       .nibbles = 16,
                                                       .left = 0,
                                                       .right = 2,
                                                       .relation = relation,
                                                       .taken = truths[relation]};
        struct hpclang_rpl_reference_edit edits[32];
        struct hpclang_rpl_reference_edit_builder builder;
        uint64_t replacements[2] = {left + 1, right - 1};
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 32));
        assert(!hpclang_rpl_execution_plan_predicate(&builder, &predicate, &state, replacements,
                                                     resolve_cell, NULL, &f));
        assert(builder.count == 32 && edits[15].expected == 8 && edits[31].expected == 7);
        replacements[0] = replacements[1];
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 32));
        if (relation != HPCLANG_RPL_CODE_GREATER_EQUAL) {
            int result = hpclang_rpl_execution_plan_predicate(&builder, &predicate, &state,
                                                              replacements, resolve_cell, NULL, &f);
            /* Equality changes all these predicates except strictly-less. */
            if (relation == HPCLANG_RPL_CODE_LESS)
                assert(!result);
            else
                assert(result < 0 && !builder.count);
        }
    }

    for (unsigned mode = 0; mode < 8; ++mode) {
        struct plan_fixture f = {0};
        put(&f, 32, 0x10);
        put(&f, 64, 0x20);
        struct hpclang_rpl_code_execution state = {0};
        state.values.registers[0] = 0x10;
        state.values.registers[2] = 0x20;
        for (unsigned i = 0; i < 5; ++i) {
            state.origins.registers[0][i] = 33 + i;
            state.origins.registers[2][i] = 65 + i;
        }
        struct hpclang_rpl_code_predicate predicate = {.left_value = 0x10,
                                                       .right_value = 0x20,
                                                       .nibbles = 5,
                                                       .left = 0,
                                                       .right = 2,
                                                       .relation = HPCLANG_RPL_CODE_LESS,
                                                       .taken = 1};
        uint64_t replacements[2] = {0x30, 0x40};
        if (mode == 1)
            replacements[0] = 0x50;
        if (mode == 2)
            state.origins.registers[2][4] = 0;
        if (mode == 3)
            predicate.left_value ^= 1;
        if (mode == 4)
            predicate.taken = 0;
        if (mode == 5) {
            f.fail = 6;
            f.error = -72;
        }
        if (mode == 7) {
            predicate.right = HPCLANG_RPL_CODE_CONSTANT;
            predicate.right_value = 0;
            predicate.relation = HPCLANG_RPL_CODE_GREATER;
            replacements[1] = 0;
        }
        struct hpclang_rpl_reference_edit edits[10];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 6 ? 5 : 10));
        int result = hpclang_rpl_execution_plan_predicate(&builder, &predicate, &state,
                                                          replacements, resolve_cell, NULL, &f);
        if (mode && mode != 7) {
            assert(result < 0 && builder.error == result && !builder.count);
            unsigned calls = f.calls;
            assert(hpclang_rpl_execution_plan_predicate(&builder, &predicate, &state, replacements,
                                                        resolve_cell, NULL, &f) == result &&
                   calls == f.calls);
        } else {
            assert(!result && builder.count == (mode == 7 ? 5 : 10));
            assert(edits[1].expected == 1 && edits[1].value == 3);
        }
    }
}

static void constant_plans(void)
{
    for (unsigned mode = 0; mode < 8; ++mode) {
        struct plan_fixture f = {0};
        f.memory[16] = 10;
        f.memory[17] = 8;
        f.memory[18] = 2;
        f.memory[99] = 7;
        const struct hpclang_rpl_reader reader = {read_cell, &f};
        struct hpclang_rpl_code_constant fact = {.p = 7, .p_origin = 100};
        assert(!hpclang_rpl_code_assignment_inspect(&reader, 16, 3, 7, &fact.assignment));
        if (mode == 1)
            fact.p_origin = 0;
        if (mode == 2)
            fact.assignment.destination_first = 6;
        if (mode == 3)
            f.memory[99] = 6;
        if (mode == 4)
            f.memory[18] = 1;
        if (mode == 7) {
            f.memory[16] = 13;
            f.memory[17] = 2;
            fact.p_origin = 0;
            assert(!hpclang_rpl_code_assignment_inspect(&reader, 16, 2, 7, &fact.assignment));
        }
        struct hpclang_rpl_reference_edit edits[4];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 5 ? 2 : 4));
        int result = hpclang_rpl_execution_plan_constant(&builder, &fact, 0, 0, mode == 6, &reader,
                                                         source_token, resolve_cell, NULL, &f);
        if (mode && mode != 7)
            assert(result < 0 && !builder.count && builder.error == result);
        else
            assert(!result && builder.count == (mode == 7 ? 2 : 4));
    }
}

static void bitshift_plans(void)
{
    const unsigned fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 15, 16};
    for (unsigned f = 0; f < 10; ++f)
        for (unsigned reg = 0; reg < 4; ++reg)
            for (unsigned mode = 0; mode < 18; ++mode) {
                struct plan_fixture fixture = {.memory = {8, 1, 9, fields[f], reg}};
                if (f == 9)
                    fixture.memory[2] = 12 + reg;
                const struct hpclang_rpl_reader reader = {read_cell, &fixture};
                struct hpclang_rpl_code_values values = {0};
                values.registers[reg] = UINT64_C(0x123456789abcdef0);
                values.registers[HPCLANG_RPL_CODE_P] = fixture.memory[96] = 3;
                struct hpclang_rpl_code_bitshift_source fact = {.p_origin = 97};
                assert(!hpclang_rpl_code_bitshift_inspect(&reader, 0, 5, &values, &fact.shift));
                for (unsigned i = 0; i < 16; ++i) {
                    fact.origins[i] = 33 + i;
                    fixture.memory[32 + i] = (fact.shift.before >> (4 * i)) & 15;
                }
                unsigned first = fact.shift.first, count = fact.shift.nibbles;
                uint64_t input = fact.shift.before ^ (UINT64_C(10) << (4 * first));
                uint64_t mask = count == 16 ? UINT64_MAX : (UINT64_C(1) << (4 * count)) - 1;
                uint64_t output = (input & ~(mask << (4 * first))) |
                                  ((((input >> (4 * first)) & mask) >> 1) << (4 * first));
                if (mode == 1)
                    fact.origins[first] = 0;
                if (mode == 2)
                    input ^= UINT64_C(1) << (4 * first); /* identical result, different SB */
                if (mode == 3)
                    output ^= UINT64_C(1) << (4 * first);
                if (mode == 4)
                    fact.shift.after ^= 1;
                if (mode == 5)
                    fact.shift.lost_bit ^= 1;
                if (mode == 6)
                    fact.shift.first ^= 1;
                if (mode == 7)
                    fact.shift.nibbles++;
                if (mode == 8)
                    fact.shift.reg = 4;
                if (mode == 9)
                    fact.shift.length++;
                if (mode == 10)
                    fixture.memory[32 + first] ^= 1;
                if (mode == 11)
                    fact.p_origin = 0;
                if (mode == 12)
                    fact.p_adjustment = 1;
                if (mode == 13) {
                    fixture.fail = 2;
                    fixture.error = -77;
                }
                if (mode == 14) {
                    fixture.fail = 2;
                    fixture.error = 1;
                }
                if (mode == 16)
                    fact.adjustment = 0x100000;
                uint8_t saved[128];
                memcpy(saved, fixture.memory, sizeof saved);
                struct hpclang_rpl_reference_edit edits[24];
                struct hpclang_rpl_reference_edit_builder builder;
                assert(!(mode == 17 ? hpclang_rpl_reference_edits_begin_packed(&builder, edits, 24)
                                    : hpclang_rpl_reference_edits_begin(&builder, edits,
                                                                        mode == 15 ? 2 : 24)));
                int result =
                    hpclang_rpl_execution_plan_bitshift(&builder, &fact, input, output, &reader,
                                                        source_token, resolve_cell, NULL, &fixture);
                assert(!memcmp(saved, fixture.memory, sizeof saved));
                if (!mode || mode == 17 || (mode == 11 && f >= 2)) {
                    assert(!result);
                    unsigned cells = 0;
                    for (unsigned n = 0; n < builder.count; ++n)
                        for (unsigned d = 0; d < edits[n].width; ++d) {
                            unsigned at = edits[n].field + d;
                            assert(at < 128 && edits[n].mapping == 1 &&
                                   fixture.memory[at] == ((edits[n].expected >> (4 * d)) & 15));
                            fixture.memory[at] = (edits[n].value >> (4 * d)) & 15;
                            ++cells;
                        }
                    assert(cells == fact.shift.length + count + (f < 2));
                    uint64_t actual = 0;
                    for (unsigned d = 0; d < 16; ++d)
                        actual |= (uint64_t)fixture.memory[32 + d] << (4 * d);
                    assert(actual == input);
                    values.registers[reg] = actual;
                    struct hpclang_rpl_code_bitshift after;
                    assert(!hpclang_rpl_code_bitshift_inspect(&reader, 0, 5, &values, &after));
                    assert(after.after == output && after.lost_bit == fact.shift.lost_bit);
                } else {
                    assert(result < 0 && !builder.count && builder.error == result);
                    unsigned calls = fixture.calls;
                    assert(hpclang_rpl_execution_plan_bitshift(&builder, &fact, input, output,
                                                               &reader, source_token, resolve_cell,
                                                               NULL, &fixture) == result);
                    assert(fixture.calls == calls);
                }
            }
}

static void logical_plans(void)
{
    const unsigned pairs[8][2] = {{0, 1}, {1, 2}, {2, 0}, {3, 2}, {1, 0}, {2, 1}, {0, 2}, {2, 3}};
    for (unsigned field = 0; field < 16; ++field) {
        if (field > 7 && field != 15)
            continue;
        for (unsigned op = 0; op < 16; ++op)
            for (unsigned packed = 0; packed < 2; ++packed) {
                struct plan_fixture f = {.memory = {0, 14, field, op}};
                const struct hpclang_rpl_reader reader = {read_cell, &f};
                struct hpclang_rpl_code_values values = {0};
                values.registers[pairs[op & 7][0]] = UINT64_C(0x123456789abcdef0);
                values.registers[pairs[op & 7][1]] = UINT64_C(0xfedcba9876543210);
                values.registers[HPCLANG_RPL_CODE_P] = f.memory[96] = 3;
                struct hpclang_rpl_code_logical_source fact = {.p_origin = 97};
                assert(!hpclang_rpl_code_logical_inspect(&reader, 0, 4, &values, &fact.logical));
                uint64_t inputs[2] = {fact.logical.left_value, fact.logical.right_value};
                uint64_t output = inputs[0];
                for (unsigned i = 0; i < 16; ++i) {
                    for (unsigned side = 0; side < 2; ++side) {
                        unsigned at = 32 + 16 * side + i;
                        fact.origins[side][i] = at + 1;
                        f.memory[at] = (inputs[side] >> (4 * i)) & 15;
                    }
                    if (i < fact.logical.first || i >= fact.logical.first + fact.logical.nibbles)
                        continue;
                    for (unsigned side = 0; side < 2; ++side)
                        inputs[side] ^= (uint64_t)(side ? 5 : 10) << (4 * i);
                    unsigned a = (inputs[0] >> (4 * i)) & 15;
                    unsigned b = (inputs[1] >> (4 * i)) & 15;
                    output = (output & ~(UINT64_C(15) << (4 * i))) |
                             ((uint64_t)(op < 8 ? a & b : a | b) << (4 * i));
                }
                uint8_t saved[128];
                memcpy(saved, f.memory, sizeof saved);
                struct hpclang_rpl_reference_edit edits[40];
                struct hpclang_rpl_reference_edit_builder builder;
                assert(!(packed ? hpclang_rpl_reference_edits_begin_packed(&builder, edits, 40)
                                : hpclang_rpl_reference_edits_begin(&builder, edits, 40)));
                assert(!hpclang_rpl_execution_plan_logical(&builder, &fact, inputs, output, &reader,
                                                           source_token, resolve_cell, NULL, &f));
                assert(!memcmp(saved, f.memory, sizeof saved));
                unsigned cells = 0;
                for (unsigned n = 0; n < builder.count; ++n) {
                    assert(edits[n].mapping == 1);
                    for (unsigned digit = 0; digit < edits[n].width; ++digit) {
                        unsigned at = edits[n].field + digit;
                        assert(at < 128 && saved[at] == ((edits[n].expected >> (4 * digit)) & 15));
                        f.memory[at] = (edits[n].value >> (4 * digit)) & 15;
                        ++cells;
                    }
                }
                assert(cells == 4 + 2 * fact.logical.nibbles + (field < 2));
                for (unsigned side = 0; side < 2; ++side) {
                    uint64_t actual = 0;
                    for (unsigned digit = 0; digit < 16; ++digit)
                        actual |= (uint64_t)f.memory[32 + 16 * side + digit] << (4 * digit);
                    assert(actual == inputs[side]);
                    values.registers[pairs[op & 7][side]] = actual;
                }
                struct hpclang_rpl_code_logical after;
                assert(!hpclang_rpl_code_logical_inspect(&reader, 0, 4, &values, &after));
                assert(after.result == output);
            }
    }
    for (unsigned mode = 0; mode < 22; ++mode) {
        struct plan_fixture f = {.memory = {0, 14, 0, 2}};
        const struct hpclang_rpl_reader reader = {read_cell, &f};
        struct hpclang_rpl_code_values values = {0};
        values.registers[HPCLANG_RPL_CODE_C] = 0; /* AND masks every right input bit. */
        values.registers[HPCLANG_RPL_CODE_A] = 0xfffff;
        put(&f, 48, 0xfffff);
        struct hpclang_rpl_code_logical_source fact = {.p_origin = 97};
        assert(!hpclang_rpl_code_logical_inspect(&reader, 0, 4, &values, &fact.logical));
        for (unsigned i = 0; i < 16; ++i) {
            fact.origins[0][i] = 33 + i;
            fact.origins[1][i] = 49 + i;
        }
        uint64_t inputs[2] = {1, 0xffffe}, output = 0;
        if (mode == 1)
            fact.origins[0][0] = 0;
        if (mode == 2)
            fact.origins[1][0] = 0;
        if (mode == 3)
            fact.p_origin = 0;
        if (mode == 4)
            output = 1;
        if (mode == 5)
            fact.logical.result = 1;
        if (mode == 6)
            fact.logical.disjunction = 1;
        if (mode == 7)
            fact.logical.first = 1;
        if (mode == 8)
            fact.logical.nibbles = 2;
        if (mode == 9)
            fact.logical.length = 3;
        if (mode == 10)
            fact.logical.right = 2;
        if (mode == 11)
            f.memory[48] = 14;
        if (mode == 12)
            fact.p_adjustment = 1;
        if (mode == 13)
            fact.logical.p = 16;
        if (mode == 14)
            fact.logical.instruction = 0x100000;
        if (mode == 15)
            f.memory[3] = 10;
        if (mode == 16)
            fact.origins[1][0] = 33; /* physical alias with contradictory value */
        if (mode == 17 || mode == 18) {
            f.fail = 6;
            f.error = mode == 17 ? -77 : 1;
        }
        if (mode == 19) {
            fact.adjustment[1] = 1;
            put(&f, 48, 0xffffe);
        }
        if (mode == 21)
            fact.adjustment[1] = 0x100000;
        uint8_t saved[128];
        memcpy(saved, f.memory, sizeof saved);
        struct hpclang_rpl_reference_edit edits[20];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 20 ? 5 : 20));
        int result = hpclang_rpl_execution_plan_logical(&builder, &fact, inputs, output, &reader,
                                                        source_token, resolve_cell, NULL, &f);
        assert(!memcmp(saved, f.memory, sizeof saved));
        if (!mode || mode == 19) {
            assert(!result && builder.count == (mode == 19 ? 11 : 7));
        } else {
            assert(result < 0 && !builder.count && builder.error == result);
            unsigned calls = f.calls;
            assert(hpclang_rpl_execution_plan_logical(&builder, &fact, inputs, output, &reader,
                                                      source_token, resolve_cell, NULL,
                                                      &f) == result);
            assert(f.calls == calls);
        }
    }
}

static void sum_plans(void)
{
    for (unsigned mode = 0; mode < 11; ++mode) {
        struct plan_fixture f = {0};
        f.memory[0] = 10;
        f.memory[1] = 0;
        f.memory[2] = mode == 9 ? 6 : 2;
        put(&f, 32, 1);
        put(&f, 48, 2);
        const struct hpclang_rpl_reader reader = {read_cell, &f};
        struct hpclang_rpl_code_values values = {0};
        values.registers[2] = 1;
        values.registers[0] = 2;
        struct hpclang_rpl_code_sum_source fact = {0};
        assert(!hpclang_rpl_code_sum_inspect(&reader, 0, 3, &values, &fact.sum));
        fact.p_origin = 100;
        for (unsigned i = 0; i < 16; ++i) {
            fact.origins[0][i] = 33 + i;
            fact.origins[1][i] = (mode == 9 ? 33 : 49) + i;
        }
        uint64_t inputs[2] = {2, 3}, output = 5;
        if (mode == 1)
            fact.origins[0][0] = 0;
        if (mode == 2)
            fact.origins[1][0] = 0;
        if (mode == 3)
            fact.p_origin = 0;
        if (mode == 4)
            output = 6;
        if (mode == 5) {
            inputs[0] = 15;
            output = 2;
        }
        if (mode == 6)
            fact.sum.result ^= 1;
        if (mode == 7) {
            fact.adjustment[0] = 1;
            put(&f, 32, 0);
        }
        if (mode == 10) {
            fact.sum.decimal = 1;
            inputs[1] = 15;
        }
        struct hpclang_rpl_reference_edit edits[16];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 8 ? 5 : 16));
        int result = hpclang_rpl_execution_plan_sum(&builder, &fact, inputs, output, &reader,
                                                    source_token, resolve_cell, NULL, &f);
        if (mode && mode != 7)
            assert(result < 0 && !builder.count && builder.error == result);
        else
            assert(!result && builder.count == (mode == 7 ? 10 : 6));
    }
}

static void p_unit_plans(void)
{
    for (unsigned operation = 12; operation <= 13; ++operation)
        for (unsigned p = 0; p < 16; ++p)
            for (unsigned input = 0; input < 16; ++input) {
                struct plan_fixture f = {0};
                f.memory[1] = operation;
                f.memory[32] = p;
                const struct hpclang_rpl_reader reader = {read_cell, &f};
                struct hpclang_rpl_code_values values = {.decimal = 1};
                values.registers[HPCLANG_RPL_CODE_P] = p;
                struct hpclang_rpl_code_unit fact;
                assert(!hpclang_rpl_code_unit_inspect(&reader, 0, 2, &values, &fact));
                fact.origins[0] = 33;
                unsigned output = (input + (operation == 12 ? 1 : 15)) & 15;
                struct hpclang_rpl_reference_edit edits[3];
                struct hpclang_rpl_reference_edit_builder builder;
                assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 3));
                int result = hpclang_rpl_execution_plan_unit(
                    &builder, &fact, input, output, &reader, source_token, resolve_cell, NULL, &f);
                unsigned boundary = operation == 12 ? 15 : 0;
                if ((p == boundary) != (input == boundary)) {
                    assert(result < 0 && !builder.count && builder.error == result);
                } else {
                    assert(!result && builder.count == 3);
                    for (unsigned i = 0; i < builder.count; ++i) {
                        assert(f.memory[edits[i].field] == edits[i].expected);
                        f.memory[edits[i].field] = edits[i].value;
                    }
                    values.registers[HPCLANG_RPL_CODE_P] = f.memory[32];
                    struct hpclang_rpl_code_unit next;
                    assert(!hpclang_rpl_code_unit_inspect(&reader, 0, 2, &values, &next));
                    assert(next.after == output && next.carry == fact.carry);
                }
            }
    for (unsigned mode = 0; mode < 6; ++mode) {
        struct plan_fixture f = {0};
        f.memory[1] = 13;
        f.memory[32] = 2;
        const struct hpclang_rpl_reader reader = {read_cell, &f};
        struct hpclang_rpl_code_values values = {0};
        values.registers[HPCLANG_RPL_CODE_P] = 2;
        struct hpclang_rpl_code_unit fact;
        assert(!hpclang_rpl_code_unit_inspect(&reader, 0, 2, &values, &fact));
        fact.origins[0] = 33;
        if (mode == 0)
            fact.origins[0] = 0;
        if (mode == 1)
            fact.p ^= 1;
        if (mode == 2)
            fact.adjustment = 1;
        if (mode == 3)
            f.memory[32] ^= 1;
        struct hpclang_rpl_reference_edit edits[3];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 4 ? 2 : 3));
        int result = hpclang_rpl_execution_plan_unit(&builder, &fact, mode == 5 ? 16 : 2, 1,
                                                     &reader, source_token, resolve_cell, NULL, &f);
        assert(result < 0 && !builder.count && builder.error == result);
    }
}

static void selected_unit_plans(void)
{
    for (unsigned mode = 0; mode < 9; ++mode) {
        struct plan_fixture f = {0};
        f.memory[0] = 11;
        f.memory[1] = 0;
        f.memory[2] = 6;
        put(&f, 32, 1);
        const struct hpclang_rpl_reader reader = {read_cell, &f};
        struct hpclang_rpl_code_values values = {0};
        values.registers[2] = 1;
        struct hpclang_rpl_code_unit fact;
        assert(!hpclang_rpl_code_unit_inspect(&reader, 0, 3, &values, &fact));
        fact.p_origin = 100;
        for (unsigned i = 0; i < 5; ++i)
            fact.origins[i] = 33 + i;
        uint64_t input = 2, output = 3;
        if (mode == 1)
            output = 4;
        if (mode == 2) {
            input = 15;
            output = 0;
        }
        if (mode == 3)
            fact.origins[0] = 0;
        if (mode == 4)
            fact.p_origin = 0;
        if (mode == 5)
            fact.after = 3;
        if (mode == 6) {
            fact.decimal = 1;
            input = 10;
            output = 11;
        }
        if (mode == 8) {
            fact.adjustment = 1;
            put(&f, 32, 0);
        }
        struct hpclang_rpl_reference_edit edits[10];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 7 ? 4 : 10));
        int result = hpclang_rpl_execution_plan_unit(&builder, &fact, input, output, &reader,
                                                     source_token, resolve_cell, NULL, &f);
        if (mode && mode != 8)
            assert(result < 0 && !builder.count && builder.error == result);
        else
            assert(!result && builder.count == (mode == 8 ? 9 : 5));
    }
}

static void immediate_unit_plans(void)
{
    for (unsigned mode = 0; mode < 7; ++mode) {
        struct plan_fixture f = {0};
        const uint8_t code[] = {8, 1, 8, 15, 9, 4};
        memcpy(f.memory, code, sizeof code);
        put(&f, 32, 10);
        const struct hpclang_rpl_reader reader = {read_cell, &f};
        struct hpclang_rpl_code_values values = {.decimal = 1};
        values.registers[HPCLANG_RPL_CODE_B] = 10;
        struct hpclang_rpl_code_unit fact;
        assert(!hpclang_rpl_code_unit_inspect(&reader, 0, 6, &values, &fact));
        for (unsigned i = 0; i < 5; ++i)
            fact.origins[i] = 33 + i;
        uint64_t input = 11, output = 6;
        if (mode == 1)
            ++output;
        if (mode == 2)
            fact.origins[0] = 0;
        if (mode == 3)
            ++f.memory[5];
        if (mode == 5)
            fact.carry = 1;
        if (mode == 6) {
            input = 4;
            output = 0xfffff;
        }
        struct hpclang_rpl_reference_edit edits[16];
        struct hpclang_rpl_reference_edit_builder builder;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, mode == 4 ? 10 : 16));
        int result = hpclang_rpl_execution_plan_unit(&builder, &fact, input, output, &reader,
                                                     source_token, resolve_cell, NULL, &f);
        if (mode)
            assert(result < 0 && !builder.count && builder.error == result);
        else
            assert(!result && builder.count == 11);
    }
}

static void native_word_plans(void)
{
    const uint32_t values[] = {0, 0xfffff, 0x100000, UINT32_MAX};
    for (unsigned operation = 0; operation < 3; ++operation)
        for (unsigned pattern = 0; pattern < 4; ++pattern)
            for (unsigned adjusted = 0; adjusted < 2; ++adjusted) {
                struct plan_fixture f = {0};
                struct hpclang_rpl_code_native_word word = {
                    .reg = operation == 2 ? HPCLANG_RPL_CODE_D : HPCLANG_RPL_CODE_B,
                    .before = values[pattern],
                    .adjustment = adjusted ? 0xffffb : 0,
                    .delta = operation == 2 ? 1
                             : operation    ? 5
                                            : -5};
                word.after = word.before + (uint32_t)word.delta;
                uint32_t opcode = operation == 1 ? 0xeb18 : 0xdb18;
                for (unsigned i = 0; i < 4; ++i)
                    f.memory[i] = (opcode >> (4 * i)) & 15;
                uint32_t source =
                    (word.before & 0xfff00000) | ((word.before - word.adjustment) & 0xfffff);
                for (unsigned i = 0; i < 8; ++i) {
                    word.origins[i] = 17 + i;
                    f.memory[16 + i] = (source >> (4 * i)) & 15;
                }
                struct hpclang_rpl_reference_edit edits[12];
                struct hpclang_rpl_reference_edit_builder builder;
                const struct hpclang_rpl_reader reader = {read_cell, &f};
                uint32_t input = word.before + 16, output = input + (uint32_t)word.delta;
                assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 12));
                assert(!hpclang_rpl_execution_plan_native_word(
                    &builder, &word, input, output, &reader, source_token, resolve_cell, NULL, &f));
                assert(builder.count == 12);
                uint32_t proposed = (input & 0xfff00000) | ((input - word.adjustment) & 0xfffff);
                for (unsigned i = 0; i < 8; ++i) {
                    assert(edits[4 + i].field == 16 + i);
                    assert(edits[4 + i].expected == ((source >> (4 * i)) & 15));
                    assert(edits[4 + i].value == ((proposed >> (4 * i)) & 15));
                }
                for (unsigned fault = 0; fault < 16; ++fault) {
                    struct hpclang_rpl_code_native_word bad = word;
                    unsigned at = fault < 4 ? fault : 16 + fault - 4;
                    if (fault < 12)
                        f.memory[at] ^= 1;
                    if (fault == 12)
                        ++bad.after;
                    if (fault == 13)
                        bad.origins[7] = 0;
                    if (fault == 14)
                        bad.delta = 2;
                    assert(
                        !hpclang_rpl_reference_edits_begin(&builder, edits, fault == 15 ? 11 : 12));
                    assert(hpclang_rpl_execution_plan_native_word(&builder, &bad, input, output,
                                                                  &reader, source_token,
                                                                  resolve_cell, NULL, &f) < 0);
                    assert(!builder.count && builder.error);
                    if (fault < 12)
                        f.memory[at] ^= 1;
                }
                assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 12));
                assert(hpclang_rpl_execution_plan_native_word(&builder, &word, input, output ^ 1,
                                                              &reader, source_token, resolve_cell,
                                                              NULL, &f) < 0 &&
                       !builder.count);
            }
}

void test_execution_plans(void)
{
    native_word_plans();
    sum_plans();
    logical_plans();
    bitshift_plans();
    selected_unit_plans();
    p_unit_plans();
    immediate_unit_plans();
    constant_plans();
    predicate_plans();
    subtraction_plans();
    packed_payload_plans();
    payload_plans();
    struct plan_fixture f = {0};
    const uint8_t program[] = {1, 4, 7, 8, 0, 8, 14}; /* C=DAT1.A; PC=(C). */
    memcpy(f.memory, program, sizeof program);
    put(&f, 32, 64);
    put(&f, 64, 112);
    put(&f, 96, 32);
    struct hpclang_rpl_code_execution state = {0};
    state.values.registers[HPCLANG_RPL_CODE_D1] = 32;
    for (unsigned i = 0; i < 5; ++i)
        state.origins.registers[HPCLANG_RPL_CODE_D1][i] = 97 + i;
    struct hpclang_rpl_reader reader = {read_cell, &f};
    struct hpclang_rpl_code_step_uses uses[2];
    assert(!hpclang_rpl_code_execution_step(&reader, &reader, 128, &state, source_token, NULL,
                                            &uses[0]));
    assert(!hpclang_rpl_code_execution_step(&reader, &reader, 125, &state, source_token, NULL,
                                            &uses[1]));
    assert(uses[0].count == 1 && uses[1].count == 2 && state.control.pc == 112);
    struct hpclang_rpl_reference_edit edits[20];
    struct hpclang_rpl_reference_edit_builder builder;
    const uint32_t first[] = {48}, second[] = {80, 120};
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 20));
    assert(!hpclang_rpl_execution_plan_uses(&builder, &uses[0], first, resolve_cell, &f));
    assert(!hpclang_rpl_execution_plan_uses(&builder, &uses[1], second, resolve_cell, &f));
    assert(builder.count == 15 && f.calls == 15);
    for (unsigned group = 0; group < 3; ++group) {
        const unsigned at[] = {32, 64, 96};
        const uint32_t before[] = {64, 112, 32}, after[] = {80, 120, 48};
        for (unsigned digit = 0; digit < 5; ++digit) {
            const struct hpclang_rpl_reference_edit *edit = &edits[group * 5 + digit];
            assert(edit->mapping == 1 && edit->width == 1 && edit->field == at[group] + digit);
            assert(edit->expected == ((before[group] >> (4 * digit)) & 15));
            assert(edit->value == ((after[group] >> (4 * digit)) & 15));
        }
    }
    assert(!hpclang_rpl_execution_plan_uses(&builder, &uses[1], second, resolve_cell, &f));
    assert(builder.count == 15); /* Exact reuse, no additional capacity. */
    const uint32_t unchanged[] = {64, 112};
    assert(hpclang_rpl_execution_plan_uses(&builder, &uses[1], unchanged, resolve_cell, &f) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(!builder.count && builder.error == HPCLANG_RPL_SPAN_MALFORMED);
    unsigned calls = f.calls;
    assert(hpclang_rpl_execution_plan_uses(&builder, &uses[0], first, resolve_cell, &f) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(f.calls == calls); /* A failed plan cannot resume silently. */
    struct hpclang_rpl_code_step_uses generated = uses[0];
    generated.addresses[0].generated = 1;
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 20));
    assert(hpclang_rpl_execution_plan_uses(&builder, &generated, first, resolve_cell, &f) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!builder.count && f.calls == calls); /* Producer validation is a separate obligation. */

    for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned digit = 0; digit < 10; ++digit) {
            struct hpclang_rpl_code_step_uses bad = uses[1];
            f.calls = 0;
            f.fail = mode < 2 ? digit + 1 : 0;
            f.error = mode ? 1 : -77;
            uint32_t cell = bad.addresses[digit / 5].origins[digit % 5];
            if (mode == 2)
                bad.addresses[digit / 5].origins[digit % 5] = 0;
            if (mode == 3)
                f.memory[cell - 1] ^= 1;
            assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 20));
            int expected = mode == 0   ? -77
                           : mode == 2 ? HPCLANG_RPL_SPAN_UNSUPPORTED
                                       : HPCLANG_RPL_SPAN_MALFORMED;
            assert(hpclang_rpl_execution_plan_uses(&builder, &bad, second, resolve_cell, &f) ==
                   expected);
            assert(!builder.count && builder.error == expected);
            if (mode == 3)
                f.memory[cell - 1] ^= 1;
        }
    f.fail = 0;
    for (unsigned capacity = 0; capacity < 10; ++capacity) {
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, capacity));
        assert(hpclang_rpl_execution_plan_uses(&builder, &uses[1], second, resolve_cell, &f) ==
               HPCLANG_RPL_SPAN_BOUNDS);
        assert(!builder.count && builder.error == HPCLANG_RPL_SPAN_BOUNDS);
    }
    /* Different five-digit windows sharing three source cells agree after
     * normalization; grouped five-digit edits alone would partially overlap. */
    struct hpclang_rpl_code_step_uses windows = {.count = 2};
    uint32_t replacements[2] = {0};
    for (unsigned use = 0; use < 2; ++use) {
        windows.addresses[use].kind = HPCLANG_RPL_CODE_USE_RETURN_TARGET;
        for (unsigned digit = 0; digit < 5; ++digit) {
            unsigned at = 32 + 2 * use + digit;
            windows.addresses[use].origins[digit] = at + 1;
            windows.addresses[use].value |= (uint32_t)f.memory[at] << (4 * digit);
            replacements[use] |= (uint32_t)(at & 15) << (4 * digit);
        }
    }
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 7));
    assert(!hpclang_rpl_execution_plan_uses(&builder, &windows, replacements, resolve_cell, &f));
    assert(builder.count == 7);
    replacements[1] ^= 1;
    assert(hpclang_rpl_execution_plan_uses(&builder, &windows, replacements, resolve_cell, &f) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(!builder.count);
    /* Two computed uses of one original value must agree on its replacement,
     * not merely have superficially compatible computed output digits. */
    struct hpclang_rpl_code_step_uses computed = {.count = 2};
    for (unsigned i = 0; i < 2; ++i) {
        computed.addresses[i] = uses[0].addresses[0];
        computed.addresses[i].value = 32 - 5 * (i + 1);
        computed.addresses[i].adjustment = (0x100000u - 5 * (i + 1)) & 0xfffffu;
        replacements[i] = 48 - 5 * (i + 1);
    }
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 5));
    assert(!hpclang_rpl_execution_plan_uses(&builder, &computed, replacements, resolve_cell, &f));
    assert(builder.count == 5 && edits[1].expected == 2 && edits[1].value == 3);
    ++replacements[1];
    assert(hpclang_rpl_execution_plan_uses(&builder, &computed, replacements, resolve_cell, &f) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    put(&f, 96, 0);
    computed.count = 1;
    computed.addresses[0].value = 0xffffb;
    replacements[0] = 0xffffc;
    assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 5));
    assert(!hpclang_rpl_execution_plan_uses(&builder, &computed, replacements, resolve_cell, &f));
    assert(edits[0].expected == 0 && edits[0].value == 1);
    for (unsigned mode = 0; mode < 3; ++mode) {
        struct hpclang_rpl_code_step_uses bad = computed;
        replacements[0] = mode == 0 ? 0x100000 : 0xffffc;
        if (mode == 1)
            bad.addresses[0].value = 0x100000;
        if (mode == 2)
            bad.addresses[0].adjustment = 0x100000;
        assert(!hpclang_rpl_reference_edits_begin(&builder, edits, 5));
        assert(hpclang_rpl_execution_plan_uses(&builder, &bad, replacements, resolve_cell, &f) ==
               HPCLANG_RPL_SPAN_BOUNDS);
        assert(!builder.count);
    }
}
