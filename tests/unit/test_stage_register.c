#include "stage-stack.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

static uint8_t data[0x100000];
static uint32_t fault;
static unsigned reads;
static int read_word(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    (void)context;
    ++reads;
    if (at > sizeof data - width || (fault >= at && fault - at < width))
        return 0;
    *value = 0;
    for (unsigned i = 0; i < width; ++i)
        *value |= (uint32_t)data[at + i] << (4 * i);
    return 1;
}
static void put(uint32_t at, uint32_t value, unsigned width)
{
    for (unsigned i = 0; i < width; ++i)
        data[at + i] = (value >> (4 * i)) & 15u;
}
static void test_stack_transfers(void)
{
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    for (unsigned cursor = 0; cursor < 8; ++cursor)
        for (unsigned scenario = 0; scenario < 10; ++scenario) {
            memset(data, 15, sizeof data);
            fault = UINT32_MAX;
            uint32_t stack[8], saved[8];
            for (unsigned i = 0; i < 8; ++i)
                stack[i] = 0x93000 + i * 16;
            unsigned top = (cursor + 7u) & 7u;
            stack[top] = 0xabc92000;
            uint32_t a = 0xabc94000, cv = 0xdef91000;
            uint32_t *roots[11] = {0}, masks[11] = {0};
            unsigned pop =
                scenario == 1 || scenario == 2 || scenario == 4 || scenario == 6 || scenario == 8;
            put(0x90000, pop ? 0x70 : 0x60, 2);
            put(0x90002, pop ? 0x3b18 : 0x10, pop ? 4 : 2);
            if (scenario == 2 || scenario == 3)
                put(0x90002, 0xffff, 4); /* Copy alone is not an address use. */
            if (scenario == 4) {
                put(0x90002, 0x5b18, 4); /* C=PC discards the popped source. */
                put(0x90006, 0x3b18, 4);
            }
            if (scenario == 5) {
                put(0x90002, 0x60, 2); /* Two copies of one initial C. */
                put(0x90004, 0x10, 2);
                put(0x91000, 0x10, 2);
            }
            if (scenario == 6) {
                put(0x90002, 0x60, 2); /* Pop/push retains the stack origin. */
                put(0x90004, 0x10, 2);
            }
            if (scenario == 7) {
                put(0x90000, 0x1c7, 4); /* Generated call return has no root. */
                put(0x90004, 0xffff, 4);
                put(0x90020, 0x70, 2);
                put(0x90022, 0x3b18, 4);
            }
            if (scenario == 8)
                put(0x92000, 0x10, 2); /* Then consume another original cell. */
            if (scenario == 9) {
                put(0x90002, 0x5b18, 4); /* Capturing C does not erase its copy. */
                put(0x90006, 0x10, 2);
            }
            memcpy(saved, stack, sizeof stack);
            unsigned count = hpclang_stage_execution_roots(&io, 0x90000, stack, cursor, 1, &a, &cv,
                                                           roots, masks);
            unsigned expected = scenario == 2 || scenario == 3 || scenario == 4 || scenario == 7
                                    ? 0u
                                : scenario == 8 ? 2u
                                                : 1u;
            assert(count == expected);
            if (count) {
                assert(roots[0] == (pop ? stack + top : &cv));
                assert(masks[0] == 0xfffff);
                if (scenario == 8) {
                    assert(roots[1] == stack + ((cursor + 6) & 7));
                    assert(masks[1] == UINT32_MAX);
                }
            }
            for (unsigned i = count; i < 11; ++i)
                assert(!roots[i] && !masks[i]);
            assert(a == 0xabc94000 && cv == 0xdef91000);
            assert(!memcmp(stack, saved, sizeof stack));
            /* A failed transfer instruction cannot fabricate a source. */
            for (unsigned nibble = 0; nibble < 2; ++nibble) {
                fault = 0x90000 + nibble;
                memset(roots, 0, sizeof roots);
                memset(masks, 0, sizeof masks);
                assert(!hpclang_stage_execution_roots(&io, 0x90000, stack, cursor, 1, &a, &cv,
                                                      roots, masks));
                assert(!roots[0] && !masks[0]);
                assert(!memcmp(stack, saved, sizeof stack));
            }
        }
}

int main(void)
{
    test_stack_transfers();
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    for (unsigned cursor = 0; cursor < 8; ++cursor)
        for (int carry = -1; carry <= 1; ++carry)
            for (unsigned selected = 0; selected < 2; ++selected)
                for (unsigned scenario = 0; scenario < 5; ++scenario) {
                    memset(data, 15, sizeof data);
                    fault = UINT32_MAX;
                    reads = 0;
                    uint32_t stack[8], saved[8];
                    for (unsigned i = 0; i < 8; ++i)
                        stack[i] = 0x92000 + i * 16;
                    memcpy(saved, stack, sizeof saved);
                    uint32_t a = 0xabc91000, c = 0xdef91000;
                    uint32_t *roots[10] = {0}, masks[10] = {0};
                    unsigned op = (scenario == 1 ? 4u : scenario == 2 ? 6u : 2u) + selected;
                    put(0x90000, (op << 12) | 0xb18, 4);
                    put(0x91000, 0x10, 2);
                    if (scenario == 1) /* Capture kills the incoming value before a use. */
                        put(0x90004, ((2u + selected) << 12) | 0xb18, 4);
                    if (scenario == 2) /* Return to PC=A/C after exchange; no second old root. */
                        stack[(cursor + 7u) & 7u] = 0x90004;
                    if (scenario == 2)
                        put(0x90004, ((2u + selected) << 12) | 0xb18, 4);
                    if (scenario == 3) { /* Both initial registers feed the executed path. */
                        if (selected)
                            a = 0xabc93000;
                        else
                            c = 0xdef93000;
                        put(0x91000, ((3u - selected) << 12) | 0xb18, 4);
                        put(0x93000, 0x10, 2);
                    }
                    if (scenario == 4) /* Unsupported instruction must not be skipped. */
                        put(0x90000, 0xffff, 4);
                    memcpy(saved, stack, sizeof saved);
                    uint32_t old_a = a, old_c = c;
                    unsigned count = hpclang_stage_execution_roots(&io, 0x90000, stack, cursor,
                                                                   carry, &a, &c, roots, masks);
                    unsigned expected = scenario == 1 || scenario == 4 ? 0u
                                        : scenario == 3                ? 3u
                                                                       : 2u;
                    assert(count == expected);
                    if (count) {
                        assert(roots[0] == (selected ? &c : &a) && masks[0] == 0xfffff);
                        if (scenario == 3)
                            assert(roots[1] == (selected ? &a : &c) && masks[1] == 0xfffff);
                        assert(roots[count - 1] == &stack[(cursor + 7u) & 7u]);
                        assert(masks[count - 1] == UINT32_MAX);
                    }
                    assert(a == old_a && c == old_c && !memcmp(stack, saved, sizeof stack));
                    assert(reads <= 192);
                    for (unsigned i = count; i < 10; ++i)
                        assert(!roots[i] && !masks[i]);
                }
    for (unsigned op = 2; op < 8; ++op)
        for (unsigned nibble = 0; nibble < 4; ++nibble) {
            memset(data, 15, sizeof data);
            put(0x90000, (op << 12) | 0xb18, 4);
            fault = 0x90000 + nibble;
            uint32_t stack[8] = {0}, a = 0x91000, c = 0x92000;
            uint32_t *roots[10] = {0}, masks[10] = {0};
            assert(!hpclang_stage_execution_roots(&io, 0x90000, stack, 0, 0, &a, &c, roots, masks));
            for (unsigned i = 0; i < 10; ++i)
                assert(!roots[i] && !masks[i]);
        }
    fault = UINT32_MAX;
    for (unsigned op = 4; op < 8; ++op) {
        memset(data, 15, sizeof data);
        put(0xffffc, (op << 12) | 0xb18, 4);
        uint32_t stack[8] = {0}, a = 0x91000, c = 0x92000;
        uint32_t *roots[10] = {0}, masks[10] = {0};
        assert(!hpclang_stage_execution_roots(&io, 0xffffc, stack, 0, 0, &a, &c, roots, masks));
        assert(!roots[0] && !masks[0]);
    }
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        memset(data, 15, sizeof data);
        put(0x90000, 0x2b18, 4);
        put(0x91000, 0x3b18, 4);
        put(0x92000, 0x10, 2);
        uint32_t stack[8], saved[8];
        for (unsigned i = 0; i < 8; ++i) {
            stack[i] = 0x93000 + i * 16;
            put(stack[i], 0x10, 2);
        }
        memcpy(saved, stack, sizeof saved);
        uint32_t a = 0xabc91000, c = 0xdef92000;
        uint32_t *roots[11] = {0}, masks[11] = {0};
        assert(hpclang_stage_execution_roots(&io, 0x90000, stack, cursor, -1, &a, &c, roots,
                                             masks) == 10);
        assert(roots[0] == &a && roots[1] == &c);
        assert(masks[0] == 0xfffff && masks[1] == 0xfffff);
        for (unsigned i = 0; i < 8; ++i) {
            assert(roots[i + 2] == stack + ((cursor + 7 - i) & 7));
            assert(masks[i + 2] == UINT32_MAX);
        }
        assert(!roots[10] && !masks[10]);
        assert(a == 0xabc91000 && c == 0xdef92000 && !memcmp(stack, saved, sizeof stack));
    }
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        memset(data, 15, sizeof data);
        fault = UINT32_MAX;
        put(0x90000, 0x10, 2);
        put(0x91000, 0xf4, 3);
        put(0x91003, 0x10, 2);
        put(0x91010, 0x2b18, 4);
        uint32_t stack[8], saved[8];
        for (unsigned i = 0; i < 8; ++i) {
            stack[(cursor + 7 - i) & 7] = 0x91000 + i * 0x100;
            if (i)
                put(0x91000 + i * 0x100, i == 7 ? 0x20 : 0x10, 2);
        }
        memcpy(saved, stack, sizeof saved);
        uint32_t a = 0xabc95000, c = 0xdef96000;
        uint32_t *roots[10] = {0}, masks[10] = {0};
        /* The ninth pop sees zero, never the old conditional branch. Without
         * clearing consumed slots, set carry falsely reaches PC=A on a loop. */
        assert(hpclang_stage_execution_roots(&io, 0x90000, stack, cursor, 0, &a, &c, roots,
                                             masks) == 8);
        for (unsigned i = 0; i < 8; ++i) {
            assert(roots[i] == stack + ((cursor + 7 - i) & 7));
            assert(masks[i] == UINT32_MAX);
        }
        assert(!roots[8] && !roots[9] && !masks[8] && !masks[9]);
        assert(a == 0xabc95000 && c == 0xdef96000 && !memcmp(stack, saved, sizeof stack));
    }
    return 0;
}
