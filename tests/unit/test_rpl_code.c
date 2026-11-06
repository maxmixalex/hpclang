#include "rpl-code.h"
#include <assert.h>
#include <string.h>

struct image
{
    uint8_t data[128];
    uint32_t address, size, fail;
};

static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    struct image *image = context;
    if (at == image->fail || at < image->address || at - image->address >= image->size)
        return 0;
    *value = image->data[at - image->address];
    return 1;
}

static void encode(struct image *image, unsigned prefix, unsigned width, uint32_t value)
{
    for (unsigned i = 0; i < width; ++i)
        image->data[prefix + i] = (value >> (4 * i)) & 15;
}

static const struct hpclang_rpl_code_patch sentinel = {123, 456, 789, 1011};
static void seed_origins(struct hpclang_rpl_code_origins *state);

static int data_token(void *context, unsigned data, uint32_t address, uint8_t value,
                      uint32_t *token)
{
    (void)value;
    if (context && address == *(uint32_t *)context)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    *token = (data ? 0x200000 : 0x100000) + address;
    return 0;
}

static void data_steps(void)
{
    struct image code = {.size = 128, .fail = UINT32_MAX};
    struct image memory = {.address = 0x180e9, .size = 128, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader instructions = {read_nibble, &code};
    const struct hpclang_rpl_reader reader = {read_nibble, &memory};
    struct hpclang_rpl_code_values values = {0};
    struct hpclang_rpl_code_origins origins = {0};
    uint32_t length = 999;
    encode(&code, 0, 2, 0xf1); /* D1=180e9. */
    encode(&code, 2, 5, memory.address);
    encode(&code, 8, 3, 0x741); /* C=DAT1.A. */
    encode(&memory, 0, 5, 0x54321);
    values.registers[HPCLANG_RPL_CODE_C] = UINT64_C(0xabcdef0000000000);
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 0, 128, &values, &origins,
                                       data_token, NULL, &length));
    assert(length == 7 && values.registers[HPCLANG_RPL_CODE_D1] == memory.address);
    for (unsigned i = 0; i < 5; ++i)
        assert(origins.registers[HPCLANG_RPL_CODE_D1][i] == 0x100002 + i);
    const struct hpclang_rpl_code_values before = values;
    const struct hpclang_rpl_code_origins prior = origins;
    for (unsigned i = 0; i < 5; ++i) {
        memory.fail = memory.address + i;
        assert(hpclang_rpl_code_data_step(&instructions, &reader, 8, 120, &values, &origins,
                                          data_token, NULL,
                                          &length) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&values, &before, sizeof values) &&
               !memcmp(&origins, &prior, sizeof origins));
        assert(length == 7);
        memory.fail = UINT32_MAX;
        uint32_t rejected = memory.address + i;
        assert(hpclang_rpl_code_data_step(&instructions, &reader, 8, 120, &values, &origins,
                                          data_token, &rejected,
                                          &length) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&values, &before, sizeof values) &&
               !memcmp(&origins, &prior, sizeof origins));
    }
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 8, 120, &values, &origins,
                                       data_token, NULL, &length));
    assert(length == 3 && values.registers[HPCLANG_RPL_CODE_C] == UINT64_C(0xabcdef0000054321));
    for (unsigned i = 0; i < 5; ++i)
        assert(origins.registers[HPCLANG_RPL_CODE_C][i] == 0x2180e9 + i);
    encode(&code, 11, 2, 0xad); /* A=C.A. */
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 11, 117, &values, &origins,
                                       data_token, NULL, &length));
    assert(values.registers[HPCLANG_RPL_CODE_A] == 0x54321);
    assert(!memcmp(origins.registers[HPCLANG_RPL_CODE_A], origins.registers[HPCLANG_RPL_CODE_C],
                   5 * sizeof(uint32_t)));
    encode(&code, 13, 2, 0xed); /* ACEX.A. */
    values.registers[HPCLANG_RPL_CODE_A] = 0x11111;
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 13, 115, &values, &origins,
                                       data_token, NULL, &length));
    assert(values.registers[HPCLANG_RPL_CODE_A] == 0x54321);
    assert(values.registers[HPCLANG_RPL_CODE_C] == UINT64_C(0xabcdef0000011111));
    encode(&code, 15, 3, 0x071); /* D1=D1+1, including overflow carry. */
    values.registers[HPCLANG_RPL_CODE_D1] = 0xfffff;
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 15, 113, &values, &origins,
                                       data_token, NULL, &length));
    assert(!values.registers[HPCLANG_RPL_CODE_D1] && values.carry == 1);
    for (unsigned i = 0; i < 5; ++i)
        assert(!origins.registers[HPCLANG_RPL_CODE_D1][i]);
    encode(&code, 18, 3, 0x541); /* DAT1=C.A is not a load. */
    struct hpclang_rpl_code_values saved = values;
    struct hpclang_rpl_code_origins saved_origins = origins;
    assert(hpclang_rpl_code_data_step(&instructions, &reader, 18, 110, &values, &origins,
                                      data_token, NULL, &length) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&values, &saved, sizeof values));
    assert(!memcmp(&origins, &saved_origins, sizeof origins));
    /* Sixteen-nibble literal wraps at P=15; no shift by 64 or origin rotation loss. */
    encode(&code, 32, 2, 0xf3);
    for (unsigned i = 0; i < 16; ++i)
        code.data[34 + i] = (uint8_t)i;
    values.registers[HPCLANG_RPL_CODE_P] = 15;
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 32, 96, &values, &origins,
                                       data_token, NULL, &length));
    assert(length == 18 && values.registers[HPCLANG_RPL_CODE_C] == UINT64_C(0x0fedcba987654321));
    for (unsigned i = 0; i < 16; ++i)
        assert(origins.registers[HPCLANG_RPL_CODE_C][(15 + i) & 15] == 0x100022 + i);
    encode(&code, 50, 3, 0xafa); /* A=C.W. */
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 50, 78, &values, &origins,
                                       data_token, NULL, &length));
    assert(values.registers[HPCLANG_RPL_CODE_A] == UINT64_C(0x0fedcba987654321));
    assert(!memcmp(origins.registers[HPCLANG_RPL_CODE_A], origins.registers[HPCLANG_RPL_CODE_C],
                   sizeof origins.registers[0]));
    encode(&code, 53, 3, 0x2fa); /* C=0.W kills every source. */
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 53, 75, &values, &origins,
                                       data_token, NULL, &length));
    assert(!values.registers[HPCLANG_RPL_CODE_C]);
    for (unsigned i = 0; i < 16; ++i)
        assert(!origins.registers[HPCLANG_RPL_CODE_C][i]);
    encode(&code, 56, 4, 0x5b18); /* C=PC does not invent encoded origins. */
    assert(!hpclang_rpl_code_data_step(&instructions, &reader, 56, 72, &values, &origins,
                                       data_token, NULL, &length));
    assert(values.registers[HPCLANG_RPL_CODE_C] == 60);
    for (unsigned i = 0; i < 5; ++i)
        assert(!origins.registers[HPCLANG_RPL_CODE_C][i]);
    values.registers[HPCLANG_RPL_CODE_D1] = 0xffffe;
    saved = values;
    saved_origins = origins;
    uint32_t old_length = length;
    assert(hpclang_rpl_code_data_step(&instructions, &reader, 8, 120, &values, &origins, data_token,
                                      NULL, &length) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&values, &saved, sizeof values));
    assert(!memcmp(&origins, &saved_origins, sizeof origins) && length == old_length);
}

static void control_steps(void)
{
    struct image code = {.size = 128, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &code};
    for (unsigned cursor = 0; cursor < 8; ++cursor)
        for (unsigned carry = 0; carry < 2; ++carry)
            for (unsigned variant = 0; variant < 7; ++variant) {
                struct hpclang_rpl_code_control state = {
                    .pc = 16, .carry = carry, .cursor = cursor};
                unsigned top = (cursor + 7) & 7;
                for (unsigned slot = 0; slot < 8; ++slot) {
                    state.stack[slot] = 0x90000 + slot;
                    for (unsigned i = 0; i < 5; ++i)
                        state.stack_origins[slot][i] = 1 + slot * 5 + i;
                }
                struct hpclang_rpl_code_control before = state;
                unsigned opcode = variant < 3 ? 0x10 * (variant + 1) : variant & 1 ? 4 : 5;
                encode(&code, 16, 3, opcode);
                if (variant >= 5)
                    encode(&code, 17, 2, variant == 5 ? 0x80 : 0x7f);
                struct hpclang_rpl_code_return_use use;
                memset(&use, 0xa5, sizeof use);
                assert(!hpclang_rpl_code_control_step(&reader, 112, &state, &use));
                int taken = variant < 3 || carry == (opcode == 4);
                int pop = taken && variant < 5;
                assert(use.present == (unsigned)pop);
                assert(state.carry == (variant == 1 ? 1u : variant == 2 ? 0u : carry));
                if (pop) {
                    assert(state.pc == before.stack[top] && state.cursor == top);
                    assert(use.target == before.stack[top]);
                    assert(!memcmp(use.origins, before.stack_origins[top], sizeof use.origins));
                    assert(!state.stack[top]);
                    for (unsigned i = 0; i < 5; ++i)
                        assert(!state.stack_origins[top][i]);
                } else {
                    uint32_t target = variant == 5 ? 0xfff91u : 144u;
                    assert(state.pc == (taken ? target : 19u));
                    assert(state.cursor == cursor);
                    assert(!memcmp(state.stack, before.stack, sizeof state.stack));
                    assert(!memcmp(state.stack_origins, before.stack_origins,
                                   sizeof state.stack_origins));
                }
                for (unsigned i = 0; i < (variant < 3 ? 2u : 3u); ++i) {
                    code.fail = 16 + i;
                    state = before;
                    struct hpclang_rpl_code_return_use old_use = use;
                    assert(hpclang_rpl_code_control_step(&reader, 112, &state, &use) ==
                           HPCLANG_RPL_SPAN_UNAVAILABLE);
                    assert(!memcmp(&state, &before, sizeof state));
                    assert(!memcmp(&use, &old_use, sizeof use));
                }
                code.fail = UINT32_MAX;
            }
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        struct hpclang_rpl_code_control state = {.cursor = cursor};
        state.stack[cursor] = 0xabcde;
        state.stack_generated[cursor] = 99;
        for (unsigned i = 0; i < 5; ++i)
            state.stack_origins[cursor][i] = i + 10;
        encode(&code, 0, 2, 0xf8);
        encode(&code, 2, 5, 32);
        encode(&code, 32, 2, 0x10);
        struct hpclang_rpl_code_return_use use;
        assert(!hpclang_rpl_code_control_step(&reader, 128, &state, &use));
        assert(state.pc == 32 && state.cursor == ((cursor + 1) & 7));
        assert(state.stack[cursor] == 7 && !use.present);
        assert(state.stack_generated[cursor] == 1);
        for (unsigned i = 0; i < 5; ++i)
            assert(!state.stack_origins[cursor][i]);
        assert(!hpclang_rpl_code_control_step(&reader, 96, &state, &use));
        assert(state.pc == 7 && state.cursor == cursor && use.present && use.target == 7);
        assert(use.generated == 1 && !state.stack_generated[cursor]);
        for (unsigned i = 0; i < 5; ++i)
            assert(!use.origins[i]);
        state.pc = 0;
        encode(&code, 32, 2, 0xf8);
        encode(&code, 34, 5, 64);
        encode(&code, 39, 2, 0x10);
        encode(&code, 64, 2, 0x10);
        assert(!hpclang_rpl_code_control_step(&reader, 128, &state, &use));
        assert(!hpclang_rpl_code_control_step(&reader, 96, &state, &use));
        assert(!hpclang_rpl_code_control_step(&reader, 64, &state, &use));
        assert(use.generated == 33 && use.target == 39);
        assert(!hpclang_rpl_code_control_step(&reader, 89, &state, &use));
        assert(use.generated == 1 && use.target == 7 && state.cursor == cursor);
    }
    struct hpclang_rpl_code_control state = {0}, before = state;
    struct hpclang_rpl_code_return_use use = {0}, old_use = use;
    encode(&code, 0, 2, 0); /* RTNSXM requires mode state we do not model. */
    assert(hpclang_rpl_code_control_step(&reader, 128, &state, &use) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&state, &before, sizeof state) && !memcmp(&use, &old_use, sizeof use));
    encode(&code, 0, 2, 0x20);
    state.stack[7] = 0x100000;
    before = state;
    assert(hpclang_rpl_code_control_step(&reader, 128, &state, &use) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&state, &before, sizeof state) && !memcmp(&use, &old_use, sizeof use));
}

static void arithmetic_modes(void)
{
    struct image code = {.size = 128, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &code};
    const uint32_t inputs[] = {0, 9, 0x99999, 0xfffff};
    const uint32_t expected[2][4] = {{1, 10, 0x9999a, 0}, {1, 0x10, 0, 0}};
    for (unsigned decimal = 0; decimal < 2; ++decimal)
        for (unsigned i = 0; i < 4; ++i) {
            struct hpclang_rpl_code_values values = {.carry = 1, .decimal = 1 - decimal};
            struct hpclang_rpl_code_origins origins;
            seed_origins(&origins);
            struct hpclang_rpl_code_origins original = origins;
            encode(&code, 0, 2, decimal ? 0x50 : 0x40);
            encode(&code, 2, 2, 0x7e);
            uint32_t length = 123;
            assert(!hpclang_rpl_code_data_step(&reader, NULL, 0, 128, &values, &origins, data_token,
                                               NULL, &length));
            assert(length == 2 && values.decimal == decimal && values.carry == 1);
            assert(!memcmp(&origins, &original, sizeof origins));
            values.registers[HPCLANG_RPL_CODE_D] = UINT64_C(0xabcdef0000000000) | inputs[i];
            struct hpclang_rpl_code_values saved = values;
            int result = hpclang_rpl_code_data_step(&reader, NULL, 2, 126, &values, &origins,
                                                    data_token, NULL, &length);
            if (decimal && i == 3) {
                assert(result == HPCLANG_RPL_SPAN_UNSUPPORTED);
                assert(!memcmp(&values, &saved, sizeof values));
                assert(!memcmp(&origins, &original, sizeof origins));
            } else {
                assert(!result && values.registers[HPCLANG_RPL_CODE_D] ==
                                      (UINT64_C(0xabcdef0000000000) | expected[decimal][i]));
                assert(values.carry == (decimal ? i == 2 : i == 3));
                for (unsigned digit = 0; digit < 5; ++digit)
                    original.registers[HPCLANG_RPL_CODE_D][digit] = 0;
                assert(!memcmp(&origins, &original, sizeof origins));
            }
        }
}

static void execution_steps(void)
{
    struct image code = {.size = 128, .fail = UINT32_MAX};
    struct image memory = {.address = 0x18000, .size = 128, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader instructions = {read_nibble, &code};
    const struct hpclang_rpl_reader data = {read_nibble, &memory};
    struct hpclang_rpl_code_execution state = {0};
    seed_origins(&state.origins);
    state.values.registers[HPCLANG_RPL_CODE_D1] = memory.address;
    encode(&code, 0, 3, 0x741); /* C=DAT1.A, then PC=(C). */
    encode(&code, 3, 4, 0xe808);
    encode(&memory, 0, 5, 0x18010);
    encode(&memory, 16, 5, 0x94000);
    struct hpclang_rpl_code_step_uses uses;
    assert(!hpclang_rpl_code_execution_step(&instructions, &data, 128, &state, data_token, NULL,
                                            &uses));
    assert(state.control.pc == 3 && state.values.registers[HPCLANG_RPL_CODE_C] == 0x18010);
    assert(uses.count == 1 && !uses.has_branch);
    assert(uses.addresses[0].kind == HPCLANG_RPL_CODE_USE_LOAD_ADDRESS);
    assert(uses.addresses[0].value == memory.address);
    assert(!memcmp(uses.addresses[0].origins, state.origins.registers[HPCLANG_RPL_CODE_D1],
                   sizeof uses.addresses[0].origins));
    struct hpclang_rpl_code_execution saved = state;
    struct hpclang_rpl_code_step_uses old_uses = uses;
    for (unsigned i = 0; i < 5; ++i) {
        uint32_t reject = 0x18010 + i;
        assert(hpclang_rpl_code_execution_step(&instructions, &data, 125, &state, data_token,
                                               &reject, &uses) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&state, &saved, sizeof state) && !memcmp(&uses, &old_uses, sizeof uses));
        memory.fail = reject;
        assert(hpclang_rpl_code_execution_step(&instructions, &data, 125, &state, data_token, NULL,
                                               &uses) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&state, &saved, sizeof state) && !memcmp(&uses, &old_uses, sizeof uses));
        memory.fail = UINT32_MAX;
    }
    assert(!hpclang_rpl_code_execution_step(&instructions, &data, 125, &state, data_token, NULL,
                                            &uses));
    assert(state.control.pc == 0x94000 && uses.count == 2 && !uses.has_branch);
    assert(uses.addresses[0].kind == HPCLANG_RPL_CODE_USE_REGISTER_TARGET);
    assert(uses.addresses[0].value == 0x18010);
    assert(uses.addresses[1].kind == HPCLANG_RPL_CODE_USE_MEMORY_TARGET);
    assert(uses.addresses[1].value == 0x94000);
    for (unsigned i = 0; i < 5; ++i) {
        assert(uses.addresses[0].origins[i] == 0x218000 + i);
        assert(uses.addresses[1].origins[i] == 0x218010 + i);
    }
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        state = saved;
        state.control.pc = 16;
        state.control.cursor = cursor;
        encode(&code, 16, 2, 0x60); /* Copy C to stack, then pop to C and jump. */
        encode(&code, 18, 2, 0x70);
        encode(&code, 20, 4, 0x3b18);
        for (unsigned step = 0; step < 3; ++step) {
            assert(!hpclang_rpl_code_execution_step(&instructions, &data, 128 - state.control.pc,
                                                    &state, data_token, NULL, &uses));
            assert(uses.count == (step == 2));
        }
        assert(state.control.pc == 0x18010 && state.control.cursor == cursor);
        assert(!state.control.stack[cursor]);
        assert(!memcmp(uses.addresses[0].origins, saved.origins.registers[HPCLANG_RPL_CODE_C],
                       sizeof uses.addresses[0].origins));
    }
    state = saved;
    state.control.pc = 32;
    encode(&code, 32, 4, 0x7b18); /* CPCEX consumes original C before killing its origins. */
    assert(!hpclang_rpl_code_execution_step(&instructions, &data, 96, &state, data_token, NULL,
                                            &uses));
    assert(state.control.pc == 0x18010 && state.values.registers[HPCLANG_RPL_CODE_C] == 36);
    assert(uses.count == 1 && uses.addresses[0].value == 0x18010);
    for (unsigned i = 0; i < 5; ++i)
        assert(!state.origins.registers[HPCLANG_RPL_CODE_C][i]);
    state = saved;
    state.control.pc = 40;
    encode(&code, 40, 2, 0xf8);
    encode(&code, 42, 5, 64);
    encode(&code, 64, 2, 0x10);
    assert(!hpclang_rpl_code_execution_step(&instructions, &data, 88, &state, data_token, NULL,
                                            &uses));
    assert(uses.has_branch && uses.branch.call && uses.branch.target == 64 && !uses.count);
    assert(
        !hpclang_rpl_code_execution_step(&instructions, &data, 2, &state, data_token, NULL, &uses));
    assert(state.control.pc == 47 && uses.count == 1);
    assert(uses.addresses[0].kind == HPCLANG_RPL_CODE_USE_RETURN_TARGET);
    for (unsigned i = 0; i < 5; ++i)
        assert(!uses.addresses[0].origins[i]);
}

static void exhaustive(void)
{
    const unsigned opcodes[] = {4, 5, 6, 7, 12, 14};
    struct hpclang_rpl_reference_mappings maps = {1, 1, 1};
    for (unsigned op = 0; op < 6; ++op) {
        unsigned width = op < 2 ? 2 : op < 4 ? 3 : 4;
        unsigned prefix = op < 4 ? 1 : 2;
        unsigned length = prefix + width;
        int call = op == 3 || op == 5;
        struct image image = {.address = 0x80000, .size = length, .fail = UINT32_MAX};
        image.data[0] = op < 4 ? opcodes[op] : 8;
        if (prefix == 2)
            image.data[1] = opcodes[op];
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        int32_t half = 1 << (width * 4 - 1);
        for (int32_t displacement = -half; displacement < half; ++displacement) {
            encode(&image, prefix, width, (uint32_t)displacement);
            struct hpclang_rpl_code_branch branch;
            int result = hpclang_rpl_code_branch_inspect(&reader, image.address, length, &branch);
            if (op < 2 && displacement == 0) {
                assert(result == HPCLANG_RPL_SPAN_UNSUPPORTED);
                continue;
            }
            assert(result == 0);
            uint32_t base = image.address + (call ? length : prefix);
            uint32_t target = (uint32_t)((int32_t)base + displacement);
            assert(branch.target == target && branch.base == base);
            assert(branch.width == width && branch.length == length);
            assert(branch.call == (unsigned)call && branch.conditional == (op < 2));
            struct hpclang_rpl_code_patch patch = sentinel;
            assert(hpclang_rpl_code_branch_plan_shift(&reader, image.address, length, &maps,
                                                      0x70000, 0x90000, 16, &patch) == 0);
            assert(memcmp(&patch, &sentinel, sizeof patch) == 0);
            /* Move only the instruction's mapping; target mapping stays put.
             * Execute the planned operand from the new PC independently. */
            for (int shift = -16; shift <= 16; shift += 32) {
                maps.target = 2;
                result = hpclang_rpl_code_branch_plan_shift(&reader, image.address, length, &maps,
                                                            image.address, image.address + length,
                                                            shift, &patch);
                maps.target = 1;
                if (displacement - shift < -half || displacement - shift >= half)
                    assert(result == HPCLANG_RPL_SPAN_BOUNDS);
                else if (op < 2 && displacement == shift)
                    assert(result == HPCLANG_RPL_SPAN_UNSUPPORTED);
                else {
                    assert(result == 1);
                    int32_t repaired = (int32_t)patch.value;
                    if (repaired >= half)
                        repaired -= half * 2;
                    assert((int32_t)base + shift + repaired == (int32_t)target);
                    assert(patch.field == image.address + prefix && patch.width == width);
                }
            }
        }
    }
}

static void native_cross_object_call(void)
{
    /* SETUP: GOSUB at ED5DF targets ED277 in a different CODE object.
     * Operand is relative to the instruction end, not its operand field. */
    struct image image = {{7, 4, 9, 12}, 0xed5df, 4, UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_code_branch branch;
    assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 4, &branch) == 0);
    assert(branch.target == 0xed277);
    struct hpclang_rpl_reference_mappings maps = {1, 1, 1};
    struct hpclang_rpl_code_patch patch;
    assert(hpclang_rpl_code_branch_plan_shift(&reader, image.address, 4, &maps, 0xed19b, 0xed659,
                                              16, &patch) == 0);
    assert(hpclang_rpl_code_branch_plan_shift(&reader, image.address, 4, &maps, 0xed19b, 0xed574,
                                              16, &patch) == 1);
    assert(patch.expected == 0xc94 && patch.value == 0xca4 && patch.width == 3);
    for (unsigned i = 0; i < 3; ++i)
        image.data[1 + i] = (patch.value >> (4 * i)) & 15;
    assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 4, &branch) == 0);
    assert(branch.target == 0xed287);
}

static void failures_and_wrap(void)
{
    struct image image = {{8, 14, 0, 0, 0, 0}, 0xffffa, 6, UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_reference_mappings maps = {1, 1, 1};
    struct hpclang_rpl_code_branch branch = {0}, before = branch;
    assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 6, &branch) == 0);
    assert(branch.base == 0 && branch.target == 0);
    struct hpclang_rpl_code_patch patch = sentinel;
    assert(hpclang_rpl_code_branch_plan_shift(&reader, image.address, 6, &maps, image.address,
                                              0x100000, -16, &patch) == 1);
    assert(patch.value == 16);
    for (unsigned i = 0; i < 6; ++i) {
        branch = before;
        image.fail = image.address + i;
        assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 6, &branch) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(memcmp(&branch, &before, sizeof branch) == 0);
        image.fail = UINT32_MAX;
        uint8_t saved = image.data[i];
        image.data[i] = 16;
        assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 6, &branch) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        image.data[i] = saved;
        assert(hpclang_rpl_code_branch_inspect(&reader, image.address, i, &branch) ==
               HPCLANG_RPL_SPAN_BOUNDS);
    }
    assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 7, &branch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_code_branch_inspect(NULL, image.address, 6, &branch) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    for (unsigned cut = 1; cut < 6; ++cut) {
        patch = sentinel;
        assert(hpclang_rpl_code_branch_plan_shift(&reader, image.address, 6, &maps,
                                                  image.address + cut, 0x100000, -16,
                                                  &patch) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(memcmp(&patch, &sentinel, sizeof patch) == 0);
        assert(hpclang_rpl_code_branch_plan_shift(&reader, image.address, 6, &maps, image.address,
                                                  image.address + cut, -16,
                                                  &patch) == HPCLANG_RPL_SPAN_BOUNDS);
    }
    image.address = 0x80000;
    for (unsigned op = 0; op < 16; ++op) {
        if (op >= 4 && op <= 9)
            continue;
        image.data[0] = op;
        assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 6, &branch) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
    }
    image.data[0] = 8;
    for (unsigned op = 0; op < 16; ++op) {
        if (op == 0 || op == 3 || op >= 6)
            continue;
        image.data[1] = op;
        assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 6, &branch) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
    }
}

static void predicate_case(struct image *image, unsigned prefix)
{
    image->address = 0x80000;
    image->size = prefix + 2;
    image->fail = UINT32_MAX;
    struct hpclang_rpl_reader reader = {read_nibble, image};
    struct hpclang_rpl_reference_mappings maps = {1, 2, 1};
    for (unsigned operand = 0; operand < 256; ++operand) {
        encode(image, prefix, 2, operand);
        struct hpclang_rpl_code_branch branch;
        int result = hpclang_rpl_code_branch_inspect(&reader, image->address, image->size, &branch);
        if (!operand) {
            assert(result == HPCLANG_RPL_SPAN_UNSUPPORTED);
            continue;
        }
        assert(result == 0 && branch.conditional && !branch.call);
        int32_t displacement = operand < 128 ? (int32_t)operand : (int32_t)operand - 256;
        uint32_t target = image->address + prefix + displacement;
        assert(branch.target == target && branch.width == 2 && branch.length == image->size);
        struct hpclang_rpl_code_patch patch = sentinel;
        result = hpclang_rpl_code_branch_plan_shift(&reader, image->address, image->size, &maps,
                                                    image->address, image->address + image->size,
                                                    16, &patch);
        if (displacement < -112)
            assert(result == HPCLANG_RPL_SPAN_BOUNDS);
        else if (displacement == 16)
            assert(result == HPCLANG_RPL_SPAN_UNSUPPORTED);
        else {
            assert(result == 1 && patch.width == 2 && patch.field == image->address + prefix);
            int32_t repaired =
                patch.value < 128 ? (int32_t)patch.value : (int32_t)patch.value - 256;
            assert(image->address + 16 + prefix + repaired == target);
        }
    }
    for (unsigned i = 0; i < image->size; ++i) {
        struct hpclang_rpl_code_branch branch = {0}, before = branch;
        image->fail = image->address + i;
        assert(hpclang_rpl_code_branch_inspect(&reader, image->address, image->size, &branch) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(memcmp(&branch, &before, sizeof branch) == 0);
        image->fail = UINT32_MAX;
        uint8_t saved = image->data[i];
        image->data[i] = 16;
        assert(hpclang_rpl_code_branch_inspect(&reader, image->address, image->size, &branch) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        image->data[i] = saved;
        assert(hpclang_rpl_code_branch_inspect(&reader, image->address, i, &branch) ==
               HPCLANG_RPL_SPAN_BOUNDS);
    }
}

static void predicates(void)
{
    struct image image = {0};
    image.data[0] = 9;
    for (unsigned field = 0; field < 16; ++field) {
        image.data[1] = field;
        for (unsigned comparison = 0; comparison < 16; ++comparison) {
            image.data[2] = comparison;
            predicate_case(&image, 3);
        }
    }
    image.data[0] = 8;
    const unsigned kinds[] = {3, 6, 7, 8, 9, 10, 11};
    for (unsigned k = 0; k < sizeof kinds / sizeof kinds[0]; ++k) {
        image.data[1] = kinds[k];
        for (unsigned operand = 0; operand < 16; ++operand) {
            image.data[2] = operand;
            predicate_case(&image, 3);
        }
    }
    image.data[1] = 0;
    image.data[2] = 8;
    const unsigned bit_tests[] = {6, 7, 10, 11};
    for (unsigned k = 0; k < 4; ++k) {
        image.data[3] = bit_tests[k];
        for (unsigned bit = 0; bit < 16; ++bit) {
            image.data[4] = bit;
            predicate_case(&image, 5);
        }
    }
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_code_branch branch;
    for (unsigned sub = 0; sub < 16; ++sub) {
        if (sub == 6 || sub == 7 || sub == 10 || sub == 11)
            continue;
        image.data[3] = sub;
        assert(hpclang_rpl_code_branch_inspect(&reader, image.address, image.size, &branch) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
    }
}

static void absolute_branches(void)
{
    struct image image = {{8, 13}, 0x80000, 7, UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    const uint32_t targets[] = {0,       1,       0x7ffff, 0x80000, 0x80001,
                                0x80006, 0x80007, 0xfffef, 0xffff0, 0xfffff};
    for (unsigned opcode = 13; opcode <= 15; opcode += 2) {
        image.data[1] = opcode;
        for (unsigned t = 0; t < sizeof targets / sizeof targets[0]; ++t) {
            encode(&image, 2, 5, targets[t]);
            struct hpclang_rpl_code_branch branch;
            assert(!hpclang_rpl_code_branch_inspect(&reader, image.address, 7, &branch));
            assert(branch.base == 0 && branch.width == 5 && branch.length == 7 &&
                   branch.target == targets[t] && branch.call == (opcode == 15) &&
                   !branch.conditional);
            for (unsigned mapping = 0; mapping < 2; ++mapping) {
                struct hpclang_rpl_reference_mappings maps = {0, mapping, 0};
                struct hpclang_rpl_code_patch patch = sentinel;
                int moves = !mapping && targets[t] >= 0x80000 && targets[t] < 0xffff0;
                int result = hpclang_rpl_code_branch_plan_shift(&reader, image.address, 7, &maps,
                                                                0x80000, 0xffff0, 16, &patch);
                assert(result == moves);
                if (moves)
                    assert(patch.field == 0x80002 && patch.width == 5 &&
                           patch.expected == targets[t] && patch.value == targets[t] + 16);
                else
                    assert(!memcmp(&patch, &sentinel, sizeof patch));
            }
        }
        for (unsigned at = 0; at < 7; ++at) {
            struct hpclang_rpl_code_branch branch;
            image.fail = image.address + at;
            assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 7, &branch) ==
                   HPCLANG_RPL_SPAN_UNAVAILABLE);
            image.fail = UINT32_MAX;
            assert(hpclang_rpl_code_branch_inspect(&reader, image.address, at, &branch) ==
                   HPCLANG_RPL_SPAN_BOUNDS);
        }
    }
}

static void absolute_data_pointers(void)
{
    struct image image = {{1, 11}, 0x10000, 7, UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_reference_mappings maps = {0, 0, 0};
    for (unsigned opcode = 11; opcode <= 15; opcode += 4) {
        image.data[1] = opcode;
        for (uint32_t target = 0; target < 0x100000; ++target) {
            encode(&image, 2, 5, target);
            struct hpclang_rpl_code_patch patch = sentinel;
            int moves = target >= 0x80000 && target < 0xfffe0;
            assert(hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps, 0x80000,
                                                       0xfffe0, 16, &patch) == moves);
            if (moves)
                assert(patch.field == 0x10002 && patch.width == 5 && patch.expected == target &&
                       patch.value == target + 16);
            else
                assert(!memcmp(&patch, &sentinel, sizeof patch));
            maps.target = 1;
            patch = sentinel;
            assert(!hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps, 0x80000,
                                                        0xfffe0, 16, &patch));
            assert(!memcmp(&patch, &sentinel, sizeof patch));
            maps.target = 0;
        }
        for (unsigned i = 0; i < 7; ++i) {
            struct hpclang_rpl_code_patch patch = sentinel;
            image.fail = image.address + i;
            assert(hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps, 0x80000,
                                                       0xfffe0, 16,
                                                       &patch) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            image.fail = UINT32_MAX;
            uint8_t saved = image.data[i];
            image.data[i] = 16;
            assert(hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps, 0x80000,
                                                       0xfffe0, 16,
                                                       &patch) == HPCLANG_RPL_SPAN_MALFORMED);
            image.data[i] = saved;
            assert(hpclang_rpl_code_pointer_plan_shift(&reader, image.address, i, &maps, 0x80000,
                                                       0xfffe0, 16,
                                                       &patch) == HPCLANG_RPL_SPAN_BOUNDS);
            assert(!memcmp(&patch, &sentinel, sizeof patch));
        }
        struct hpclang_rpl_code_patch patch = sentinel;
        encode(&image, 2, 5, 0x90000);
        assert(!hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps, image.address,
                                                    image.address + 7, -16, &patch));
        assert(!memcmp(&patch, &sentinel, sizeof patch));
        assert(hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps,
                                                   image.address + 1, image.address + 7, 16,
                                                   &patch) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps, 0x90000,
                                                   0x90001, -16, &patch) == 1);
        assert(patch.value == 0x8fff0);
    }
    for (unsigned opcode = 0; opcode < 256; ++opcode) {
        if (opcode == 0xb1 || opcode == 0xf1)
            continue;
        encode(&image, 0, 2, opcode);
        struct hpclang_rpl_code_patch patch = sentinel;
        assert(hpclang_rpl_code_pointer_plan_shift(&reader, image.address, 7, &maps, 0x80000,
                                                   0xfffe0, 16,
                                                   &patch) == HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!memcmp(&patch, &sentinel, sizeof patch));
    }
}

static void indirect_transfers(void)
{
    struct image code = {{0}, 0x40000, 4, UINT32_MAX};
    struct image data = {{0}, 0x40000, 5, UINT32_MAX};
    struct hpclang_rpl_reader instructions = {read_nibble, &code};
    struct hpclang_rpl_reader pointers = {read_nibble, &data};
    const struct hpclang_rpl_code_indirect unchanged = {1, 2, 3, 4, 5};
    encode(&data, 0, 5, 0xabcde);
    for (unsigned opcode = 0; opcode < 0x10000; ++opcode) {
        encode(&code, 0, 4, opcode);
        struct hpclang_rpl_code_indirect transfer = unchanged;
        int supported =
            opcode == 0x2b18 || opcode == 0x3b18 || opcode == 0xc808 || opcode == 0xe808;
        unsigned selects_c = opcode == 0x3b18 || opcode == 0xe808;
        int result = hpclang_rpl_code_indirect_inspect(
            &instructions, &pointers, code.address, 4, selects_c ? 0xabc40001 : 0xabc40000,
            selects_c ? 0xdef40000 : 0xdef40001, &transfer);
        if (!supported) {
            assert(result == HPCLANG_RPL_SPAN_UNSUPPORTED);
            assert(!memcmp(&transfer, &unchanged, sizeof transfer));
            continue;
        }
        assert(!result && transfer.instruction == code.address);
        assert(transfer.register_c == (opcode == 0x3b18 || opcode == 0xe808));
        assert(transfer.via_memory == (opcode == 0xc808 || opcode == 0xe808));
        assert(transfer.register_value == 0x40000);
        assert(transfer.target == (transfer.via_memory ? 0xabcde : 0x40000));
        /* Numerically identical code/data addresses resolve through distinct
         * readers. Register jumps must not dereference either register. */
        if (!transfer.via_memory) {
            assert(!hpclang_rpl_code_indirect_inspect(&instructions, NULL, code.address, 4,
                                                      0x123fffff, 0x456fffff, &transfer));
            assert(transfer.target == 0xfffff);
        }
    }
    encode(&code, 0, 4, 0xc808);
    for (unsigned which = 0; which < 2; ++which) {
        struct image *image = which ? &data : &code;
        for (unsigned i = 0; i < image->size; ++i) {
            struct hpclang_rpl_code_indirect transfer = unchanged;
            image->fail = image->address + i;
            assert(hpclang_rpl_code_indirect_inspect(&instructions, &pointers, code.address, 4,
                                                     data.address, 0,
                                                     &transfer) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            image->fail = UINT32_MAX;
            uint8_t saved = image->data[i];
            image->data[i] = 16;
            assert(hpclang_rpl_code_indirect_inspect(&instructions, &pointers, code.address, 4,
                                                     data.address, 0,
                                                     &transfer) == HPCLANG_RPL_SPAN_MALFORMED);
            image->data[i] = saved;
            assert(!memcmp(&transfer, &unchanged, sizeof transfer));
        }
    }
    struct hpclang_rpl_code_indirect transfer = unchanged;
    for (unsigned limit = 0; limit < 4; ++limit)
        assert(hpclang_rpl_code_indirect_inspect(&instructions, &pointers, code.address, limit,
                                                 data.address, 0,
                                                 &transfer) == HPCLANG_RPL_SPAN_BOUNDS);
    for (uint32_t at = 0xffffc; at <= 0xfffff; ++at)
        assert(hpclang_rpl_code_indirect_inspect(&instructions, &pointers, code.address, 4, at, 0,
                                                 &transfer) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_code_indirect_inspect(&instructions, NULL, code.address, 4, data.address, 0,
                                             &transfer) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&transfer, &unchanged, sizeof transfer));
    data.address = 0xffffb;
    code.address = 0xffffc;
    assert(!hpclang_rpl_code_indirect_inspect(&instructions, &pointers, code.address, 4,
                                              data.address, 0, &transfer));
    assert(transfer.target == 0xabcde);
    assert(hpclang_rpl_code_indirect_inspect(&instructions, &pointers, code.address + 1, 4,
                                             data.address, 0,
                                             &transfer) == HPCLANG_RPL_SPAN_BOUNDS);
}

struct visit_state
{
    uint32_t next, count, stop;
};

static int visit_instruction(void *context, uint32_t at, uint32_t length)
{
    struct visit_state *state = context;
    assert(at == state->next);
    state->next += length;
    return ++state->count == state->stop ? 123 : 0;
}

static void instruction_extents(void)
{
    /* Literal encodings independently transcribed from the Saturn instruction
     * reference and Rechlin's Saturn+ opcode notes. Include opcode-looking
     * immediate data, conditional returns and the historically misdecoded
     * five-nibble 819 field/register form. 80B26 is independently framed by
     * the pinned HP49g+/HP50g native wrapper at ARM 0x30ee4. */
    const char *const codes[] = {"00",      "0ef0",
                                 "100",     "10c",
                                 "11c",     "12c",
                                 "13f",     "14f",
                                 "15ff",    "160",
                                 "170",     "180",
                                 "1900",    "1a0000",
                                 "1b00000", "1c0",
                                 "1d00",    "1e0000",
                                 "1f00000", "2f",
                                 "308",     "3f80bfe808281b201f",
                                 "400",     "500",
                                 "6000",    "7000",
                                 "800",     "807",
                                 "8080",    "80810",
                                 "808208",  "8082f80bfe808281b201f",
                                 "8083",    "8084f",
                                 "8085f",   "8086f00",
                                 "8087f00", "8088f",
                                 "8089f",   "808af00",
                                 "808bf00", "808c",
                                 "808d",    "808e",
                                 "808f",    "809",
                                 "80a",     "80cf",
                                 "80df",    "80e",
                                 "80ff",    "810",
                                 "817",     "818f0f",
                                 "8188bf",  "819f3",
                                 "81a80c",  "81af2c",
                                 "81b2",    "81b3",
                                 "81b4",    "81b5",
                                 "81b6",    "81b7",
                                 "81c",     "81f",
                                 "82f",     "83f00",
                                 "84f",     "85f",
                                 "86f00",   "87f00",
                                 "88f00",   "89f00",
                                 "8af00",   "8bf00",
                                 "8c0000",  "8d00000",
                                 "8e0000",  "8f00000",
                                 "9ff00",   "aff",
                                 "bff",     "cf",
                                 "df",      "ef",
                                 "ff",      "80b083f",
                                 "80b0877", "80b08c7",
                                 "80b18d3", "80b8827",
                                 "80b8897", "80b9811",
                                 "80b9887", "80b60",
                                 "80b70",   "80bfe",
                                 "80bff",   "80b82f",
                                 "80b26"};
    for (unsigned c = 0; c < sizeof codes / sizeof codes[0]; ++c) {
        struct image image = {.address = 0x80000, .size = 64, .fail = UINT32_MAX};
        uint32_t expected = (uint32_t)strlen(codes[c]);
        for (unsigned i = 0; i < expected; ++i)
            image.data[i] = codes[c][i] <= '9' ? codes[c][i] - '0' : codes[c][i] - 'a' + 10;
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        uint32_t length = 123;
        assert(!hpclang_rpl_code_extent(&reader, image.address, image.size, &length));
        assert(length == expected);
        assert(!hpclang_rpl_code_extent(&reader, image.address, expected, &length));
        for (unsigned i = 0; i < expected; ++i) {
            length = 123;
            assert(hpclang_rpl_code_extent(&reader, image.address, i, &length) ==
                   HPCLANG_RPL_SPAN_BOUNDS);
            assert(length == 123);
            image.fail = image.address + i;
            assert(hpclang_rpl_code_extent(&reader, image.address, expected, &length) ==
                   HPCLANG_RPL_SPAN_UNAVAILABLE);
            assert(length == 123);
            image.fail = UINT32_MAX;
            uint8_t saved = image.data[i];
            image.data[i] = 16;
            assert(hpclang_rpl_code_extent(&reader, image.address, expected, &length) ==
                   HPCLANG_RPL_SPAN_MALFORMED);
            assert(length == 123);
            image.data[i] = saved;
        }
        image.address = 0x100000u - expected;
        assert(!hpclang_rpl_code_extent(&reader, image.address, expected, &length));
        assert(hpclang_rpl_code_extent(&reader, image.address + 1, expected, &length) ==
               HPCLANG_RPL_SPAN_BOUNDS);
    }
    /* All immediate lengths, including maximum LA (21) and LC (18). */
    for (unsigned n = 0; n < 16; ++n) {
        for (unsigned la = 0; la < 2; ++la) {
            struct image image = {{3, 0}, 0x80000, 64, UINT32_MAX};
            if (la) {
                const uint8_t prefix[] = {8, 0, 8, 2};
                memcpy(image.data, prefix, sizeof prefix);
            }
            image.data[la ? 4 : 1] = n;
            uint32_t length = 0;
            struct hpclang_rpl_reader reader = {read_nibble, &image};
            assert(!hpclang_rpl_code_extent(&reader, image.address, image.size, &length));
            assert(length == n + (la ? 6 : 3));
        }
    }
    const char *const unknown[] = {"105",      "11f",     "12d",     "80811", "818f40", "819f4",
                                   "81af30",   "81af2d",  "81b0",    "81b8",  "80b02",  "80bf7f",
                                   "80b83f00", "80b2858", "80b08e8", "80b36"};
    for (unsigned c = 0; c < sizeof unknown / sizeof unknown[0]; ++c) {
        struct image image = {.address = 0x80000, .size = 64, .fail = UINT32_MAX};
        for (unsigned i = 0; unknown[c][i]; ++i)
            image.data[i] = unknown[c][i] <= '9' ? unknown[c][i] - '0' : unknown[c][i] - 'a' + 10;
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        uint32_t length = 123;
        assert(hpclang_rpl_code_extent(&reader, image.address, image.size, &length) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(length == 123);
    }
    struct image image = {{3, 3, 8, 0, 11, 15, 8, 1, 9, 15, 3, 4, 0, 0}, 0xffff2, 14, UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct visit_state state = {image.address, 0, 0};
    assert(!hpclang_rpl_code_visit(&reader, image.address, image.size, visit_instruction, &state));
    assert(state.count == 3 && state.next == 0x100000);
    state = (struct visit_state){image.address, 0, 2};
    assert(hpclang_rpl_code_visit(&reader, image.address, image.size, visit_instruction, &state) ==
           123);
    assert(state.count == 2 && state.next == image.address + 11);
    state = (struct visit_state){image.address, 0, 0};
    assert(hpclang_rpl_code_visit(&reader, image.address, image.size - 1, visit_instruction,
                                  &state) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(state.count == 2);
    assert(!hpclang_rpl_code_visit(&reader, image.address, 0, visit_instruction, &state));
    assert(state.count == 2);
    assert(hpclang_rpl_code_visit(NULL, 0, 0, visit_instruction, &state) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
}

static void native_service_continuation(void)
{
    /* Mapped ROM 24CB0: service, LC(5) 24E2A, D=C.A, RTN. */
    struct image image = {
        {8, 0, 11, 2, 6, 3, 4, 10, 2, 14, 4, 2, 13, 7, 0, 1}, 0x24cb0, 16, UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct visit_state state = {image.address, 0, 0};
    assert(!hpclang_rpl_code_visit(&reader, image.address, image.size, visit_instruction, &state));
    assert(state.count == 4 && state.next == 0x24cc0);
    image.fail = image.address + 5; /* Framing must not read the continuation. */
    uint32_t length = 0;
    assert(!hpclang_rpl_code_extent(&reader, image.address, image.size, &length));
    assert(length == 5);
    struct hpclang_rpl_code_branch branch = {0}, unchanged = branch;
    assert(hpclang_rpl_code_branch_inspect(&reader, image.address, 5, &branch) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&branch, &unchanged, sizeof branch));
}

static void register_assignments(void)
{
    const struct
    {
        const char *code;
        unsigned p, source, destination, source_first, first, width, constant;
    } cases[] = {
        {"130", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D0, 0, 0, 5, 0},
        {"131", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D1, 0, 0, 5, 0},
        {"134", 15, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D0, 0, 0, 5, 0},
        {"135", 15, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D1, 0, 0, 5, 0},
        {"138", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D0, 0, 0, 4, 0},
        {"139", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D1, 0, 0, 4, 0},
        {"13c", 15, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D0, 0, 0, 4, 0},
        {"13d", 15, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D1, 0, 0, 4, 0},
        {"db", 0, HPCLANG_RPL_CODE_D, HPCLANG_RPL_CODE_C, 0, 0, 5, 0},
        {"10c", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_R4, 0, 0, 16, 0},
        {"110", 0, HPCLANG_RPL_CODE_R0, HPCLANG_RPL_CODE_A, 0, 0, 16, 0},
        {"81af1c", 0, HPCLANG_RPL_CODE_R4, HPCLANG_RPL_CODE_C, 0, 0, 5, 0},
        {"81a41c", 0, HPCLANG_RPL_CODE_R4, HPCLANG_RPL_CODE_C, 15, 15, 1, 0},
        {"a94", 9, HPCLANG_RPL_CODE_B, HPCLANG_RPL_CODE_A, 0, 0, 10, 0},
        {"af7", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D, 0, 0, 16, 0},
        {"a83", 15, HPCLANG_RPL_CODE_CONSTANT, HPCLANG_RPL_CODE_D, 0, 15, 1, 0},
        {"d0", 0, HPCLANG_RPL_CODE_CONSTANT, HPCLANG_RPL_CODE_A, 0, 0, 5, 0},
        {"80cf", 2, HPCLANG_RPL_CODE_P, HPCLANG_RPL_CODE_C, 0, 15, 1, 0},
        {"80d2", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_P, 2, 0, 1, 0},
        {"2f", 0, HPCLANG_RPL_CODE_CONSTANT, HPCLANG_RPL_CODE_P, 0, 0, 1, 15},
        {"81b4", 0, HPCLANG_RPL_CODE_NEXT_PC, HPCLANG_RPL_CODE_A, 0, 0, 5, 0x80004},
        {"81b5", 15, HPCLANG_RPL_CODE_NEXT_PC, HPCLANG_RPL_CODE_C, 0, 0, 5, 0x80004},
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        struct image image = {
            .address = 0x80000, .size = (uint32_t)strlen(cases[i].code), .fail = UINT32_MAX};
        for (unsigned n = 0; n < image.size; ++n)
            image.data[n] =
                cases[i].code[n] <= '9' ? cases[i].code[n] - '0' : cases[i].code[n] - 'a' + 10;
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        struct hpclang_rpl_code_assignment value, untouched;
        memset(&untouched, 0xa5, sizeof untouched);
        value = untouched;
        assert(!hpclang_rpl_code_assignment_inspect(&reader, image.address, image.size, cases[i].p,
                                                    &value));
        assert(value.instruction == image.address && value.length == image.size);
        assert((unsigned)value.source == cases[i].source &&
               (unsigned)value.destination == cases[i].destination);
        assert(value.source_first == cases[i].source_first &&
               value.destination_first == cases[i].first);
        assert(value.nibbles == cases[i].width && value.constant == cases[i].constant);
        struct hpclang_rpl_code_origins origins, expected;
        seed_origins(&origins);
        expected = origins;
        const uint32_t pc_tokens[] = {1001, 1002, 1003, 1004, 1005};
        for (unsigned n = 0; n < value.nibbles; ++n)
            expected.registers[value.destination][value.destination_first + n] =
                value.source == HPCLANG_RPL_CODE_CONSTANT ? 0
                : value.source == HPCLANG_RPL_CODE_NEXT_PC
                    ? pc_tokens[n]
                    : origins.registers[value.source][value.source_first + n];
        assert(hpclang_rpl_code_origins_assign(&origins, &value, pc_tokens) == 0);
        assert(!memcmp(&origins, &expected, sizeof origins));
        for (unsigned n = 0; n < image.size; ++n) {
            value = untouched;
            assert(hpclang_rpl_code_assignment_inspect(&reader, image.address, n, cases[i].p,
                                                       &value) == HPCLANG_RPL_SPAN_BOUNDS);
            assert(!memcmp(&value, &untouched, sizeof value));
            image.fail = image.address + n;
            assert(hpclang_rpl_code_assignment_inspect(&reader, image.address, image.size,
                                                       cases[i].p,
                                                       &value) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            assert(!memcmp(&value, &untouched, sizeof value));
            image.fail = UINT32_MAX;
            uint8_t saved = image.data[n];
            image.data[n] = 16;
            assert(hpclang_rpl_code_assignment_inspect(&reader, image.address, image.size,
                                                       cases[i].p,
                                                       &value) == HPCLANG_RPL_SPAN_MALFORMED);
            assert(!memcmp(&value, &untouched, sizeof value));
            image.data[n] = saved;
        }
        assert(hpclang_rpl_code_assignment_inspect(&reader, image.address, image.size, 16,
                                                   &value) == HPCLANG_RPL_SPAN_MALFORMED);
    }
    const char *unknown[] = {"12c", "dc",   "81af2c", "81a81c", "140", "fa",  "80b26",
                             "132", "133",  "136",    "137",    "13a", "13b", "13e",
                             "13f", "81b2", "81b3",   "81b6",   "81b7"};
    for (unsigned i = 0; i < sizeof unknown / sizeof unknown[0]; ++i) {
        struct image image = {
            .address = 0x80000, .size = (uint32_t)strlen(unknown[i]), .fail = UINT32_MAX};
        for (unsigned n = 0; n < image.size; ++n)
            image.data[n] = unknown[i][n] <= '9' ? unknown[i][n] - '0' : unknown[i][n] - 'a' + 10;
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        struct hpclang_rpl_code_assignment value, untouched;
        memset(&untouched, 0xa5, sizeof untouched);
        value = untouched;
        assert(hpclang_rpl_code_assignment_inspect(&reader, image.address, image.size, 0, &value) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!memcmp(&value, &untouched, sizeof value));
    }
}

static void register_exchanges(void)
{
    const struct
    {
        const char *code;
        unsigned p, left, right, first, width;
    } cases[] = {
        {"132", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D0, 0, 5},
        {"133", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D1, 0, 5},
        {"136", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D0, 0, 5},
        {"137", 15, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D1, 0, 5},
        {"13a", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D0, 0, 4},
        {"13b", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_D1, 0, 4},
        {"13e", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D0, 0, 4},
        {"13f", 15, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D1, 0, 4},
        {"dc", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_B, 0, 5},
        {"dd", 0, HPCLANG_RPL_CODE_B, HPCLANG_RPL_CODE_C, 0, 5},
        {"de", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_C, 0, 5},
        {"df", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_D, 0, 5},
        {"afe", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_C, 0, 16},
        {"a8e", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_C, 15, 1},
        {"a9e", 15, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_C, 0, 16},
        {"a9e", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_C, 0, 1},
        {"ace", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_C, 15, 1},
        {"ade", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_C, 3, 12},
        {"120", 0, HPCLANG_RPL_CODE_A, HPCLANG_RPL_CODE_R0, 0, 16},
        {"12c", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_R4, 0, 16},
        {"81af2c", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_R4, 0, 5},
        {"81a22c", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_R4, 2, 1},
        {"81a62c", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_R4, 0, 2},
        {"81a32c", 0, HPCLANG_RPL_CODE_C, HPCLANG_RPL_CODE_R4, 0, 3},
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        struct image image = {
            .address = 0x80000, .size = (uint32_t)strlen(cases[i].code), .fail = UINT32_MAX};
        for (unsigned n = 0; n < image.size; ++n)
            image.data[n] =
                cases[i].code[n] <= '9' ? cases[i].code[n] - '0' : cases[i].code[n] - 'a' + 10;
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        struct hpclang_rpl_code_exchange value, untouched;
        memset(&untouched, 0xa5, sizeof untouched);
        value = untouched;
        assert(!hpclang_rpl_code_exchange_inspect(&reader, image.address, image.size, cases[i].p,
                                                  &value));
        assert(value.instruction == image.address && value.length == image.size);
        assert((unsigned)value.left == cases[i].left && (unsigned)value.right == cases[i].right);
        assert(value.first == cases[i].first && value.nibbles == cases[i].width);
        struct hpclang_rpl_code_origins origins, expected;
        seed_origins(&origins);
        expected = origins;
        for (unsigned n = value.first; n < value.first + value.nibbles; ++n) {
            expected.registers[value.left][n] = origins.registers[value.right][n];
            expected.registers[value.right][n] = origins.registers[value.left][n];
        }
        assert(hpclang_rpl_code_origins_exchange(&origins, &value) == 0);
        assert(!memcmp(&origins, &expected, sizeof origins));
        for (unsigned n = 0; n < image.size; ++n) {
            value = untouched;
            assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, n, cases[i].p,
                                                     &value) == HPCLANG_RPL_SPAN_BOUNDS);
            assert(!memcmp(&value, &untouched, sizeof value));
            image.fail = image.address + n;
            assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, image.size, cases[i].p,
                                                     &value) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            assert(!memcmp(&value, &untouched, sizeof value));
            image.fail = UINT32_MAX;
            uint8_t saved = image.data[n];
            image.data[n] = 16;
            assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, image.size, cases[i].p,
                                                     &value) == HPCLANG_RPL_SPAN_MALFORMED);
            assert(!memcmp(&value, &untouched, sizeof value));
            image.data[n] = saved;
        }
        assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, image.size, 16, &value) ==
               HPCLANG_RPL_SPAN_MALFORMED);
    }
    const char *unknown[] = {"10c", "db",  "81af1c", "81a82c", "125",  "12f", "81af2d",
                             "140", "130", "131",    "134",    "135",  "138", "139",
                             "13c", "13d", "80ff",   "fa",     "80b26"};
    for (unsigned i = 0; i < sizeof unknown / sizeof unknown[0]; ++i) {
        struct image image = {
            .address = 0x80000, .size = (uint32_t)strlen(unknown[i]), .fail = UINT32_MAX};
        for (unsigned n = 0; n < image.size; ++n)
            image.data[n] = unknown[i][n] <= '9' ? unknown[i][n] - '0' : unknown[i][n] - 'a' + 10;
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        struct hpclang_rpl_code_exchange value, untouched;
        memset(&untouched, 0xa5, sizeof untouched);
        value = untouched;
        assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, image.size, 0, &value) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!memcmp(&value, &untouched, sizeof value));
    }
    struct image image = {.address = 0xffffe, .size = 2, .fail = UINT32_MAX, .data = {13, 14}};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_code_exchange value, untouched;
    memset(&untouched, 0xa5, sizeof untouched);
    assert(!hpclang_rpl_code_exchange_inspect(&reader, image.address, 2, 0, &value));
    value = untouched;
    assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, 3, 0, &value) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&value, &untouched, sizeof value));
    assert(hpclang_rpl_code_exchange_inspect(&reader, 0x100000, 2, 0, &value) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&value, &untouched, sizeof value));
    assert(hpclang_rpl_code_exchange_inspect(NULL, image.address, 2, 0, &value) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&value, &untouched, sizeof value));
    assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, 2, 0, NULL) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    reader.read_nibble = NULL;
    assert(hpclang_rpl_code_exchange_inspect(&reader, image.address, 2, 0, &value) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&value, &untouched, sizeof value));
}

static void seed_origins(struct hpclang_rpl_code_origins *state);

static void literal_origins(const struct hpclang_rpl_code_literal *literal, unsigned p)
{
    struct hpclang_rpl_code_origins initial, state, expected;
    seed_origins(&initial);
    /* Alias the complete source with the destination: full A/C loads at nonzero
     * P must rotate the original tokens, not repeatedly copy overwritten ones. */
    initial.registers[literal->destination][1] = 0;
    state = expected = initial;
    unsigned start = literal->destination <= HPCLANG_RPL_CODE_C ? p : 0;
    for (unsigned digit = 0; digit < 16; ++digit) {
        unsigned input = (digit + 16 - start) % 16;
        if (input < literal->nibbles)
            expected.registers[literal->destination][digit] =
                initial.registers[literal->destination][input];
    }
    assert(!hpclang_rpl_code_origins_literal(
        &state, literal, p, state.registers[literal->destination], literal->nibbles));
    assert(!memcmp(&state, &expected, sizeof state));
}

static void literal_origin_failures(void)
{
    struct hpclang_rpl_code_origins initial, state;
    seed_origins(&initial);
    const struct hpclang_rpl_code_literal valid = {
        .destination = HPCLANG_RPL_CODE_D0, .nibbles = 5, .mask = 0xfffff, .value = 0x8076b};
    for (unsigned mode = 0; mode < 9; ++mode) {
        struct hpclang_rpl_code_literal literal = valid;
        unsigned p = 0, count = 5;
        switch (mode) {
        case 0:
            literal.destination = HPCLANG_RPL_CODE_CONSTANT;
            break;
        case 1:
            literal.destination = (enum hpclang_rpl_code_register) - 1;
            break;
        case 2:
            count = 0;
            break;
        case 3:
            count = literal.nibbles = 17;
            break;
        case 4:
            count = literal.nibbles = 3;
            break;
        case 5:
            p = 16;
            break;
        case 6:
            literal.mask ^= 1;
            break;
        case 7:
            literal.value |= UINT64_C(1) << 20;
            break;
        case 8:
            literal.nibbles = 4;
            break;
        }
        state = initial;
        assert(hpclang_rpl_code_origins_literal(&state, &literal, p, state.registers[0], count) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&state, &initial, sizeof state));
    }
    state = initial;
    assert(hpclang_rpl_code_origins_literal(NULL, &valid, 0, state.registers[0], 5) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_code_origins_literal(&state, NULL, 0, state.registers[0], 5) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_code_origins_literal(&state, &valid, 0, NULL, 5) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&state, &initial, sizeof state));
}

static void literal_loads(void)
{
    struct hpclang_rpl_code_literal untouched;
    memset(&untouched, 0x5a, sizeof untouched);
    static const unsigned address_opcodes[] = {9, 10, 11, 13, 14, 15};
    static const unsigned address_widths[] = {2, 4, 5, 2, 4, 5};
    for (unsigned op = 0; op < 6; ++op)
        for (unsigned p = 0; p < 16; ++p) {
            unsigned width = address_widths[op];
            struct image image = {
                .address = 0x80000, .size = width + 2, .fail = UINT32_MAX, .data = {1}};
            image.data[1] = address_opcodes[op];
            uint64_t expected = 0;
            for (unsigned i = 0; i < width; ++i) {
                image.data[2 + i] = (i * 7 + 3) & 15;
                expected |= (uint64_t)image.data[2 + i] << (4 * i);
            }
            const struct hpclang_rpl_reader reader = {read_nibble, &image};
            struct hpclang_rpl_code_literal value = untouched;
            assert(
                !hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, p, &value));
            assert(value.destination == (op < 3 ? HPCLANG_RPL_CODE_D0 : HPCLANG_RPL_CODE_D1));
            assert(value.instruction == image.address && value.length == width + 2 &&
                   value.field == image.address + 2 && value.nibbles == width &&
                   value.value == expected && value.mask == ((UINT64_C(1) << (4 * width)) - 1));
            literal_origins(&value, p);
            for (unsigned i = 0; i < image.size; ++i) {
                value = untouched;
                assert(hpclang_rpl_code_literal_inspect(&reader, image.address, i, p, &value) ==
                       HPCLANG_RPL_SPAN_BOUNDS);
                assert(!memcmp(&value, &untouched, sizeof value));
                image.fail = image.address + i;
                assert(hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, p,
                                                        &value) == HPCLANG_RPL_SPAN_UNAVAILABLE);
                assert(!memcmp(&value, &untouched, sizeof value));
                image.fail = UINT32_MAX;
                uint8_t saved = image.data[i];
                image.data[i] = 16;
                assert(hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, p,
                                                        &value) == HPCLANG_RPL_SPAN_MALFORMED);
                assert(!memcmp(&value, &untouched, sizeof value));
                image.data[i] = saved;
            }
        }
    for (unsigned a = 0; a < 2; ++a)
        for (unsigned width = 1; width <= 16; ++width)
            for (unsigned p = 0; p < 16; ++p) {
                unsigned prefix = a ? 5 : 2;
                struct image image = {
                    .address = 0x80000, .size = prefix + width, .fail = UINT32_MAX};
                const uint8_t la[] = {8, 0, 8, 2};
                if (a)
                    memcpy(image.data, la, sizeof la);
                else
                    image.data[0] = 3;
                image.data[prefix - 1] = width - 1;
                for (unsigned i = 0; i < width; ++i)
                    image.data[prefix + i] = (i * 7 + 3) & 15;
                const struct hpclang_rpl_reader reader = {read_nibble, &image};
                struct hpclang_rpl_code_literal value = untouched;
                assert(!hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, p,
                                                         &value));
                assert(value.instruction == image.address && value.length == image.size &&
                       value.field == image.address + prefix && value.nibbles == width &&
                       value.destination == (a ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C));
                literal_origins(&value, p);
                for (unsigned nibble = 0; nibble < 16; ++nibble) {
                    unsigned source = (nibble + 16 - p) & 15;
                    assert(((value.mask >> (4 * nibble)) & 15) == (source < width ? 15u : 0u));
                    assert(((value.value >> (4 * nibble)) & 15) ==
                           (source < width ? image.data[prefix + source] : 0u));
                }
                for (unsigned i = 0; i < image.size; ++i) {
                    value = untouched;
                    assert(hpclang_rpl_code_literal_inspect(&reader, image.address, i, p, &value) ==
                           HPCLANG_RPL_SPAN_BOUNDS);
                    assert(!memcmp(&value, &untouched, sizeof value));
                    image.fail = image.address + i;
                    assert(hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, p,
                                                            &value) ==
                           HPCLANG_RPL_SPAN_UNAVAILABLE);
                    image.fail = UINT32_MAX;
                    assert(!memcmp(&value, &untouched, sizeof value));
                    uint8_t saved = image.data[i];
                    image.data[i] = 16;
                    assert(hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, p,
                                                            &value) == HPCLANG_RPL_SPAN_MALFORMED);
                    image.data[i] = saved;
                    assert(!memcmp(&value, &untouched, sizeof value));
                }
                value = untouched;
                assert(hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, 16,
                                                        &value) == HPCLANG_RPL_SPAN_MALFORMED);
                assert(!memcmp(&value, &untouched, sizeof value));
            }
    struct image image = {.address = 0x80000, .size = 2, .fail = UINT32_MAX, .data = {2, 0}};
    const struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_code_literal value = untouched;
    assert(hpclang_rpl_code_literal_inspect(&reader, image.address, image.size, 0, &value) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&value, &untouched, sizeof value));
}

static void seed_origins(struct hpclang_rpl_code_origins *state)
{
    memset(state, 0, sizeof *state);
    for (unsigned reg = 0; reg <= HPCLANG_RPL_CODE_D1; ++reg)
        for (unsigned n = 0; n < 16; ++n)
            state->registers[reg][n] = 0x100u + reg * 16u + n;
}

static void origin_aliases_and_failures(void)
{
    struct hpclang_rpl_code_origins before, state, expected;
    seed_origins(&before);
    struct hpclang_rpl_code_assignment assignment = {
        .source = HPCLANG_RPL_CODE_A,
        .destination = HPCLANG_RPL_CODE_A,
        .source_first = 0,
        .destination_first = 1,
        .nibbles = 15,
    };
    state = expected = before;
    for (unsigned n = 0; n < 15; ++n)
        expected.registers[0][n + 1] = before.registers[0][n];
    assert(hpclang_rpl_code_origins_assign(&state, &assignment, NULL) == 0);
    assert(!memcmp(&state, &expected, sizeof state));
    assignment.source = HPCLANG_RPL_CODE_CONSTANT;
    state = expected = before;
    for (unsigned n = 1; n < 16; ++n)
        expected.registers[0][n] = 0;
    assert(hpclang_rpl_code_origins_assign(&state, &assignment, NULL) == 0);
    assert(!memcmp(&state, &expected, sizeof state));
    assignment.source = HPCLANG_RPL_CODE_NEXT_PC;
    assignment.destination_first = 0;
    assignment.nibbles = 5;
    state = expected = before;
    assert(hpclang_rpl_code_origins_assign(&state, &assignment, NULL) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&state, &before, sizeof state));
    for (unsigned n = 0; n < 5; ++n)
        expected.registers[0][n] = before.registers[0][n + 1];
    assert(hpclang_rpl_code_origins_assign(&state, &assignment, state.registers[0] + 1) == 0);
    assert(!memcmp(&state, &expected, sizeof state));

    struct hpclang_rpl_code_exchange exchange = {
        .left = HPCLANG_RPL_CODE_A,
        .right = HPCLANG_RPL_CODE_D1,
        .first = 0,
        .nibbles = 4,
    };
    state = expected = before;
    for (unsigned n = 0; n < 4; ++n) {
        expected.registers[0][n] = before.registers[12][n];
        expected.registers[12][n] = before.registers[0][n];
    }
    assert(hpclang_rpl_code_origins_exchange(&state, &exchange) == 0);
    assert(!memcmp(&state, &expected, sizeof state));
    exchange.nibbles = 6;
    state = before;
    assert(hpclang_rpl_code_origins_exchange(&state, &exchange) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&state, &before, sizeof state));

    for (unsigned load = 0; load < 2; ++load) {
        struct hpclang_rpl_code_memory transfer = {
            .data = HPCLANG_RPL_CODE_A,
            .address = HPCLANG_RPL_CODE_D1,
            .first = load,
            .nibbles = 15,
            .load = load,
        };
        state = expected = before;
        for (unsigned n = 0; n < 15; ++n)
            expected.registers[0][n + 1] = before.registers[0][n];
        assert(hpclang_rpl_code_origins_memory(&state, &transfer, state.registers[0] + !load, 15) ==
               0);
        assert(!memcmp(&state, &expected, sizeof state));
    }
    for (unsigned fault = 0; fault < 9; ++fault) {
        struct hpclang_rpl_code_memory transfer = {
            .data = HPCLANG_RPL_CODE_A,
            .address = HPCLANG_RPL_CODE_D1,
            .nibbles = 5,
        };
        unsigned count = 5;
        switch (fault) {
        case 0:
            transfer.data = HPCLANG_RPL_CODE_R0;
            break;
        case 1:
            transfer.address = HPCLANG_RPL_CODE_A;
            break;
        case 2:
            transfer.load = 2;
            break;
        case 3:
            transfer.first = 16;
            break;
        case 4:
            transfer.nibbles = count = 0;
            break;
        case 5:
            transfer.nibbles = count = 17;
            break;
        case 6:
            transfer.first = UINT32_MAX;
            break;
        case 7:
            count = 4;
            break;
        default:
            transfer.data = (enum hpclang_rpl_code_register) - 1;
            break;
        }
        state = before;
        assert(hpclang_rpl_code_origins_memory(&state, &transfer, state.registers[0], count) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&state, &before, sizeof state));
    }
    for (unsigned fault = 0; fault < 7; ++fault) {
        assignment = (struct hpclang_rpl_code_assignment){
            .source = HPCLANG_RPL_CODE_R0,
            .destination = HPCLANG_RPL_CODE_A,
            .nibbles = 5,
        };
        switch (fault) {
        case 0:
            assignment.source = (enum hpclang_rpl_code_register) - 1;
            break;
        case 1:
            assignment.destination = HPCLANG_RPL_CODE_CONSTANT;
            break;
        case 2:
            assignment.nibbles = 0;
            break;
        case 3:
            assignment.nibbles = 17;
            break;
        case 4:
            assignment.source_first = 12;
            break;
        case 5:
            assignment.destination_first = UINT32_MAX;
            break;
        default:
            assignment.source = HPCLANG_RPL_CODE_CONSTANT;
            assignment.source_first = 1;
            break;
        }
        state = before;
        assert(hpclang_rpl_code_origins_assign(&state, &assignment, NULL) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&state, &before, sizeof state));
    }
}

static void memory_transfers(void)
{
    const unsigned fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 15};
    const unsigned starts[] = {0, 0, 2, 0, 15, 3, 0, 0, 0};
    const unsigned widths[] = {1, 1, 1, 3, 1, 12, 2, 16, 5};
    for (unsigned form = 0; form < 3; ++form)
        for (unsigned selector = 0; selector < (form ? 8u : 16u); ++selector)
            for (unsigned field = 0; field < (form == 0 ? 1u : form == 1 ? 9u : 16u); ++field)
                for (unsigned p = 0; p < 16; ++p) {
                    struct image image = {{1, form ? 5 : 4, selector + (form == 2 ? 8 : 0),
                                           form == 1 ? fields[field] : field},
                                          0x80001,
                                          form ? 4 : 3,
                                          UINT32_MAX};
                    struct hpclang_rpl_reader reader = {read_nibble, &image};
                    struct hpclang_rpl_code_memory value, unchanged;
                    memset(&unchanged, 0xa5, sizeof unchanged);
                    value = unchanged;
                    assert(hpclang_rpl_code_memory_inspect(&reader, image.address, image.size, p,
                                                           &value) == 0);
                    assert(value.instruction == image.address && value.length == image.size);
                    assert(value.data ==
                           ((selector & 4) ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A));
                    assert(value.address ==
                           ((selector & 1) ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0));
                    assert(value.load == ((selector & 2) != 0));
                    unsigned first = form == 1 ? (field == 0 ? p : starts[field]) : 0;
                    unsigned nibbles = form == 0    ? (selector < 8 ? 5 : 2)
                                       : form == 2  ? field + 1
                                       : field == 1 ? p + 1
                                                    : widths[field];
                    assert(value.first == first && value.nibbles == nibbles);
                    struct hpclang_rpl_code_origins origins, expected;
                    seed_origins(&origins);
                    expected = origins;
                    uint32_t tags[16], expected_tags[16];
                    for (unsigned n = 0; n < 16; ++n)
                        tags[n] = expected_tags[n] = 0x800u + n;
                    for (unsigned n = 0; n < value.nibbles; ++n)
                        if (value.load)
                            expected.registers[value.data][value.first + n] = tags[n];
                        else
                            expected_tags[n] = origins.registers[value.data][value.first + n];
                    assert(hpclang_rpl_code_origins_memory(&origins, &value, tags, value.nibbles) ==
                           0);
                    assert(!memcmp(&origins, &expected, sizeof origins));
                    assert(!memcmp(tags, expected_tags, sizeof tags));
                    for (unsigned n = 0; n < image.size; ++n) {
                        value = unchanged;
                        assert(hpclang_rpl_code_memory_inspect(&reader, image.address, n, p,
                                                               &value) == HPCLANG_RPL_SPAN_BOUNDS);
                        assert(!memcmp(&value, &unchanged, sizeof value));
                        image.fail = image.address + n;
                        assert(hpclang_rpl_code_memory_inspect(&reader, image.address, image.size,
                                                               p, &value) ==
                               HPCLANG_RPL_SPAN_UNAVAILABLE);
                        assert(!memcmp(&value, &unchanged, sizeof value));
                        image.fail = UINT32_MAX;
                    }
                }
    for (unsigned field = 8; field < 15; ++field) {
        struct image image = {{1, 5, 0, field}, 0x80000, 4, UINT32_MAX};
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        struct hpclang_rpl_code_memory value, unchanged;
        memset(&unchanged, 0xa5, sizeof unchanged);
        value = unchanged;
        assert(hpclang_rpl_code_memory_inspect(&reader, image.address, 4, 0, &value) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!memcmp(&value, &unchanged, sizeof value));
        assert(hpclang_rpl_code_memory_inspect(&reader, image.address, 4, 16, &value) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&value, &unchanged, sizeof value));
        image.data[3] = 16;
        assert(hpclang_rpl_code_memory_inspect(&reader, image.address, 4, 0, &value) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&value, &unchanged, sizeof value));
    }
}

static void pc_exchanges(void)
{
    struct image image = {{0}, 0x90001, 4, UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_code_pc_exchange unchanged, value;
    memset(&unchanged, 0xa5, sizeof unchanged);
    for (unsigned opcode = 0; opcode < 0x10000; ++opcode) {
        encode(&image, 0, 4, opcode);
        value = unchanged;
        int result = hpclang_rpl_code_pc_exchange_inspect(&reader, image.address, 4, 0xabc12345,
                                                          0xdef67890, &value);
        if (opcode != 0x6b18 && opcode != 0x7b18) {
            assert(result == HPCLANG_RPL_SPAN_UNSUPPORTED);
            assert(!memcmp(&value, &unchanged, sizeof value));
            continue;
        }
        unsigned destination = opcode == 0x6b18 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C;
        assert(!result && value.target == (opcode == 0x6b18 ? 0x12345u : 0x67890u));
        assert(value.capture.instruction == image.address && value.capture.length == 4);
        assert(value.capture.destination == destination && value.capture.nibbles == 5);
        assert(value.capture.source == HPCLANG_RPL_CODE_NEXT_PC);
        assert(!value.capture.source_first && !value.capture.destination_first);
        assert(value.capture.constant == image.address + 4);
        struct hpclang_rpl_code_origins origins, before;
        seed_origins(&origins);
        before = origins;
        const uint32_t pc[5] = {901, 902, 903, 904, 905};
        assert(!hpclang_rpl_code_origins_assign(&origins, &value.capture, pc));
        memcpy(before.registers[destination], pc, sizeof pc);
        assert(!memcmp(&origins, &before, sizeof origins));
        for (unsigned fault = 0; fault < 4; ++fault) {
            image.fail = image.address + fault;
            value = unchanged;
            assert(hpclang_rpl_code_pc_exchange_inspect(&reader, image.address, 4, 0, 0, &value) ==
                   HPCLANG_RPL_SPAN_UNAVAILABLE);
            assert(!memcmp(&value, &unchanged, sizeof value));
        }
        image.fail = UINT32_MAX;
    }
    encode(&image, 0, 4, 0x6b18);
    for (unsigned limit = 0; limit < 4; ++limit) {
        value = unchanged;
        assert(hpclang_rpl_code_pc_exchange_inspect(&reader, image.address, limit, 0, 0, &value) ==
               HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&value, &unchanged, sizeof value));
    }
    image.address = 0xffffb;
    assert(!hpclang_rpl_code_pc_exchange_inspect(&reader, image.address, 4, 0, 0, &value));
    assert(value.capture.constant == 0xfffff);
    for (image.address = 0xffffc; image.address <= 0x100000; ++image.address) {
        value = unchanged;
        assert(hpclang_rpl_code_pc_exchange_inspect(&reader, image.address, 4, 0, 0, &value) ==
               HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&value, &unchanged, sizeof value));
    }
}

static void pc_capture_bounds(void)
{
    for (unsigned destination = 4; destination <= 5; ++destination) {
        struct image image = {{8, 1, 11, destination}, 0xffffb, 4, UINT32_MAX};
        struct hpclang_rpl_reader reader = {read_nibble, &image};
        struct hpclang_rpl_code_assignment value, unchanged;
        memset(&unchanged, 0xa5, sizeof unchanged);
        value = unchanged;
        assert(hpclang_rpl_code_assignment_inspect(&reader, image.address, 4, 0, &value) == 0);
        assert(value.source == HPCLANG_RPL_CODE_NEXT_PC && value.constant == 0xfffff);
        for (image.address = 0xffffc; image.address < 0x100000; ++image.address) {
            value = unchanged;
            assert(hpclang_rpl_code_assignment_inspect(&reader, image.address,
                                                       0x100000 - image.address, 0,
                                                       &value) == HPCLANG_RPL_SPAN_BOUNDS);
            assert(!memcmp(&value, &unchanged, sizeof value));
        }
    }
}

static void address_adjustments(void)
{
    const unsigned opcodes[] = {6, 7, 8, 12};
    const struct hpclang_rpl_code_address_adjust unchanged = {123, 456, HPCLANG_RPL_CODE_P, 789};
    struct image image = {.address = 0x90001, .size = 3, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    for (unsigned op = 0; op < 4; ++op) {
        for (unsigned immediate = 0; immediate < 16; ++immediate) {
            image.data[0] = 1;
            image.data[1] = opcodes[op];
            image.data[2] = immediate;
            struct hpclang_rpl_code_address_adjust actual = unchanged;
            assert(hpclang_rpl_code_address_adjust_inspect(&reader, image.address, 3, &actual) ==
                   0);
            assert(actual.instruction == image.address && actual.length == 3);
            assert(actual.address == (op % 2 ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0));
            assert(actual.delta == (int32_t)(immediate + 1) * (op < 2 ? 1 : -1));
            for (unsigned byte = 0; byte < 3; ++byte) {
                actual = unchanged;
                assert(hpclang_rpl_code_address_adjust_inspect(&reader, image.address, byte,
                                                               &actual) == HPCLANG_RPL_SPAN_BOUNDS);
                assert(!memcmp(&actual, &unchanged, sizeof actual));
                image.fail = image.address + byte;
                assert(hpclang_rpl_code_address_adjust_inspect(
                           &reader, image.address, 3, &actual) == HPCLANG_RPL_SPAN_UNAVAILABLE);
                assert(!memcmp(&actual, &unchanged, sizeof actual));
                image.fail = UINT32_MAX;
                uint8_t saved = image.data[byte];
                image.data[byte] = 16;
                assert(hpclang_rpl_code_address_adjust_inspect(
                           &reader, image.address, 3, &actual) == HPCLANG_RPL_SPAN_MALFORMED);
                assert(!memcmp(&actual, &unchanged, sizeof actual));
                image.data[byte] = saved;
            }
        }
    }
    for (unsigned opcode = 0; opcode < 16; ++opcode) {
        if (opcode == 6 || opcode == 7 || opcode == 8 || opcode == 12)
            continue;
        image.data[1] = opcode;
        struct hpclang_rpl_code_address_adjust actual = unchanged;
        assert(hpclang_rpl_code_address_adjust_inspect(&reader, image.address, 3, &actual) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!memcmp(&actual, &unchanged, sizeof actual));
    }
    image.data[1] = 6;
    image.address = 0xFFFFD;
    struct hpclang_rpl_code_address_adjust actual = unchanged;
    assert(hpclang_rpl_code_address_adjust_inspect(&reader, image.address, 3, &actual) == 0);
    actual = unchanged;
    assert(hpclang_rpl_code_address_adjust_inspect(&reader, 0xFFFFE, 3, &actual) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_code_address_adjust_inspect(&reader, 0x100000, 1, &actual) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_code_address_adjust_inspect(NULL, image.address, 3, &actual) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_code_address_adjust_inspect(&reader, image.address, 3, NULL) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&actual, &unchanged, sizeof actual));
}

static void address_origin_invalidation(void)
{
    struct hpclang_rpl_code_origins initial;
    memset(&initial, 0x5a, sizeof initial);
    for (int reg = -1; reg <= HPCLANG_RPL_CODE_D1 + 1; ++reg) {
        for (int delta = -17; delta <= 17; ++delta) {
            struct hpclang_rpl_code_origins state = initial, expected = initial;
            struct hpclang_rpl_code_address_adjust adjust = {
                0x90000, 3, (enum hpclang_rpl_code_register)reg, delta};
            int valid = (reg == HPCLANG_RPL_CODE_D0 || reg == HPCLANG_RPL_CODE_D1) && delta &&
                        delta >= -16 && delta <= 16;
            if (valid) {
                memset(expected.registers[reg], 0, 5 * sizeof(uint32_t));
                expected.adjustment[reg] = 0;
            }
            assert(hpclang_rpl_code_origins_adjust(&state, &adjust) ==
                   (valid ? 0 : HPCLANG_RPL_SPAN_MALFORMED));
            assert(!memcmp(&state, &expected, sizeof state));
        }
    }
    const int32_t extremes[] = {INT32_MIN, INT32_MAX};
    for (unsigned i = 0; i < 2; ++i) {
        struct hpclang_rpl_code_address_adjust adjust = {0, 3, HPCLANG_RPL_CODE_D0, extremes[i]};
        struct hpclang_rpl_code_origins state = initial;
        assert(hpclang_rpl_code_origins_adjust(&state, &adjust) == HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&state, &initial, sizeof state));
        assert(hpclang_rpl_code_origins_adjust(NULL, &adjust) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    }
    assert(hpclang_rpl_code_origins_adjust(&initial, NULL) == HPCLANG_RPL_SPAN_UNAVAILABLE);
}

struct bank_entry_observations
{
    unsigned count, stop;
    uint8_t seen[128];
};

static int bank_entry_observe(void *context, uint32_t address, uint8_t value)
{
    struct bank_entry_observations *state = context;
    assert(address >= 0x80000 && address < 0x80080 && value <= 15);
    state->seen[address - 0x80000] = 1;
    return ++state->count == state->stop ? -79 : 0;
}

static void bank_entry_program(void)
{
    struct image image = {.address = 0x80000, .size = 61, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &image};
    image.data[0] = 1;
    image.data[1] = 11;
    encode(&image, 2, 5, 0x12345);
    image.data[7] = 6;
    encode(&image, 8, 3, 8);
    image.data[16] = 8;
    image.data[17] = 9;
    image.data[18] = 0;
    encode(&image, 19, 2, 13);
    const uint8_t body[] = {2, 0, 3, 4, 0, 0, 0, 15, 15, 8, 0, 5, 1, 4, 6,
                            1, 3, 4, 8, 0, 5, 1, 4,  12, 8, 0, 4, 0, 3};
    memcpy(image.data + 32, body, sizeof body);
    struct hpclang_rpl_code_bank_entry output, sentinel;
    memset(&sentinel, 0x5a, sizeof sentinel);
    struct bank_entry_observations observations = {0};
    assert(!hpclang_rpl_code_bank_entry_inspect(&reader, image.address, image.address + image.size,
                                                image.address, 0, &output, bank_entry_observe,
                                                &observations));
    assert(output.selector == 0x12345 && output.dispatch == image.address + 16 &&
           output.body == image.address + 32);
    unsigned count = observations.count;
    for (unsigned stop = 1; stop <= count; ++stop) {
        output = sentinel;
        observations = (struct bank_entry_observations){.stop = stop};
        assert(hpclang_rpl_code_bank_entry_inspect(
                   &reader, image.address, image.address + image.size, image.address, 0, &output,
                   bank_entry_observe, &observations) == -79);
        assert(!memcmp(&output, &sentinel, sizeof output));
    }
    for (unsigned at = 0; at < image.size; ++at) {
        if ((at >= 11 && at < 16) || (at >= 21 && at < 32))
            continue;
        observations = (struct bank_entry_observations){0};
        image.fail = image.address + at;
        assert(hpclang_rpl_code_bank_entry_inspect(
                   &reader, image.address, image.address + image.size, image.address, 0, &output,
                   bank_entry_observe, &observations) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&output, &sentinel, sizeof output));
        image.fail = UINT32_MAX;
        if (at < 32)
            continue;
        image.data[at] ^= 1;
        assert(hpclang_rpl_code_bank_entry_inspect(
                   &reader, image.address, image.address + image.size, image.address, 0, &output,
                   bank_entry_observe, &observations) == HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!memcmp(&output, &sentinel, sizeof output));
        image.data[at] ^= 1;
    }
    for (unsigned p = 2; p <= 16; ++p) {
        observations = (struct bank_entry_observations){0};
        assert(hpclang_rpl_code_bank_entry_inspect(&reader, image.address,
                                                   image.address + image.size, image.address, p,
                                                   &output, bank_entry_observe, &observations) ==
               (p == 16 ? HPCLANG_RPL_SPAN_MALFORMED : HPCLANG_RPL_SPAN_UNSUPPORTED));
        assert(!observations.count && !memcmp(&output, &sentinel, sizeof output));
    }
    assert(hpclang_rpl_code_bank_entry_inspect(
               &reader, image.address, image.address + image.size - 1, image.address, 0, &output,
               bank_entry_observe, &observations) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&output, &sentinel, sizeof output));
}

static void bank_exit_program(void)
{
    struct image image = {.address = 0x80000, .size = 107, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &image};
    encode(&image, 0, 2, 0xb1);
    encode(&image, 2, 5, 0x12345);
    encode(&image, 7, 4, 0x86);
    const unsigned order[] = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1};
    for (unsigned i = 0; i < sizeof order / sizeof *order; ++i) {
        unsigned at = 16 + i * 5;
        encode(&image, at, 3, 0x98 | (order[i] << 8));
        encode(&image, at + 3, 2, (order[i] == 1 ? 71 : 78) - (at + 3));
    }
    encode(&image, 71, 2, 0xb1);
    encode(&image, 73, 5, 0x54321);
    const uint8_t body[] = {2, 0, 3, 4, 0, 0, 0, 15, 15, 8, 0, 5, 1, 4, 6,
                            1, 3, 4, 8, 0, 5, 1, 4,  12, 8, 0, 4, 0, 3};
    memcpy(image.data + 78, body, sizeof body);
    struct hpclang_rpl_code_bank_entry output, sentinel;
    memset(&sentinel, 0x5a, sizeof sentinel);
    struct bank_entry_observations failures = {0};
    for (unsigned p = 0; p <= 1; ++p) {
        struct bank_entry_observations observations = {0};
        assert(!hpclang_rpl_code_bank_entry_inspect(&reader, image.address,
                                                    image.address + image.size, image.address, p,
                                                    &output, bank_entry_observe, &observations));
        assert(output.selector == (p ? 0x54321u : 0x12345u));
        assert(output.dispatch == image.address + 16 && output.body == image.address + 78);
        struct bank_entry_observations consumed = observations;
        for (unsigned stop = 1; stop <= consumed.count; ++stop) {
            output = sentinel;
            observations = (struct bank_entry_observations){.stop = stop};
            assert(hpclang_rpl_code_bank_entry_inspect(
                       &reader, image.address, image.address + image.size, image.address, p,
                       &output, bank_entry_observe, &observations) == -79);
            assert(!memcmp(&output, &sentinel, sizeof output));
        }
        for (unsigned at = 0; at < image.size; ++at) {
            if (!consumed.seen[at])
                continue;
            image.fail = image.address + at;
            assert(hpclang_rpl_code_bank_entry_inspect(
                       &reader, image.address, image.address + image.size, image.address, p,
                       &output, bank_entry_observe, &failures) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            assert(!memcmp(&output, &sentinel, sizeof output));
        }
        image.fail = UINT32_MAX;
    }
    for (unsigned end = 71; end < image.size; ++end) {
        assert(hpclang_rpl_code_bank_entry_inspect(&reader, image.address, image.address + end,
                                                   image.address, 1, &output, bank_entry_observe,
                                                   &failures) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&output, &sentinel, sizeof output));
    }
    for (unsigned at = 78; at < image.size; ++at) {
        image.data[at] ^= 1;
        assert(hpclang_rpl_code_bank_entry_inspect(
                   &reader, image.address, image.address + image.size, image.address, 1, &output,
                   bank_entry_observe, &failures) == HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!memcmp(&output, &sentinel, sizeof output));
        image.data[at] ^= 1;
    }
    /* No matching P=1 test must not reinterpret the following D0 literal. */
    image.data[68] = 2;
    assert(hpclang_rpl_code_bank_entry_inspect(&reader, image.address, image.address + image.size,
                                               image.address, 1, &output, bank_entry_observe,
                                               &failures) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&output, &sentinel, sizeof output));
}

struct origin_plan_fixture
{
    struct hpclang_rpl_code_origin_cell cells[5];
    unsigned fail_token;
    int error;
};

static int origin_plan_resolve(void *context, uint32_t token,
                               struct hpclang_rpl_code_origin_cell *cell)
{
    struct origin_plan_fixture *fixture = context;
    if (token == fixture->fail_token)
        return fixture->error;
    if (!token || token > 5)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    *cell = fixture->cells[token - 1];
    return 0;
}

static void origin_repair_plans(void)
{
    struct origin_plan_fixture fixture = {0};
    uint32_t tokens[] = {1, 2, 3, 4, 5};
    struct hpclang_rpl_reference_edit edits[5];
    unsigned count;
    for (unsigned i = 0; i < 5; ++i)
        fixture.cells[i] = (struct hpclang_rpl_code_origin_cell){7, 0x80000 + i, i + 1};
    assert(!hpclang_rpl_code_origins_plan(tokens, 0x54321, 0x54331, origin_plan_resolve, &fixture,
                                          edits, &count));
    assert(count == 1 && edits[0].mapping == 7 && edits[0].field == 0x80000 &&
           edits[0].width == 5 && edits[0].expected == 0x54321 && edits[0].value == 0x54331);
    /* Every permutation must repair the sources, not the register digit order. */
    unsigned permutations = 0;
    for (unsigned a = 1; a <= 5; ++a)
        for (unsigned b = 1; b <= 5; ++b)
            for (unsigned c = 1; c <= 5; ++c)
                for (unsigned d = 1; d <= 5; ++d)
                    for (unsigned e = 1; e <= 5; ++e) {
                        uint32_t order[] = {a, b, c, d, e}, seen = 0, current = 0;
                        for (unsigned i = 0; i < 5; ++i) {
                            seen |= 1u << order[i];
                            current |= order[i] << (4 * i);
                        }
                        if (seen != 62)
                            continue;
                        assert(!hpclang_rpl_code_origins_plan(
                            order, current, 0xabcde, origin_plan_resolve, &fixture, edits, &count));
                        assert(count == 1 && edits[0].expected == 0x54321 && edits[0].width == 5);
                        uint32_t reconstructed = 0;
                        for (unsigned i = 0; i < 5; ++i)
                            reconstructed |= ((edits[0].value >> (4 * (order[i] - 1))) & 15u)
                                             << (4 * i);
                        assert(reconstructed == 0xabcde);
                        ++permutations;
                    }
    assert(permutations == 120);
    /* Dispersed mappings must not be combined just because values are related. */
    for (unsigned i = 0; i < 5; ++i)
        fixture.cells[i].mapping = i % 2;
    assert(!hpclang_rpl_code_origins_plan(tokens, 0x54321, 0xabcde, origin_plan_resolve, &fixture,
                                          edits, &count));
    assert(count == 5);
    for (unsigned i = 0; i < 5; ++i)
        assert(edits[i].width == 1 && edits[i].mapping == (edits[i].field - 0x80000) % 2);
    for (unsigned i = 0; i < 5; ++i)
        tokens[i] = 1;
    assert(!hpclang_rpl_code_origins_plan(tokens, 0x11111, 0x22222, origin_plan_resolve, &fixture,
                                          edits, &count));
    assert(count == 1 && edits[0].width == 1 && edits[0].value == 2);
    struct hpclang_rpl_reference_edit sentinel[5];
    memset(sentinel, 0x5a, sizeof sentinel);
    for (unsigned mode = 0; mode < 7; ++mode) {
        uint32_t replacement = 0x22222;
        int expected = HPCLANG_RPL_SPAN_MALFORMED;
        fixture.fail_token = 0;
        fixture.cells[0].expected = 1;
        fixture.cells[0].field = 0x80000;
        tokens[4] = 1;
        if (mode == 0)
            replacement = 0x22223; /* One original cell cannot hold both digits. */
        else if (mode == 1)
            fixture.cells[0].expected = 16;
        else if (mode == 2) {
            tokens[4] = 0;
            expected = HPCLANG_RPL_SPAN_UNSUPPORTED;
        } else if (mode == 3) {
            fixture.cells[0].field = 0x100000;
            expected = HPCLANG_RPL_SPAN_BOUNDS;
        } else if (mode == 4 || mode == 5) {
            fixture.fail_token = 1;
            fixture.error = mode == 4 ? -79 : 1;
            expected = mode == 4 ? -79 : HPCLANG_RPL_SPAN_MALFORMED;
        } else {
            replacement = 0x100000;
            expected = HPCLANG_RPL_SPAN_BOUNDS;
        }
        memcpy(edits, sentinel, sizeof edits);
        count = 99;
        assert(hpclang_rpl_code_origins_plan(tokens, 0x11111, replacement, origin_plan_resolve,
                                             &fixture, edits, &count) == expected);
        assert(count == 99 && !memcmp(edits, sentinel, sizeof edits));
    }
}

static void native_threaded_exit(void)
{
    struct image code = {.size = 4, .fail = UINT32_MAX};
    struct image memory = {.address = 0x100, .size = 128, .fail = UINT32_MAX};
    encode(&code, 0, 4, 0xdb18);
    encode(&memory, 16, 5, 0x120);
    encode(&memory, 32, 5, 0x130);
    encode(&memory, 48, 5, 0x60);
    const struct hpclang_rpl_reader instructions = {read_nibble, &code};
    const struct hpclang_rpl_reader data = {read_nibble, &memory};
    struct hpclang_rpl_code_execution initial = {0}, state;
    seed_origins(&initial.origins);
    initial.values.registers[HPCLANG_RPL_CODE_A] = UINT64_C(0xfedcba9876543210);
    initial.values.registers[HPCLANG_RPL_CODE_B] = UINT64_C(0x1234567800000115);
    initial.values.registers[HPCLANG_RPL_CODE_D] = UINT64_C(0x87654321ffffffff);
    initial.values.carry = initial.control.carry = 1;
    initial.values.decimal = 1; /* 81BD arithmetic is native binary arithmetic. */
    initial.control.cursor = 3;
    initial.control.stack[2] = 0xabcde;
    struct hpclang_rpl_code_step_uses uses, sentinel;
    memset(&sentinel, 0xa5, sizeof sentinel);
    state = initial;
    assert(
        !hpclang_rpl_code_execution_step(&instructions, &data, 4, &state, data_token, NULL, &uses));
    assert(uses.count == 4 && !uses.has_branch && state.control.pc == 0x60);
    assert(state.values.registers[HPCLANG_RPL_CODE_A] == UINT64_C(0xfedcba9876500130));
    assert(state.values.registers[HPCLANG_RPL_CODE_B] == UINT64_C(0x1234567800000110));
    assert(state.values.registers[HPCLANG_RPL_CODE_D] == UINT64_C(0x8765432100000000));
    assert(state.values.registers[HPCLANG_RPL_CODE_D0] == 0x125);
    assert(state.values.carry == 1 && state.values.decimal == 1 && state.control.carry == 1);
    assert(state.control.cursor == 3 && state.control.stack[2] == 0xabcde);
    for (unsigned use = 0; use < 4; ++use) {
        assert(uses.addresses[use].kind ==
               (use < 3 ? HPCLANG_RPL_CODE_USE_LOAD_ADDRESS : HPCLANG_RPL_CODE_USE_MEMORY_TARGET));
        assert(uses.addresses[use].value == (use < 3 ? 0x110 + 16 * use : 0x60));
        for (unsigned digit = 0; digit < 5; ++digit)
            assert(uses.addresses[use].origins[digit] ==
                   (use ? 0x200110 + 16 * (use - 1) + digit
                        : initial.origins.registers[HPCLANG_RPL_CODE_B][digit]));
        assert(uses.addresses[use].adjustment == (use ? 0 : 0xffffb));
    }
    for (unsigned reg = 0; reg < 13; ++reg)
        for (unsigned digit = 0; digit < 16; ++digit) {
            uint32_t expected = initial.origins.registers[reg][digit];
            if ((reg == HPCLANG_RPL_CODE_B && digit >= 5 && digit < 8) ||
                (reg == HPCLANG_RPL_CODE_D && digit < 8))
                expected = 0;
            if (reg == HPCLANG_RPL_CODE_D0 && digit < 5)
                expected = 0;
            if (reg == HPCLANG_RPL_CODE_A && digit < 5)
                expected = 0x200120 + digit;
            assert(state.origins.registers[reg][digit] == expected);
        }
    state.control.pc = 0;
    encode(&memory, 11, 5, 0x120);
    assert(
        !hpclang_rpl_code_execution_step(&instructions, &data, 4, &state, data_token, NULL, &uses));
    assert(uses.addresses[0].value == 0x10b && uses.addresses[0].adjustment == 0xffff6);
    assert(state.origins.adjustment[HPCLANG_RPL_CODE_B] == 0xffff6);
    assert(!memcmp(uses.addresses[0].origins, initial.origins.registers[HPCLANG_RPL_CODE_B],
                   sizeof uses.addresses[0].origins));
    /* Every dependent data digit: unavailable, malformed, and token rejection
     * must preserve both outputs even after earlier loads succeeded. */
    for (unsigned mode = 0; mode < 3; ++mode)
        for (unsigned load = 0; load < 3; ++load)
            for (unsigned digit = 0; digit < 5; ++digit) {
                unsigned offset = 16 + load * 16 + digit;
                uint8_t original = memory.data[offset];
                uint32_t rejected = memory.address + offset;
                memory.fail = mode == 0 ? rejected : UINT32_MAX;
                if (mode == 1)
                    memory.data[offset] = 16;
                state = initial;
                uses = sentinel;
                assert(hpclang_rpl_code_execution_step(&instructions, &data, 4, &state, data_token,
                                                       mode == 2 ? &rejected : NULL, &uses) ==
                       (mode == 1 ? HPCLANG_RPL_SPAN_MALFORMED : HPCLANG_RPL_SPAN_UNAVAILABLE));
                assert(!memcmp(&state, &initial, sizeof state));
                assert(!memcmp(&uses, &sentinel, sizeof uses));
                memory.data[offset] = original;
            }
    memory.fail = UINT32_MAX;
    for (unsigned digit = 0; digit < 4; ++digit) {
        state = initial;
        uses = sentinel;
        code.fail = digit;
        assert(hpclang_rpl_code_execution_step(&instructions, &data, 4, &state, data_token, NULL,
                                               &uses) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&state, &initial, sizeof state));
        assert(!memcmp(&uses, &sentinel, sizeof uses));
    }
    code.fail = UINT32_MAX;
    for (unsigned mode = 0; mode < 4; ++mode) {
        state = initial;
        if (mode == 0)
            state.values.registers[HPCLANG_RPL_CODE_B] = 1; /* First read crosses 20 bits. */
        if (mode == 1)
            encode(&memory, 16, 5, 0xffffb); /* D0+5 cannot be captured canonically. */
        if (mode == 2)
            encode(&memory, 32, 5, 0xffffc); /* Third read crosses 20 bits. */
        struct hpclang_rpl_code_execution saved = state;
        uses = sentinel;
        assert(hpclang_rpl_code_execution_step(&instructions, &data, mode == 3 ? 3 : 4, &state,
                                               data_token, NULL, &uses) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&state, &saved, sizeof state));
        assert(!memcmp(&uses, &sentinel, sizeof uses));
        encode(&memory, 16, 5, 0x120);
        encode(&memory, 32, 5, 0x130);
    }
}

static void computed_origin_transfers(void)
{
    struct hpclang_rpl_code_origins origins;
    seed_origins(&origins);
    origins.adjustment[HPCLANG_RPL_CODE_B] = 0xffffb;
    struct hpclang_rpl_code_assignment copy = {
        .source = HPCLANG_RPL_CODE_B, .destination = HPCLANG_RPL_CODE_C, .nibbles = 5};
    assert(!hpclang_rpl_code_origins_assign(&origins, &copy, NULL));
    assert(origins.adjustment[HPCLANG_RPL_CODE_C] == 0xffffb);
    assert(!memcmp(origins.registers[HPCLANG_RPL_CODE_B], origins.registers[HPCLANG_RPL_CODE_C],
                   5 * sizeof(uint32_t)));
    copy.source = HPCLANG_RPL_CODE_C;
    copy.destination = HPCLANG_RPL_CODE_A;
    copy.source_first = 2;
    copy.nibbles = 1;
    assert(!hpclang_rpl_code_origins_assign(&origins, &copy, NULL));
    assert(!origins.registers[HPCLANG_RPL_CODE_A][0]);
    copy.source = HPCLANG_RPL_CODE_CONSTANT;
    copy.source_first = 0;
    copy.destination = HPCLANG_RPL_CODE_C;
    copy.destination_first = 6;
    assert(!hpclang_rpl_code_origins_assign(&origins, &copy, NULL));
    assert(origins.adjustment[HPCLANG_RPL_CODE_C] ==
           0xffffb); /* High write preserves low formula. */
    copy.destination_first = 2;
    assert(!hpclang_rpl_code_origins_assign(&origins, &copy, NULL));
    assert(!origins.adjustment[HPCLANG_RPL_CODE_C]);
    for (unsigned digit = 0; digit < 5; ++digit)
        assert(!origins.registers[HPCLANG_RPL_CODE_C][digit]);
    struct hpclang_rpl_code_exchange exchange = {
        .left = HPCLANG_RPL_CODE_B, .right = HPCLANG_RPL_CODE_A, .nibbles = 5};
    assert(!hpclang_rpl_code_origins_exchange(&origins, &exchange));
    assert(origins.adjustment[HPCLANG_RPL_CODE_A] == 0xffffb);
    assert(!origins.adjustment[HPCLANG_RPL_CODE_B]);
    exchange.first = 2;
    exchange.nibbles = 1;
    assert(!hpclang_rpl_code_origins_exchange(&origins, &exchange));
    assert(!origins.adjustment[HPCLANG_RPL_CODE_A]);
    assert(!origins.registers[HPCLANG_RPL_CODE_B][2]);
    for (unsigned digit = 0; digit < 5; ++digit)
        if (digit != 2)
            assert(!origins.registers[HPCLANG_RPL_CODE_A][digit]);
    origins.adjustment[HPCLANG_RPL_CODE_C] = 0xffffb;
    for (unsigned digit = 0; digit < 5; ++digit)
        origins.registers[HPCLANG_RPL_CODE_C][digit] = 100 + digit;
    struct hpclang_rpl_code_memory memory = {.data = HPCLANG_RPL_CODE_C,
                                             .address = HPCLANG_RPL_CODE_D1,
                                             .first = 2,
                                             .nibbles = 1,
                                             .load = 0};
    uint32_t token = 999;
    assert(!hpclang_rpl_code_origins_memory(&origins, &memory, &token, 1));
    assert(!token); /* A computed digit cannot be stored as its base digit. */
    token = 999;
    memory.load = 1;
    assert(!hpclang_rpl_code_origins_memory(&origins, &memory, &token, 1));
    assert(!origins.adjustment[HPCLANG_RPL_CODE_C]);
    for (unsigned digit = 0; digit < 5; ++digit)
        assert(origins.registers[HPCLANG_RPL_CODE_C][digit] == (digit == 2 ? 999u : 0u));
    origins.adjustment[HPCLANG_RPL_CODE_C] = 0xffffb;
    const struct hpclang_rpl_code_literal literal = {.destination = HPCLANG_RPL_CODE_C,
                                                     .nibbles = 1,
                                                     .mask = UINT64_C(15) << 12,
                                                     .value = UINT64_C(9) << 12};
    assert(!hpclang_rpl_code_origins_literal(&origins, &literal, 3, &token, 1));
    assert(!origins.adjustment[HPCLANG_RPL_CODE_C]);
    for (unsigned digit = 0; digit < 5; ++digit)
        assert(origins.registers[HPCLANG_RPL_CODE_C][digit] == (digit == 3 ? 999u : 0u));
    struct image code = {.size = 2, .fail = UINT32_MAX};
    encode(&code, 0, 2, 0x60);
    const struct hpclang_rpl_reader reader = {read_nibble, &code};
    struct hpclang_rpl_code_execution state = {0};
    state.origins = origins;
    state.origins.adjustment[HPCLANG_RPL_CODE_C] = 0xffffb;
    state.values.registers[HPCLANG_RPL_CODE_C] = 0x12345;
    state.control.stack_generated[0] = 123;
    struct hpclang_rpl_code_step_uses uses;
    assert(!hpclang_rpl_code_execution_step(&reader, NULL, 2, &state, data_token, NULL, &uses));
    assert(state.control.stack[0] == 0x12345);
    assert(!state.control.stack_generated[0]);
    for (unsigned digit = 0; digit < 5; ++digit)
        assert(!state.control.stack_origins[0][digit]); /* RSTK does not yet carry expressions. */
    state.control.stack_generated[0] = 123;
    state.control.pc = 0;
    encode(&code, 0, 2, 0x70);
    assert(!hpclang_rpl_code_execution_step(&reader, NULL, 2, &state, data_token, NULL, &uses));
    assert(!state.control.stack_generated[0]);
    for (unsigned digit = 0; digit < 5; ++digit)
        assert(!state.origins.registers[HPCLANG_RPL_CODE_C][digit]);
}

static void generated_return_plans(void)
{
    struct image code = {.address = 0x100, .size = 128, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &code};
    const struct hpclang_rpl_reference_mappings mapping = {1, 1, 1};
    for (unsigned kind = 0; kind < 3; ++kind) {
        unsigned prefix = kind ? 2 : 1, width = kind == 0 ? 3 : kind == 1 ? 4 : 5;
        unsigned length = prefix + width;
        encode(&code, 16, prefix, kind == 0 ? 7 : kind == 1 ? 0xe8 : 0xf8);
        encode(&code, 16 + prefix, width, kind == 2 ? 0x160 : 0x160 - 0x110 - length);
        struct hpclang_rpl_code_address_use use = {.kind = HPCLANG_RPL_CODE_USE_RETURN_TARGET,
                                                   .value = 0x110 + length,
                                                   .generated = 0x111};
        struct hpclang_rpl_code_patch patch = sentinel;
        int result = hpclang_rpl_code_generated_return_plan(
            &reader, &use, &mapping, 0x110, 0x110 + length, 16, use.value + 16, &patch);
        assert(result == (kind == 2 ? 0 : 1));
        if (result) {
            assert(patch.field == 0x110 + prefix && patch.width == width);
            assert(patch.expected == 0x160 - 0x110 - length && patch.value == patch.expected - 16);
        } else {
            assert(!memcmp(&patch, &sentinel, sizeof patch));
        }
        patch = sentinel;
        assert(hpclang_rpl_code_generated_return_plan(&reader, &use, &mapping, 0x100, 0x180, 16,
                                                      use.value + 16, &patch) == (kind == 2));
        if (kind == 2)
            assert(patch.value == 0x170);
        else
            assert(!memcmp(&patch, &sentinel, sizeof patch));
        const struct hpclang_rpl_reference_mappings target_only = {2, 1, 1};
        assert(hpclang_rpl_code_generated_return_plan(&reader, &use, &target_only, 0x150, 0x180, 16,
                                                      use.value, &patch) == 1);
        assert(hpclang_rpl_code_generated_return_plan(&reader, &use, &mapping, 0x110,
                                                      0x110 + length, -16, use.value - 16,
                                                      &patch) == (kind == 2 ? 0 : 1));
        for (unsigned digit = 0; digit < length; ++digit) {
            code.fail = 0x110 + digit;
            patch = sentinel;
            assert(hpclang_rpl_code_generated_return_plan(&reader, &use, &mapping, 0x100, 0x180, 16,
                                                          use.value + 16,
                                                          &patch) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            assert(!memcmp(&patch, &sentinel, sizeof patch));
        }
        code.fail = UINT32_MAX;
        for (unsigned mode = 0; mode < 7; ++mode) {
            struct hpclang_rpl_code_address_use bad = use;
            uint32_t first = 0x110, end = 0x110 + length, replacement = use.value + 16;
            if (mode == 0)
                bad.generated = 0;
            if (mode == 1)
                bad.adjustment = 1;
            if (mode == 2)
                bad.origins[0] = 1;
            if (mode == 3)
                ++bad.value;
            if (mode == 4)
                ++replacement;
            if (mode == 5)
                ++first;
            if (mode == 6)
                --end;
            patch = sentinel;
            assert(hpclang_rpl_code_generated_return_plan(&reader, &bad, &mapping, first, end, 16,
                                                          replacement, &patch) < 0);
            assert(!memcmp(&patch, &sentinel, sizeof patch));
        }
    }
    code.address = 0xffff9;
    code.size = 7;
    encode(&code, 0, 2, 0xf8);
    encode(&code, 2, 5, 32);
    const struct hpclang_rpl_code_address_use wrap = {
        .kind = HPCLANG_RPL_CODE_USE_RETURN_TARGET, .value = 0, .generated = 0xffffa};
    struct hpclang_rpl_code_patch patch = sentinel;
    assert(!hpclang_rpl_code_generated_return_plan(&reader, &wrap, &mapping, 0xffff9, 0x100000, -16,
                                                   0xffff0, &patch));
    assert(!memcmp(&patch, &sentinel, sizeof patch));
}

static void subtract_fields(void)
{
    struct image code = {.size = 128, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &code};
    static const unsigned firsts[] = {0, 0, 2, 0, 15, 3, 0, 0};
    static const unsigned widths[] = {1, 1, 1, 3, 1, 12, 2, 16};
    for (unsigned field = 0; field < 8; ++field)
        for (unsigned p = 0; p < 16; ++p)
            for (unsigned decimal = 0; decimal < 2; ++decimal) {
                unsigned first = field == 0 ? p : firsts[field];
                unsigned width = field == 1 ? p + 1 : widths[field];
                uint64_t mask = width == 16 ? UINT64_MAX : (UINT64_C(1) << (4 * width)) - 1;
                mask <<= 4 * first;
                struct hpclang_rpl_code_values values = {.carry = 1, .decimal = decimal};
                values.registers[HPCLANG_RPL_CODE_P] = p;
                values.registers[HPCLANG_RPL_CODE_A] = UINT64_C(0x8888888888888888) & ~mask;
                values.registers[HPCLANG_RPL_CODE_C] = UINT64_C(1) << (4 * first);
                struct hpclang_rpl_code_values before = values;
                struct hpclang_rpl_code_origins origins;
                seed_origins(&origins);
                origins.adjustment[HPCLANG_RPL_CODE_A] = 5;
                struct hpclang_rpl_code_origins old = origins;
                encode(&code, 0, 3, 0xa0b | (field << 4));
                uint32_t length = 99;
                assert(!hpclang_rpl_code_data_step(&reader, NULL, 0, 128, &values, &origins,
                                                   data_token, NULL, &length));
                uint64_t expected = before.registers[HPCLANG_RPL_CODE_A] |
                                    ((decimal ? UINT64_C(0x9999999999999999) : UINT64_MAX) & mask);
                assert(values.registers[HPCLANG_RPL_CODE_A] == expected);
                assert(values.carry == 1 && values.decimal == decimal && length == 3);
                for (unsigned reg = 1; reg < 13; ++reg)
                    assert(values.registers[reg] == before.registers[reg]);
                for (unsigned digit = 0; digit < 16; ++digit) {
                    unsigned written = digit >= first && digit < first + width;
                    unsigned dependent = first < 5 && digit < 5;
                    assert(origins.registers[0][digit] ==
                           (written || dependent ? 0 : old.registers[0][digit]));
                }
                assert(origins.adjustment[0] == (first < 5 ? 0 : 5));
                for (unsigned digit = 0; digit < 3; ++digit) {
                    values = before;
                    origins = old;
                    length = 99;
                    code.fail = digit;
                    assert(hpclang_rpl_code_data_step(&reader, NULL, 0, 128, &values, &origins,
                                                      data_token, NULL, &length) < 0);
                    assert(!memcmp(&values, &before, sizeof values));
                    assert(!memcmp(&origins, &old, sizeof origins) && length == 99);
                }
                code.fail = UINT32_MAX;
            }
    /* Invalid BCD inputs cannot partially publish arithmetic or provenance. */
    encode(&code, 0, 3, 0xa6b);
    struct hpclang_rpl_code_values values = {.decimal = 1};
    values.registers[0] = 0xa0;
    struct hpclang_rpl_code_values before = values;
    struct hpclang_rpl_code_origins origins;
    seed_origins(&origins);
    struct hpclang_rpl_code_origins old = origins;
    uint32_t length = 99;
    assert(hpclang_rpl_code_data_step(&reader, NULL, 0, 128, &values, &origins, data_token, NULL,
                                      &length) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&values, &before, sizeof values));
    assert(!memcmp(&origins, &old, sizeof origins) && length == 99);
}

int main(void)
{
    subtract_fields();
    generated_return_plans();
    computed_origin_transfers();
    native_threaded_exit();
    execution_steps();
    arithmetic_modes();
    control_steps();
    data_steps();
    origin_repair_plans();
    bank_exit_program();
    bank_entry_program();
    address_origin_invalidation();
    address_adjustments();
    exhaustive();
    native_cross_object_call();
    failures_and_wrap();
    predicates();
    absolute_branches();
    absolute_data_pointers();
    indirect_transfers();
    instruction_extents();
    native_service_continuation();
    register_assignments();
    literal_loads();
    literal_origin_failures();
    register_exchanges();
    pc_capture_bounds();
    pc_exchanges();
    memory_transfers();
    origin_aliases_and_failures();
    return 0;
}
