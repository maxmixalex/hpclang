#include "rpl-code.h"
#include <assert.h>
#include <string.h>

struct image
{
    uint8_t data[7];
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
        if (op == 0 || op == 3 || (op >= 6 && op <= 12) || op == 14)
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

int main(void)
{
    exhaustive();
    native_cross_object_call();
    failures_and_wrap();
    predicates();
    return 0;
}
