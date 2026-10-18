#include "rpl-code.h"

static int read_word(const struct hpclang_rpl_reader *reader, uint32_t at, unsigned width,
                     uint32_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, at + i, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        *value |= (uint32_t)nibble << (4 * i);
    }
    return 0;
}

int hpclang_rpl_code_branch_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                    uint32_t limit, struct hpclang_rpl_code_branch *branch)
{
    if (!reader || !reader->read_nibble || !branch)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 1, &opcode);
    if (result)
        return result;
    struct hpclang_rpl_code_branch next = {0};
    next.instruction = instruction;
    next.field = instruction + 1;
    if (opcode == 4 || opcode == 5) {
        next.width = 2;
        next.conditional = 1;
    } else if (opcode == 6 || opcode == 7) {
        next.width = 3;
        next.call = opcode == 7;
    } else if (opcode == 9) {
        next.field = instruction + 3;
        next.width = 2;
        next.conditional = 1;
    } else if (opcode == 8) {
        if (limit < 2)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(reader, instruction + 1, 1, &opcode);
        if (result)
            return result;
        if (opcode == 12 || opcode == 14) {
            ++next.field;
            next.width = 4;
            next.call = opcode == 14;
        } else if (opcode == 3 || (opcode >= 6 && opcode <= 11)) {
            next.field = instruction + 3;
            next.width = 2;
            next.conditional = 1;
        } else if (opcode == 0) {
            if (limit < 4)
                return HPCLANG_RPL_SPAN_BOUNDS;
            result = read_word(reader, instruction + 2, 2, &opcode);
            if (result)
                return result;
            if (opcode != 0x68 && opcode != 0x78 && opcode != 0xa8 && opcode != 0xb8)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            next.field = instruction + 5;
            next.width = 2;
            next.conditional = 1;
        } else {
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    next.length = next.field - instruction + next.width;
    if (next.length > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;
    /* Predicate operands are part of the proven instruction, including fields
     * and bit indices. Validate them even though relocation preserves them. */
    uint32_t prefix;
    result = read_word(reader, instruction, next.field - instruction, &prefix);
    if (result)
        return result;
    result = read_word(reader, next.field, next.width, &next.encoded);
    if (result)
        return result;
    if (next.conditional && !next.encoded)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    next.base = (next.field + (next.call ? next.width : 0)) & 0xfffffu;
    uint32_t range = 1u << (4 * next.width);
    int32_t displacement = (int32_t)next.encoded;
    if (next.encoded >= range / 2)
        displacement -= (int32_t)range;
    next.target = (uint32_t)((int32_t)next.base + displacement) & 0xfffffu;
    *branch = next;
    return 0;
}

int hpclang_rpl_code_branch_plan_shift(const struct hpclang_rpl_reader *reader,
                                       uint32_t instruction, uint32_t limit,
                                       const struct hpclang_rpl_reference_mappings *mappings,
                                       uint32_t first, uint32_t end, int32_t delta,
                                       struct hpclang_rpl_code_patch *patch)
{
    if (!mappings || !patch)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (first >= end || end > 0x100000u || (int64_t)first + delta < 0 ||
        (int64_t)end + delta > 0x100000)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_code_branch branch;
    int result = hpclang_rpl_code_branch_inspect(reader, instruction, limit, &branch);
    if (result)
        return result;
    int source_moves = 0;
    if (mappings->field == mappings->moving && instruction < end &&
        first < instruction + branch.length) {
        if (instruction < first || instruction + branch.length > end)
            return HPCLANG_RPL_SPAN_BOUNDS;
        source_moves = 1;
    }
    int target_moves =
        mappings->target == mappings->moving && branch.target >= first && branch.target < end;
    uint32_t base = (uint32_t)((int64_t)branch.base + (source_moves ? delta : 0)) & 0xfffffu;
    uint32_t target = (uint32_t)((int64_t)branch.target + (target_moves ? delta : 0));
    uint32_t distance = (target - base) & 0xfffffu;
    uint32_t range = 1u << (4 * branch.width);
    if (distance >= range / 2 && distance < 0x100000u - range / 2)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t value = distance & (range - 1);
    if (branch.conditional && !value)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (value == branch.encoded)
        return 0;
    *patch = (struct hpclang_rpl_code_patch){branch.field, branch.width, branch.encoded, value};
    return 1;
}
