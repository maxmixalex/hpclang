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
    /* Unwritten fixture cells are unsupported opcodes, not RTNSXM (00). */
    memset(memory, 15, sizeof memory);
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

static int oversized_condition(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    int result = read_word(context, at, width, value);
    if (result && at == 0xffffe && width == 2)
        *value = 256;
    return result;
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
            int returns = instruction == 0 || instruction == 0x10 || instruction == 0x20 ||
                          instruction == 0x30;
            assert(root == (returns ? stack + ((cursor + 7) & 7) : NULL));
            if (returns) {
                for (unsigned nibble = 0; nibble < 2; ++nibble) {
                    fault = 0x88800 + nibble;
                    assert(!hpclang_stage_immediate_return_root(&io, 0x88800, stack, cursor));
                }
                fault = UINT32_MAX;
            }
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
                    put(stack[slot], depth + 1 < length ? 0x10 * (depth % 4) : tail, 2);
                }
                memcpy(before, stack, sizeof stack);
                unsigned count = hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots);
                unsigned expected =
                    tail == 0 || tail == 0x10 || tail == 0x20 || tail == 0x30 ? 8 : length;
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
                put(at - 0x20, 0x10 * (depth % 4), 2);
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
            put(target, 0xff, 2);
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
    /* Calls to unclassified code, conditionals and register jumps stop discovery. */
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

static void test_call_return_chain(void)
{
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    for (unsigned kind = 0; kind < 2; ++kind) {
        setup();
        uint32_t stack[8] = {0}, *roots[8] = {0};
        unsigned prefix = kind ? 2 : 1, width = kind ? 4 : 3;
        uint32_t next = 0x90000 + prefix + width;
        const int32_t offsets[] = {-(1 << (4 * width - 1)), -32, 32, (1 << (4 * width - 1)) - 1};
        for (unsigned i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
            uint32_t target = next + offsets[i];
            put(0x90000, kind ? 0xe8 : 7, prefix);
            put(0x90000 + prefix, (uint32_t)offsets[i], width);
            put(next, 0x10, 2);
            put(target, 0x10, 2);
            assert(hpclang_stage_return_roots(&io, 0x90000, stack, 0, roots) == 1);
            assert(roots[0] == stack + 7 && !writes);
            put(target, 0xff, 2);
        }
    }
    for (unsigned kind = 0; kind < 3; ++kind)
        for (unsigned cursor = 0; cursor < 8; ++cursor)
            for (unsigned depth = 1; depth <= 9; ++depth) {
                setup();
                uint32_t stack[8], before[8], *roots[8] = {0};
                unsigned prefix = kind ? 2 : 1;
                unsigned width = kind == 0 ? 3 : kind == 1 ? 4 : 5;
                for (unsigned i = 0; i < 8; ++i) {
                    stack[i] = 0x89000 + 16 * i;
                    put(stack[i], 0x10, 2);
                }
                memcpy(before, stack, sizeof stack);
                for (unsigned i = 0; i < depth; ++i) {
                    uint32_t at = 0x88000 + 64 * i;
                    put(at, kind == 0 ? 7 : kind == 1 ? 0xe8 : 0xf8, prefix);
                    put(at + prefix, kind == 2 ? at + 64 : 64 - prefix - width, width);
                    put(at + prefix + width, 0x10, 2);
                }
                put(0x88000 + 64 * depth, 0, 2);
                unsigned expected = depth < 8 ? 8 - depth : 0;
                assert(hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots) == expected);
                for (unsigned i = 0; i < 8; ++i)
                    assert(roots[i] == (i < expected ? &stack[(cursor + 7 - i) & 7] : NULL));
                assert(!memcmp(before, stack, sizeof stack) && !writes);
                /* A fault in the first call cannot expose even a plausible old root. */
                for (unsigned i = 0; i < prefix + width; ++i) {
                    fault = 0x88000 + i;
                    assert(!hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots));
                    assert(!memcmp(before, stack, sizeof stack) && !writes);
                }
            }
}

static void test_conditional_paths(void)
{
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    for (unsigned cursor = 0; cursor < 8; ++cursor)
        for (unsigned carry = 0; carry < 2; ++carry)
            for (unsigned opcode = 4; opcode <= 5; ++opcode)
                for (unsigned encoded = 0; encoded < 256; ++encoded) {
                    uint32_t at = 0x88000;
                    int32_t offset = encoded < 128 ? (int32_t)encoded : (int32_t)encoded - 256;
                    uint32_t target = (uint32_t)((int32_t)at + 1 + offset);
                    int overlaps = encoded && target < at + 3 && target + 2 > at;
                    setup();
                    uint32_t stack[8], before[8], *roots[8] = {0};
                    for (unsigned i = 0; i < 8; ++i)
                        stack[i] = 0x8a000;
                    stack[(cursor + 7) & 7] = at;
                    memcpy(before, stack, sizeof stack);
                    put(0x87000, carry ? 0x20 : 0x30, 2);
                    put(at, opcode, 1);
                    put(at + 1, encoded, 2);
                    int taken = carry == (opcode == 4);
                    unsigned expected = 1;
                    if (encoded) {
                        if (!taken || !overlaps) {
                            put(taken ? target : at + 3, 0x10, 2);
                            expected = 2;
                        }
                        /* Overlapping taken targets see the actual branch bytes:
                         * offsets -2/+1 stop at unsupported opcodes, -1 loops. */
                    } else if (taken) {
                        expected = 2;
                    }
                    assert(hpclang_stage_return_roots(&io, 0x87000, stack, cursor, roots) ==
                           expected);
                    for (unsigned i = 0; i < 8; ++i)
                        assert(roots[i] == (i < expected ? &stack[(cursor + 7 - i) & 7] : NULL));
                    for (unsigned nibble = 0; nibble < 3; ++nibble) {
                        fault = at + nibble;
                        assert(hpclang_stage_return_roots(&io, 0x87000, stack, cursor, roots) == 1);
                    }
                    fault = UINT32_MAX;
                    /* A plausible native carry is never inferred from these inputs. */
                    assert(!hpclang_stage_return_roots(&io, at, stack, cursor, roots));
                    put(0x87000, 0, 2); /* RTNSXM does not establish carry. */
                    assert(hpclang_stage_return_roots(&io, 0x87000, stack, cursor, roots) == 1);
                    assert(!writes && !memcmp(stack, before, sizeof stack));
                }
    /* Both RTN and RTNSXM retain a previously established carry. */
    for (unsigned carry = 0; carry < 2; ++carry)
        for (unsigned preserve = 0; preserve <= 0x10; preserve += 0x10) {
            setup();
            uint32_t stack[8] = {0}, *roots[8] = {0};
            stack[7] = 0x88100;
            stack[6] = 0x88200;
            put(0x88000, carry ? 0x20 : 0x30, 2);
            put(0x88100, preserve, 2);
            put(0x88200, carry ? 4 : 5, 3);
            assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 3);
            assert(roots[2] == stack + 5);
        }
    /* A generated call frame sets carry without becoming an original root. */
    setup();
    uint32_t stack[8] = {0}, *roots[8] = {0};
    put(0x88000, 0x37, 4); /* GOSUB to 88007 */
    put(0x88004, 5, 3);    /* RTNNC */
    put(0x88007, 0x30, 2);
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 1);
    assert(roots[0] == stack + 7 && !writes);
    /* A callee's later flag-setting return supersedes the earlier carry. */
    stack[7] = 0x88000;
    put(0x87000, 0x20, 2);
    put(0x88000, 0x1c7, 4); /* Call 88020, leaving an unsupported fallthrough after RTNNC. */
    put(0x88007, 0xff, 2);
    put(0x88020, 0x30, 2);
    assert(hpclang_stage_return_roots(&io, 0x87000, stack, 0, roots) == 2);
    assert(roots[0] == stack + 7 && roots[1] == stack + 6);
    /* A taken conditional cycle must retain the instruction budget. */
    stack[7] = 0x88100;
    put(0x88000, 0x20, 2);
    put(0x88100, 0xff4, 3);
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 1);

    setup();
    stack[7] = 0xffffd;
    put(0x88000, 0x20, 2);
    put(0xffffd, 4, 3); /* Last complete RTNC encoding. */
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 2);
    for (unsigned i = 0; i < 3; ++i) {
        fault = 0xffffd + i;
        assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 1);
    }
    fault = UINT32_MAX;
    const struct hpclang_stage_stack_io malformed = {NULL, oversized_condition, NULL};
    assert(hpclang_stage_return_roots(&malformed, 0x88000, stack, 0, roots) == 1);
    put(0xffffd, 0x24, 3); /* Both paths wrap to an unreadable zero PC. */
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 1);
    stack[7] = 0xffffe;
    put(0xffffe, 4, 2); /* Truncated operand must not be read across the mapping. */
    assert(hpclang_stage_return_roots(&io, 0x88000, stack, 0, roots) == 1);
}

static void test_main_page(void)
{
    uint32_t map[256] = {0};
    const uint32_t physical = 0x08040000u;
    uint32_t output = 0x12345u;
    assert(hpclang_stage_main_page(NULL, physical, &output) == -1);
    assert(hpclang_stage_main_page(map, physical, NULL) == -1);
    assert(hpclang_stage_main_page(map, physical + 1, &output) == -1);
    assert(hpclang_stage_main_page(map, physical, &output) == 0);
    assert(output == 0x12345u);
    /* Every possible full-page placement, including odd Saturn pages. */
    for (unsigned first = 128; first < 255; ++first) {
        memset(map, 0, sizeof map);
        map[first] = physical;
        map[first + 1] = physical + 2048;
        assert(hpclang_stage_main_page(map, physical, &output) == 1);
        assert(output == first * 4096u);
        output = 0x12345u;
        map[first + 1] = physical + 4096;
        assert(hpclang_stage_main_page(map, physical, &output) == -1);
        assert(output == 0x12345u);
        map[first] = 0;
        map[first + 1] = physical + 2048;
        assert(hpclang_stage_main_page(map, physical, &output) == -1);
        assert(output == 0x12345u);
    }
    memset(map, 0, sizeof map);
    map[128] = physical;
    map[129] = physical + 2048;
    /* A second full alias is ambiguous even when each pair is contiguous. */
    map[200] = physical;
    map[201] = physical + 2048;
    assert(hpclang_stage_main_page(map, physical, &output) == -1);
    map[201] = 0;
    /* Interior/straddling aliases cannot escape a start-address-only scan. */
    for (unsigned offset = 1; offset < 4096; ++offset) {
        map[200] = physical + offset;
        assert(hpclang_stage_main_page(map, physical, &output) == -1);
        assert(output == 0x12345u);
    }
    for (unsigned offset = 1; offset < 2048; ++offset) {
        map[200] = physical - offset;
        assert(hpclang_stage_main_page(map, physical, &output) == -1);
        assert(output == 0x12345u);
    }
    map[200] = physical - 2048;
    assert(hpclang_stage_main_page(map, physical, &output) == 1);
    memset(map, 0, sizeof map);
    map[255] = physical;
    assert(hpclang_stage_main_page(map, physical, &output) == -1);
    map[255] = 0;
    map[128] = physical + 2048;
    assert(hpclang_stage_main_page(map, physical, &output) == -1);
    memset(map, 0, sizeof map);
    map[254] = 0xfffff000u;
    map[255] = 0xfffff800u;
    assert(hpclang_stage_main_page(map, 0xfffff000u, &output) == 1);
    assert(output == 0xfe000u);
}

static void test_initial_carry(void)
{
    const struct hpclang_stage_stack_io io = {NULL, read_word, NULL};
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        for (unsigned condition = 0; condition < 2; ++condition) {
            for (int carry = -1; carry <= 1; ++carry) {
                setup();
                uint32_t stack[8], before[8], *roots[8];
                for (unsigned i = 0; i < 8; ++i) {
                    stack[i] = 0x89000 + i * 0x10;
                    roots[i] = NULL;
                }
                memcpy(before, stack, sizeof stack);
                /* Test conditional returns and both signed branch directions.
                 * Only the taken branch reaches RTN; fallthrough is unsupported. */
                for (unsigned path = 0; path < 3; ++path) {
                    uint32_t displacement = path == 0 ? 0 : path == 1 ? 4 : 252;
                    put(0x88000, (displacement << 4) | (condition ? 4 : 5), 3);
                    if (path)
                        put(path == 1 ? 0x88005 : 0x87ffd, 0x10, 2);
                    memset(roots, 0, sizeof roots);
                    unsigned count = hpclang_stage_return_roots_with_carry(&io, 0x88000, stack,
                                                                           cursor, carry, roots);
                    unsigned expected = carry >= 0 && carry == (int)condition;
                    assert(count == expected);
                    assert(!expected || roots[0] == &stack[(cursor + 7) & 7]);
                    assert(roots[expected] == NULL && !memcmp(before, stack, sizeof stack));
                    assert(!writes);
                    assert(!hpclang_stage_return_roots(&io, 0x88000, stack, cursor, roots));
                    /* A missing opcode/displacement nibble cannot establish a path. */
                    for (unsigned nibble = 0; nibble < 3; ++nibble) {
                        fault = 0x88000 + nibble;
                        memset(roots, 0, sizeof roots);
                        assert(!hpclang_stage_return_roots_with_carry(&io, 0x88000, stack, cursor,
                                                                      carry, roots));
                        assert(roots[0] == NULL && !writes);
                        assert(!memcmp(before, stack, sizeof stack));
                    }
                    fault = UINT32_MAX;
                }
            }
        }
    }
    uint32_t stack[8] = {0}, *roots[8] = {NULL};
    for (int carry = -2; carry <= 2; carry += 4)
        assert(!hpclang_stage_return_roots_with_carry(&io, 0x88000, stack, 0, carry, roots));
    assert(roots[0] == NULL && !writes);
}

int main(void)
{
    test_initial_carry();
    test_main_page();
    test_conditional_paths();
    test_call_return_chain();
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
