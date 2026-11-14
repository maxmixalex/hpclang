#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
#include "rpl-execution.h"

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return NULL;
}

struct fixture
{
    uint8_t native[0xc60], code[128], data[128];
    uint32_t pc, reads, observations, tokens, visits, spaces;
    uint32_t fail, mode, stop, visit_error;
};
struct reader_context
{
    struct fixture *fixture;
    unsigned space;
};

static uint8_t nibble(const struct fixture *f, unsigned space, uint32_t at)
{
    if (space == HPCLANG_RPL_EXECUTION_PC) {
        assert(at < 8);
        return (f->pc >> (at * 4)) & 15;
    }
    if (space == HPCLANG_RPL_EXECUTION_NATIVE) {
        assert(at < sizeof(f->native) * 2);
        return (f->native[at / 2] >> (4 * (at & 1))) & 15;
    }
    assert(at < 128);
    return space == HPCLANG_RPL_EXECUTION_CODE ? f->code[at] : f->data[at];
}

static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    struct reader_context *reader = context;
    struct fixture *f = reader->fixture;
    ++f->reads;
    if (f->reads == f->fail && f->mode == 1)
        return 0;
    *value = f->reads == f->fail && f->mode == 2 ? 16 : nibble(f, reader->space, at);
    return 1;
}

static int observe(void *context, unsigned space, uint32_t at, uint8_t value)
{
    struct fixture *f = context;
    assert(value == nibble(f, space, at));
    f->spaces |= 1u << space;
    if (++f->observations == f->fail && (f->mode == 3 || f->mode == 4))
        return f->mode == 3 ? -73 : 73;
    return 0;
}

static int token(void *context, unsigned data, uint32_t at, uint8_t value, uint32_t *result)
{
    struct fixture *f = context;
    assert(data <= 1 && value == nibble(f, data, at));
    if (++f->tokens == f->fail && (f->mode == 5 || f->mode == 6))
        return f->mode == 5 ? -74 : 74;
    *result = (data ? 0x100000 : 1) + at;
    return 0;
}

static int visit(void *context, uint32_t pc, const struct hpclang_rpl_code_execution *state,
                 const struct hpclang_rpl_code_step_uses *uses)
{
    struct fixture *f = context;
    ++f->visits;
    if (f->visits == 1) {
        assert(pc == 0 && state->control.pc == 3 && uses->count == 1);
        assert(uses->addresses[0].kind == HPCLANG_RPL_CODE_USE_LOAD_ADDRESS);
        assert(uses->addresses[0].value == 32);
        for (unsigned i = 0; i < 5; ++i)
            assert(uses->addresses[0].origins[i] == 0x200000 + 0x958 * 2 + i);
    } else {
        assert(f->visits == 2 && pc == 3 && state->control.pc == 96 && uses->count == 2);
        assert(uses->addresses[1].kind == HPCLANG_RPL_CODE_USE_MEMORY_TARGET);
        assert(uses->addresses[1].value == 96);
        for (unsigned i = 0; i < 5; ++i) {
            assert(uses->addresses[0].origins[i] == 0x100000 + 32 + i);
            assert(uses->addresses[1].origins[i] == 0x100000 + 64 + i);
        }
    }
    if (f->visit_error)
        return f->visit_error == 1 ? -75 : 2;
    return f->visits == f->stop;
}

static int store_record(void *context, const struct hpclang_rpl_code_store *effect)
{
    struct fixture *f = context;
    ++f->tokens; /* Count scratch records; no native/code/data write. */
    assert(effect->instruction == 0 && effect->first == 0 && effect->nibbles == 5);
    assert(effect->address.kind == HPCLANG_RPL_CODE_USE_STORE_ADDRESS);
    assert(effect->address.value == 32 && effect->value == 0 && !effect->adjustment);
    for (unsigned i = 0; i < 5; ++i) {
        assert(effect->address.origins[i] == 0x200000 + 0x958 * 2 + i);
        assert(effect->origins[i] == 0x200000 + 0x90c * 2 + i);
    }
    return f->visit_error;
}

static int store_visit(void *context, uint32_t pc, const struct hpclang_rpl_code_execution *state,
                       const struct hpclang_rpl_code_step_uses *uses)
{
    struct fixture *f = context;
    ++f->visits;
    assert(pc == 0 && state->control.pc == 3 && uses->count == 1);
    assert(uses->addresses[0].kind == HPCLANG_RPL_CODE_USE_STORE_ADDRESS);
    return 0;
}

static void reset(struct fixture *f)
{
    *f = (struct fixture){.stop = 2};
    f->native[0x958] = 32;
    const uint8_t code[] = {1, 4, 7, 8, 0, 8, 14}; /* C=DAT1.A; PC=(C). */
    memcpy(f->code, code, sizeof code);
    f->data[33] = 4; /* pointer 64 */
    f->data[65] = 6; /* target 96 */
}

int main(void)
{
    struct fixture f;
    reset(&f);
    struct reader_context native = {&f, HPCLANG_RPL_EXECUTION_NATIVE};
    struct reader_context code = {&f, HPCLANG_RPL_EXECUTION_CODE};
    struct reader_context data = {&f, HPCLANG_RPL_EXECUTION_DATA};
    const struct hpclang_rpl_execution_io io = {{read_nibble, &native},
                                                {read_nibble, &code},
                                                {read_nibble, &data},
                                                observe,
                                                token,
                                                visit,
                                                &f,
                                                NULL};
    struct hpclang_rpl_memory_bounds bounds = {.magic = HPCLANG_RPL_BOUNDS_MAGIC_V5,
                                               .temp_limit = 0xa0000,
                                               .user_limit = 0xb0000,
                                               .execution_pc = &f.pc};
    struct hpclang_boot_info boot = {.model = HPCLANG_MODEL_HP50G,
                                     .saturn_state = (uintptr_t)f.native,
                                     .platform_cookie = (uintptr_t)&bounds |
                                                        HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG};
    struct hpclang_rpl_code_execution state;
    uint32_t steps;
    assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 20, &state, &steps) ==
           HPCLANG_RPL_EXECUTION_BOUNDARY);
    assert(steps == 2 && f.visits == 2 && f.spaces == 15);
    unsigned reads = f.reads, observations = f.observations, tokens = f.tokens;
    assert(observations == reads + 8 && tokens == 10);
    const struct fixture original = f;
    for (unsigned mode = 1; mode <= 6; ++mode) {
        unsigned limit = mode <= 2 ? reads : mode <= 4 ? observations : tokens;
        for (unsigned fail = 1; fail <= limit; ++fail) {
            reset(&f);
            f.mode = mode;
            f.fail = fail;
            int result = hpclang_rpl_execution_collect(&boot, &io, 0x200000, 20, &state, &steps);
            assert(result == (mode == 1   ? HPCLANG_RPL_SPAN_UNAVAILABLE
                              : mode == 3 ? -73
                              : mode == 5 ? -74
                                          : HPCLANG_RPL_SPAN_MALFORMED));
            assert(steps <= 1 && !memcmp(f.native, original.native, sizeof f.native));
            assert(!memcmp(f.code, original.code, sizeof f.code));
            assert(!memcmp(f.data, original.data, sizeof f.data));
        }
    }
    reset(&f);
    assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 1, &state, &steps) ==
           HPCLANG_RPL_EXECUTION_BUDGET);
    assert(steps == 1 && state.control.pc == 3);
    reset(&f);
    f.stop = 0;
    assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 2, &state, &steps) ==
           HPCLANG_RPL_EXECUTION_BUDGET);
    reset(&f);
    f.stop = 0;
    assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 3, &state, &steps) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED); /* RTNSXM at target cannot be skipped. */
    assert(steps == 2 && state.control.pc == 96);
    for (unsigned error = 1; error <= 2; ++error) {
        reset(&f);
        f.visit_error = error;
        assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 20, &state, &steps) ==
               (error == 1 ? -75 : HPCLANG_RPL_SPAN_MALFORMED));
        assert(steps == 1);
    }
    reset(&f);
    assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 0, &state, &steps) < 0);
    assert(!steps && !f.reads && !f.observations);
    const uint32_t versions[] = {HPCLANG_RPL_BOUNDS_MAGIC_V2, HPCLANG_RPL_BOUNDS_MAGIC,
                                 HPCLANG_RPL_BOUNDS_MAGIC_V4};
    for (unsigned i = 0; i < 3; ++i) {
        bounds.magic = versions[i];
        assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 20, &state, &steps) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!steps && !f.reads && !f.observations);
    }
    bounds.magic = HPCLANG_RPL_BOUNDS_MAGIC_V5;
    f.pc = 0x100000;
    assert(hpclang_rpl_execution_collect(&boot, &io, 0x200000, 20, &state, &steps) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!steps && !f.reads);
    for (unsigned mode = 0; mode < 4; ++mode) {
        reset(&f);
        f.code[2] = 1; /* DAT1=A.A */
        f.visit_error = mode == 1 ? -78 : mode == 2 ? 1 : 0;
        struct hpclang_rpl_execution_io stores = io;
        stores.visit = store_visit;
        struct fixture before = f;
        int result = hpclang_rpl_execution_collect_effects(&boot, &stores, 0x200000, 1, &state,
                                                           &steps, mode == 3 ? NULL : store_record);
        assert(result == (mode == 0   ? HPCLANG_RPL_EXECUTION_BUDGET
                          : mode == 1 ? -78
                          : mode == 2 ? HPCLANG_RPL_SPAN_MALFORMED
                                      : HPCLANG_RPL_SPAN_UNSUPPORTED));
        assert(steps == (mode == 0 ? 1u : 0u) && f.visits == steps);
        assert(state.control.pc == (mode == 0 ? 3u : 0u));
        assert(f.tokens == (mode == 3 ? 0u : 1u));
        assert(!memcmp(f.native, before.native, sizeof f.native));
        assert(!memcmp(f.data, before.data, sizeof f.data));
        assert(!memcmp(f.code, before.code, sizeof f.code));
    }
    return 0;
}
