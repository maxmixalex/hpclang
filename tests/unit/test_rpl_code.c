#include "rpl-code.h"
#include <assert.h>
#include <string.h>

struct image
{
    uint8_t data[64];
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
     * five-nibble 819 field/register form. */
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
                                 "80bff",   "80b82f"};
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
    const char *const unknown[] = {"105",   "11f",    "12d",      "80811",   "818f40",
                                   "819f4", "81af30", "81af2d",   "81b0",    "81b8",
                                   "80b02", "80bf7f", "80b83f00", "80b2858", "80b08e8"};
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

int main(void)
{
    exhaustive();
    native_cross_object_call();
    failures_and_wrap();
    predicates();
    absolute_branches();
    absolute_data_pointers();
    indirect_transfers();
    instruction_extents();
    return 0;
}
