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

void test_execution_plans(void)
{
    subtraction_plans();
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
