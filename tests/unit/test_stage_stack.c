#include "stage-stack.h"
#include <assert.h>
#include <string.h>

static uint8_t memory[0x80000];
static uint32_t fault;
static unsigned writes;
static int fail_write;

static void put(uint32_t at, uint32_t value, unsigned width)
{
    assert(at >= 0x80000 && at + width <= 0x100000);
    for (unsigned i = 0; i < width; ++i)
        memory[at - 0x80000 + i] = (value >> (4 * i)) & 15;
}
static int read_word(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    (void)context;
    if (at < 0x80000 || at > 0x100000 - width || (fault >= at && fault - at < width))
        return 0;
    *value = 0;
    for (unsigned i = 0; i < width; ++i)
        *value |= (uint32_t)memory[at - 0x80000 + i] << (4 * i);
    return 1;
}
static int write_word(void *context, uint32_t at, uint32_t value)
{
    (void)context;
    ++writes;
    if (fail_write)
        return 0;
    put(at, value, 5);
    return 1;
}
static uint32_t get(uint32_t at)
{
    uint32_t value;
    assert(read_word(NULL, at, 5, &value));
    return value;
}
static void setup(void)
{
    memset(memory, 0, sizeof memory);
    fault = UINT32_MAX;
    writes = 0;
    fail_write = 0;
    put(0x806f8, 0xf0000, 5);
    put(0xf0000, 0x90000, 5);
    put(0xf0005, 0x92000, 5); /* Caller input beneath the hidden descriptor. */
    put(0x90000, 0x2a4e, 5);
    put(0x90005, 45, 5);
    put(0x90012, 0x08012340, 8);
    put(0x90022, 0x08012340, 8);
    put(0x807ed, 0x12345, 5); /* An unrelated field must remain unchanged. */
}

static int oversized_instruction(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    (void)context;
    (void)at;
    (void)width;
    *value = 0x130;
    return 1;
}

static void test_immediate_return_root(void)
{
    setup();
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    uint32_t stack[8] = {0x80001, 0x81002, 0x82003, 0x83004, 0x84005, 0x85006, 0x86007, 0x87008};
    uint32_t before[8];
    memcpy(before, stack, sizeof stack);
    for (uint32_t instruction = 0; instruction < 256; ++instruction) {
        put(0x88800, instruction, 2);
        for (uint32_t cursor = 0; cursor < 8; ++cursor) {
            uint32_t *root = hpclang_stage_immediate_return_root(&io, 0x88800, stack, cursor);
            int returns = instruction == 0x10 || instruction == 0x20 || instruction == 0x30;
            assert(root == (returns ? stack + ((cursor + 7) & 7) : NULL));
        }
    }
    assert(!writes && !memcmp(before, stack, sizeof stack));
    put(0xffffe, 0x30, 2);
    assert(hpclang_stage_immediate_return_root(&io, 0xffffe, stack, 0) == &stack[7]);
    for (uint32_t at = 0xffffe; at < 0x100000; ++at) {
        fault = at;
        assert(!hpclang_stage_immediate_return_root(&io, 0xffffe, stack, 0));
    }
    fault = UINT32_MAX;
    assert(!hpclang_stage_immediate_return_root(&io, 0xfffff, stack, 0));
    assert(!hpclang_stage_immediate_return_root(&io, UINT32_MAX, stack, 0));
    assert(!hpclang_stage_immediate_return_root(&io, 0xffffe, stack, 8));
    assert(!hpclang_stage_immediate_return_root(&io, 0xffffe, stack, UINT32_MAX));
    assert(!hpclang_stage_immediate_return_root(NULL, 0xffffe, stack, 0));
    assert(!hpclang_stage_immediate_return_root(&io, 0xffffe, NULL, 0));
    const struct hpclang_stage_stack_io missing = {0};
    const struct hpclang_stage_stack_io malformed = {NULL, oversized_instruction, NULL};
    assert(!hpclang_stage_immediate_return_root(&missing, 0xffffe, stack, 0));
    assert(!hpclang_stage_immediate_return_root(&malformed, 0xffffe, stack, 0));
    stack[7] = 0x100000;
    assert(!hpclang_stage_immediate_return_root(&io, 0xffffe, stack, 0));
    stack[7] = 0xfffff;
    assert(hpclang_stage_immediate_return_root(&io, 0xffffe, stack, 0) == &stack[7]);
    stack[7] = 0;
    assert(hpclang_stage_immediate_return_root(&io, 0xffffe, stack, 0) == &stack[7]);
    assert(!writes);
}

static void test_return_chain(void)
{
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        for (unsigned length = 1; length <= 8; ++length) {
            for (unsigned tail = 0; tail < 256; ++tail) {
                setup();
                uint32_t stack[8], before[8];
                uint32_t *roots[8] = {0};
                put(0x88000, 0x10, 2);
                for (unsigned depth = 0; depth < 8; ++depth) {
                    unsigned slot = (cursor + 7 - depth) & 7;
                    stack[slot] = 0x88100 + depth * 0x10;
                    /* Unvisited lower cells also look like execution addresses. */
                    put(stack[slot], depth + 1 < length ? 0x10 * (1 + depth % 3) : tail, 2);
                }
                memcpy(before, stack, sizeof stack);
                unsigned count = hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots);
                unsigned expected = tail == 0x10 || tail == 0x20 || tail == 0x30 ? 8 : length;
                assert(count == expected);
                for (unsigned depth = 0; depth < 8; ++depth)
                    assert(roots[depth] ==
                           (depth < count ? stack + ((cursor + 7 - depth) & 7) : NULL));
                assert(!writes && !memcmp(before, stack, sizeof stack));
                for (unsigned depth = 0; depth < length; ++depth) {
                    fault = depth ? stack[(cursor + 8 - depth) & 7] : 0x88000;
                    assert(hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots) == depth);
                }
            }
        }
    }
    setup();
    uint32_t stack[8] = {0};
    uint32_t *roots[8] = {0};
    put(0x88000, 0x10, 2);
    stack[7] = 0x88000;
    stack[6] = 0x100000;
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 1);
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 8, roots) == 0);
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, NULL) == 0);
    assert(hpclang_stage_return_roots(NULL, 0x88000, stack, 0, roots) == 0);
    assert(hpclang_stage_return_roots(&io, 0x88000, NULL, 0, roots) == 0);
    /* Repeated target values still belong to distinct consumed cells. */
    for (unsigned i = 0; i < 8; ++i)
        stack[i] = 0x88000;
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 8);
    for (unsigned i = 0; i < 8; ++i)
        assert(roots[i] == &stack[7 - i]);
    assert(!writes);
}

static unsigned put_jump(uint32_t at, uint32_t target, unsigned kind)
{
    unsigned width = kind == 0 ? 3 : kind == 1 ? 4 : 5;
    unsigned prefix = kind == 0 ? 1 : 2;
    put(at, kind == 0 ? 6 : kind == 1 ? 0xc8 : 0xd8, prefix);
    put(at + prefix, kind == 2 ? target : target - at - prefix, width);
    return prefix + width;
}

static void test_jump_return_chain(void)
{
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        setup();
        uint32_t stack[8], before[8];
        uint32_t *roots[8] = {0};
        put_jump(0x88000, 0x88020, 0);
        put(0x88020, 0x10, 2);
        for (unsigned depth = 0; depth < 8; ++depth) {
            uint32_t at = 0x88100 + depth * 0x100;
            stack[(cursor + 7 - depth) & 7] = at;
            if (depth < 7) {
                put_jump(at, at - 0x20, depth % 3);
                put(at - 0x20, 0x10 * (1 + depth % 3), 2);
            }
        }
        memcpy(before, stack, sizeof stack);
        assert(hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots) == 8);
        for (unsigned depth = 0; depth < 8; ++depth)
            assert(roots[depth] == stack + ((cursor + 7 - depth) & 7));
        for (unsigned depth = 0; depth < 7; ++depth) {
            uint32_t at = 0x88100 + depth * 0x100;
            unsigned size = depth % 3 == 0 ? 4 : depth % 3 == 1 ? 6 : 7;
            for (unsigned nibble = 0; nibble < size; ++nibble) {
                fault = at + nibble;
                assert(hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots) == depth + 1);
            }
            fault = at - 0x20;
            assert(hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots) == depth + 1);
        }
        assert(!writes && !memcmp(before, stack, sizeof stack));
    }
    /* Every relative encoding is tested against its independently selected
     * destination. Exclude overlap with the source instruction itself. */
    for (unsigned kind = 0; kind < 2; ++kind) {
        setup();
        uint32_t stack[8] = {0}, *roots[8];
        unsigned bits = kind ? 16 : 12;
        unsigned prefix = kind ? 2 : 1;
        for (int32_t offset = -(1 << (bits - 1)); offset < (1 << (bits - 1)); ++offset) {
            uint32_t target = 0x90000 + prefix + offset;
            if (target + 2 > 0x90000 && target < 0x90006)
                continue;
            put_jump(0x90000, target, kind);
            put(target, 0x10, 2);
            assert(hpclang_stage_return_roots(&io, 0x90000, stack, 0, roots) == 1);
            assert(roots[0] == &stack[7]);
            put(target, 0, 2);
        }
        assert(!writes);
    }
    setup();
    uint32_t stack[8] = {0}, *roots[8] = {0};
    put_jump(0x88000, 0x88000, 0); /* No return in a cycle. */
    assert(!hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots));
    for (unsigned jumps = 63; jumps <= 64; ++jumps) {
        setup();
        for (unsigned i = 0; i < jumps; ++i)
            put_jump(0x88000 + 8 * i, 0x88008 + 8 * i, 2);
        put(0x88000 + 8 * jumps, 0x10, 2);
        assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == (jumps == 63));
    }
    /* Calls, conditionals and register jumps require separate state analysis. */
    const uint32_t unsupported[] = {0x107, 0x10e8, 0x10f8, 0x14, 0x15, 0x2b18, 0x3b18};
    for (unsigned i = 0; i < sizeof unsupported / sizeof unsupported[0]; ++i) {
        setup();
        put(0x88000, unsupported[i], 7);
        assert(!hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots));
    }
    for (unsigned kind = 0; kind < 3; ++kind) {
        unsigned size = kind == 0 ? 4 : kind == 1 ? 6 : 7;
        setup();
        uint32_t at = 0x100000 - size;
        put_jump(at, 0xffff0, kind);
        put(0xffff0, 0x10, 2);
        assert(hpclang_stage_return_roots(&io, at, stack, 0, roots) == 1);
        put(at + 1, kind == 0 ? 6 : kind == 1 ? 0xc8 : 0xd8, kind ? 2 : 1);
        assert(!hpclang_stage_return_roots(&io, at + 1, stack, 0, roots));
    }
}

int main(void)
{
    test_jump_return_chain();
    test_immediate_return_root();
    test_return_chain();
    struct hpclang_stage_stack_io io = {NULL, read_word, write_word};
    struct stage_launcher_bias bias;
    setup();
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    assert(bias.active && bias.object == 0x90000 && get(0x806f8) == 0xf0005);
    assert(get(0xf0005) == 0x92000);
    /* Simulate application push and relocation of the hidden native object. */
    put(0x806f8, 0xf0000, 5);
    put(0xf0000, 0x93000, 5);
    memcpy(memory + 0x10100, memory + 0x10000, 50);
    bias.object = 0x90100;
    /* Native Launcher V3 controls return after these already-consumed ARM
     * fields are poisoned, including after descriptor relocation. The object
     * remains a root; restore must not reinterpret its entry/base as roots. */
    put(0x90112, 0xdeadbeef, 8);
    put(0x90122, 0xbaadf00d, 8);
    fault = 0x90112;
    assert(hpclang_stage_stack_restore(&io, &bias));
    fault = UINT32_MAX;
    assert(get(0x806f8) == 0xefffb && get(0xefffb) == 0x90100);
    /* Native PrRUN drops exactly the restored cell, retaining output/input. */
    put(0x806f8, get(0x806f8) + 5, 5);
    assert(get(get(0x806f8)) == 0x93000 && get(0xf0005) == 0x92000);
    assert(get(0x807ed) == 0x12345);

    const uint32_t changed[] = {0x90000, 0x90005, 0x90012, 0x90022};
    for (unsigned i = 0; i < 4; ++i) {
        setup();
        memory[changed[i] - 0x80000] ^= 1;
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias));
        assert(!writes && !bias.active && get(0x806f8) == 0xf0000);
    }
    for (uint32_t at = 0x90012; at < 0x9002a; ++at) {
        if (at >= 0x9001a && at < 0x90022)
            continue;
        setup();
        fault = at;
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    }
    const uint32_t invalid_top[] = {0x7ffff, 0xffffc, 0xfffff};
    for (unsigned i = 0; i < 3; ++i) {
        setup();
        put(0x806f8, invalid_top[i], 5);
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    }
    setup();
    put(0xf0000, 0xfffd0, 5);
    put(0xfffd0, 0x2a4e, 5);
    put(0xfffd5, 45, 5);
    assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    setup();
    assert(!hpclang_stage_stack_hide(&io, 0, &bias));
    assert(!hpclang_stage_stack_hide(&io, 0x08012341, &bias));
    assert(!writes);
    fail_write = 1;
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == -1 && !bias.active);
    fail_write = 0;
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    fail_write = 1;
    assert(!hpclang_stage_stack_restore(&io, &bias));
    assert(!hpclang_stage_stack_hide(NULL, 0, &bias));
    assert(hpclang_stage_stack_restore(NULL, NULL));

    setup();
    put(0x90005, 21, 5);
    put(0x9000a, 0x08012340, 8);
    put(0x90012, 0x01c00000, 8);
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    assert(hpclang_stage_stack_restore(&io, &bias));
    for (uint32_t at = 0x9000a; at < 0x90012; ++at) {
        fault = at;
        writes = 0;
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    }
    fault = UINT32_MAX;
    put(0x9000a, 0x08012344, 8);
    assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    setup();
    put(0x90000, 0x2a2c, 5);
    put(0x90005, 19, 5);
    const uint8_t magic[] = {0x4c, 0xb3, 0x80, 0x41, 0x3e, 0x43, 0x50};
    for (unsigned i = 0; i < sizeof magic; ++i)
        put(0x9000a + 2 * i, magic[i], 2);
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    assert(hpclang_stage_stack_restore(&io, &bias));
    memory[0x90017 - 0x80000] ^= 1;
    writes = 0;
    assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    return 0;
}
