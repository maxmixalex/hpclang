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
static void seed_origins(struct hpclang_rpl_code_origins *state);

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

static void literal_loads(void)
{
    struct hpclang_rpl_code_literal untouched;
    memset(&untouched, 0x5a, sizeof untouched);
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
            if (valid)
                memset(expected.registers[reg], 0, 5 * sizeof(uint32_t));
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

int main(void)
{
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
    register_exchanges();
    pc_capture_bounds();
    memory_transfers();
    origin_aliases_and_failures();
    return 0;
}
