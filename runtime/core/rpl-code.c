#include "rpl-code.h"

struct instruction_cursor
{
    const struct hpclang_rpl_reader *reader;
    uint32_t at, limit, used;
    uint8_t code[21]; /* LA can contain sixteen immediate nibbles. */
};

static int instruction_take(struct instruction_cursor *cursor, unsigned count)
{
    if (count > cursor->limit || count > sizeof cursor->code)
        return HPCLANG_RPL_SPAN_BOUNDS;
    while (cursor->used < count) {
        uint8_t nibble;
        if (!cursor->reader->read_nibble(cursor->reader->context, cursor->at + cursor->used,
                                         &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        cursor->code[cursor->used++] = nibble;
    }
    return 0;
}

static int instruction_size(struct instruction_cursor *cursor, unsigned *size)
{
    int result = instruction_take(cursor, 2);
    if (result)
        return result;
    const uint8_t *op = cursor->code;
    switch (op[0]) {
    case 0:
        *size = op[1] == 14 ? 4 : 2;
        return 0;
    case 1:
        if (op[1] <= 2) {
            result = instruction_take(cursor, 3);
            if (result)
                return result;
            if ((op[2] & 7) > 4)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
        switch (op[1]) {
        case 5:
        case 9:
        case 13:
            *size = 4;
            break;
        case 10:
        case 14:
            *size = 6;
            break;
        case 11:
        case 15:
            *size = 7;
            break;
        default:
            *size = 3;
            break;
        }
        return 0;
    case 2:
        *size = 2;
        return 0;
    case 3:
        *size = op[1] + 3;
        return 0;
    case 4:
    case 5:
        *size = 3;
        return 0;
    case 6:
    case 7:
        *size = 4;
        return 0;
    case 9:
        *size = 5;
        return 0;
    case 10:
    case 11:
        *size = 3;
        return 0;
    case 12:
    case 13:
    case 14:
    case 15:
        *size = 2;
        return 0;
    default:
        break; /* 8: extended instructions. */
    }
    if (op[1] >= 12) {
        *size = (op[1] & 1) ? 7 : 6;
        return 0;
    }
    if (op[1] >= 2) {
        *size = (op[1] == 2 || op[1] == 4 || op[1] == 5) ? 3 : 5;
        return 0;
    }
    result = instruction_take(cursor, 3);
    if (result)
        return result;
    if (op[1] == 1) {
        if (op[2] < 8 || op[2] >= 12) {
            *size = 3;
            return 0;
        }
        *size = op[2] == 11 ? 4 : op[2] == 9 ? 5 : 6;
        result = instruction_take(cursor, *size);
        if (result)
            return result;
        if ((op[2] == 8 && (op[4] & 7) > 3) || (op[2] == 9 && op[4] > 3) ||
            (op[2] == 10 && (op[4] > 2 || (op[5] & 7) > 4)) ||
            (op[2] == 11 && (op[3] < 2 || op[3] > 7)))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        return 0;
    }
    if (op[2] == 11) {
        /* Saturn+ facts: Eric Rechlin's newopcodes.txt and the pinned ROM
         * 80B26 wrapper described below. Unknown selectors remain unsupported. */
        result = instruction_take(cursor, 5);
        if (result)
            return result;
        if (op[4] == 8) {
            result = instruction_take(cursor, 7);
            if (result)
                return result;
            unsigned custom = op[6] >= 8 && op[6] <= 14;
            unsigned pair = op[5];
            unsigned cross = pair == 3 || pair == 7 || pair == 12 || pair == 13;
            if ((op[3] == 0 &&
                 (cross || (custom && pair != 5 && pair != 10 && pair != 14 && pair != 15))) ||
                (op[3] == 1 && (cross || custom)) || op[3] == 3 || op[3] == 4 || op[3] == 5 ||
                op[3] == 8 || op[3] == 9 || op[3] == 10) {
                *size = 7;
                return 0;
            }
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
        unsigned service = (op[3] << 4) | op[4];
        switch (service) {
        case 0x00:
        case 0x01:
        case 0x03:
        case 0x05:
        case 0x06:
        case 0x11:
        case 0x13:
        case 0x15:
        case 0x16:
        case 0x21:
        case 0x23:
        /* HP49g+/HP50g: service table 0x41440, native selector 0x62,
         * wrapper 0x30ee4. Receives PC+5, saves that continuation separately
         * from helper 0x306a0's data arguments, and returns it unchanged.
         * This proves framing only, not helper effects or relocation safety. */
        case 0x26:
        case 0x31:
        case 0x33:
        case 0x41:
        case 0x43:
        case 0x50:
        case 0x51:
        case 0x60:
        case 0x65:
        case 0x70:
        case 0x71:
        case 0x80:
        case 0x92:
        case 0xa0:
        case 0xa2:
        case 0xb0:
        case 0xb2:
        case 0xc2:
        case 0xfe:
        case 0xff:
            *size = 5;
            return 0;
        case 0x82:
            *size = 6;
            return 0;
        default:
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
    }
    if (op[2] != 8) {
        *size = op[2] == 12 || op[2] == 13 || op[2] == 15 ? 4 : 3;
        return 0;
    }
    result = instruction_take(cursor, 4);
    if (result)
        return result;
    switch (op[3]) {
    case 1:
    case 2:
        result = instruction_take(cursor, 5);
        if (result)
            return result;
        if (op[3] == 1 && op[4])
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        *size = op[3] == 1 ? 5 : op[4] + 6;
        return 0;
    case 4:
    case 5:
    case 8:
    case 9:
        *size = 5;
        return 0;
    case 6:
    case 7:
    case 10:
    case 11:
        *size = 7;
        return 0;
    default:
        *size = 4;
        return 0;
    }
}

int hpclang_rpl_code_extent(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                            uint32_t limit, uint32_t *length)
{
    if (!reader || !reader->read_nibble || !length)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result)
        return result;
    result = instruction_take(&cursor, size);
    if (result)
        return result;
    *length = size;
    return 0;
}

int hpclang_rpl_code_visit(const struct hpclang_rpl_reader *reader, uint32_t first, uint32_t length,
                           hpclang_rpl_code_visit_fn visit, void *context)
{
    if (!reader || !reader->read_nibble || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (first >= 0x100000u || length > 0x100000u - first)
        return HPCLANG_RPL_SPAN_BOUNDS;
    while (length) {
        uint32_t size;
        int result = hpclang_rpl_code_extent(reader, first, length, &size);
        if (result)
            return result;
        result = visit(context, first, size);
        if (result)
            return result;
        first += size;
        length -= size;
    }
    return 0;
}

int hpclang_rpl_code_literal_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                     uint32_t limit, unsigned p,
                                     struct hpclang_rpl_code_literal *literal)
{
    if (!reader || !reader->read_nibble || !literal)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result || (result = instruction_take(&cursor, size)))
        return result;
    const uint8_t *op = cursor.code;
    unsigned prefix;
    enum hpclang_rpl_code_register destination;
    if (op[0] == 3) {
        prefix = 2;
        destination = HPCLANG_RPL_CODE_C;
    } else if (op[0] == 8 && op[1] == 0 && op[2] == 8 && op[3] == 2) {
        prefix = 5;
        destination = HPCLANG_RPL_CODE_A;
    } else if (op[0] == 1 && (op[1] == 9 || op[1] == 10 || op[1] == 11 || op[1] == 13 ||
                              op[1] == 14 || op[1] == 15)) {
        prefix = 2;
        destination = op[1] < 12 ? HPCLANG_RPL_CODE_D0 : HPCLANG_RPL_CODE_D1;
        p = 0; /* Address literals always replace low nibbles, independent of P. */
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    struct hpclang_rpl_code_literal next = {.instruction = instruction,
                                            .length = size,
                                            .field = instruction + prefix,
                                            .destination = destination,
                                            .nibbles = size - prefix};
    for (unsigned i = 0; i < next.nibbles; ++i) {
        unsigned shift = 4 * ((p + i) & 15);
        next.mask |= UINT64_C(15) << shift;
        next.value |= (uint64_t)op[prefix + i] << shift;
    }
    *literal = next;
    return 0;
}

int hpclang_rpl_code_assignment_inspect(const struct hpclang_rpl_reader *reader,
                                        uint32_t instruction, uint32_t limit, unsigned p,
                                        struct hpclang_rpl_code_assignment *assignment)
{
    if (!reader || !reader->read_nibble || !assignment)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result || (result = instruction_take(&cursor, size)))
        return result;
    const uint8_t *op = cursor.code;
    struct hpclang_rpl_code_assignment next = {.instruction = instruction, .length = size};
    unsigned field = 7;
    if (op[0] == 8 && op[1] == 1 && op[2] == 11 && (op[3] == 4 || op[3] == 5)) {
        if (instruction > 0xffffbu)
            return HPCLANG_RPL_SPAN_BOUNDS;
        next.source = HPCLANG_RPL_CODE_NEXT_PC;
        next.destination = op[3] == 4 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C;
        next.nibbles = 5;
        next.constant = instruction + 4;
    } else if (op[0] == 1 && op[1] == 3 && !(op[2] & 2)) {
        next.source = (op[2] & 4) ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A;
        next.destination = (op[2] & 1) ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0;
        next.nibbles = (op[2] & 8) ? 4 : 5;
    } else if (op[0] == 2) {
        next.source = HPCLANG_RPL_CODE_CONSTANT;
        next.destination = HPCLANG_RPL_CODE_P;
        next.constant = op[1];
        next.nibbles = 1;
    } else if (op[0] == 8 && op[1] == 0 && (op[2] == 12 || op[2] == 13)) {
        next.source = op[2] == 12 ? HPCLANG_RPL_CODE_P : HPCLANG_RPL_CODE_C;
        next.destination = op[2] == 12 ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_P;
        next.source_first = op[2] == 13 ? op[3] : 0;
        next.destination_first = op[2] == 12 ? op[3] : 0;
        next.nibbles = 1;
    } else if ((op[0] == 1 && op[1] < 2) ||
               (op[0] == 8 && op[1] == 1 && op[2] == 10 && op[4] < 2)) {
        unsigned selector = op[0] == 1 ? op[2] : op[5];
        unsigned from_saved = op[0] == 1 ? op[1] : op[4];
        enum hpclang_rpl_code_register working =
            selector < 8 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C;
        enum hpclang_rpl_code_register saved = HPCLANG_RPL_CODE_R0 + (selector & 7);
        next.source = from_saved ? saved : working;
        next.destination = from_saved ? working : saved;
        field = op[0] == 1 ? 7 : op[3];
    } else if (op[0] == 13 || (op[0] == 10 && op[1] >= 8)) {
        unsigned selector = op[0] == 13 ? op[1] : op[2];
        if (selector >= 12)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        field = op[0] == 13 ? 15 : op[1] - 8;
        if (selector < 4) {
            next.source = HPCLANG_RPL_CODE_CONSTANT;
            next.destination = (enum hpclang_rpl_code_register)selector;
        } else {
            static const uint8_t destinations[] = {0, 1, 2, 3, 1, 2, 0, 2};
            static const uint8_t sources[] = {1, 2, 0, 2, 0, 1, 2, 3};
            next.source = (enum hpclang_rpl_code_register)sources[selector - 4];
            next.destination = (enum hpclang_rpl_code_register)destinations[selector - 4];
        }
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    if (!next.nibbles) {
        static const unsigned first[] = {0, 0, 2, 0, 15, 3, 0, 0};
        static const unsigned width[] = {1, 1, 1, 3, 1, 12, 2, 16};
        if (field >= 8 && field != 15)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        next.destination_first = field == 15 ? 0 : field == 0 ? p : first[field];
        next.source_first = next.source == HPCLANG_RPL_CODE_CONSTANT ? 0 : next.destination_first;
        next.nibbles = field == 15 ? 5 : field == 1 ? p + 1 : width[field];
    }
    *assignment = next;
    return 0;
}

int hpclang_rpl_code_memory_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                    uint32_t limit, unsigned p,
                                    struct hpclang_rpl_code_memory *memory)
{
    if (!reader || !reader->read_nibble || !memory)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (!result)
        result = instruction_take(&cursor, size);
    if (result)
        return result;
    const uint8_t *op = cursor.code;
    if (op[0] != 1 || (op[1] != 4 && op[1] != 5))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned selector = op[2];
    struct hpclang_rpl_code_memory next = {
        .instruction = instruction,
        .length = size,
        .data = selector & 4 ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A,
        .address = selector & 1 ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0,
        .load = (selector & 2) != 0,
    };
    if (op[1] == 5 && selector >= 8) {
        next.nibbles = op[3] + 1u;
    } else {
        unsigned field = op[1] == 4 ? (selector < 8 ? 15 : 6) : op[3];
        static const unsigned first[] = {0, 0, 2, 0, 15, 3, 0, 0};
        static const unsigned width[] = {1, 1, 1, 3, 1, 12, 2, 16};
        if (field >= 8 && field != 15)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        next.first = field == 15 ? 0 : field == 0 ? p : first[field];
        next.nibbles = field == 15 ? 5 : field == 1 ? p + 1 : width[field];
    }
    *memory = next;
    return 0;
}

int hpclang_rpl_code_address_adjust_inspect(const struct hpclang_rpl_reader *reader,
                                            uint32_t instruction, uint32_t limit,
                                            struct hpclang_rpl_code_address_adjust *adjust)
{
    if (!reader || !reader->read_nibble || !adjust)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    int result = instruction_take(&cursor, 2);
    if (result)
        return result;
    unsigned opcode = cursor.code[1];
    if (cursor.code[0] != 1 || (opcode != 6 && opcode != 7 && opcode != 8 && opcode != 12))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if ((result = instruction_take(&cursor, 3)))
        return result;
    struct hpclang_rpl_code_address_adjust next = {
        .instruction = instruction,
        .length = 3,
        .address = opcode == 6 || opcode == 8 ? HPCLANG_RPL_CODE_D0 : HPCLANG_RPL_CODE_D1,
        .delta = ((int32_t)cursor.code[2] + 1) * (opcode < 8 ? 1 : -1),
    };
    *adjust = next;
    return 0;
}

static unsigned origin_register_width(enum hpclang_rpl_code_register reg)
{
    if ((unsigned)reg <= HPCLANG_RPL_CODE_R4)
        return 16;
    if (reg == HPCLANG_RPL_CODE_P)
        return 1;
    if (reg == HPCLANG_RPL_CODE_D0 || reg == HPCLANG_RPL_CODE_D1)
        return 5;
    return 0;
}

static int origin_field_valid(enum hpclang_rpl_code_register reg, unsigned first, unsigned count)
{
    unsigned width = origin_register_width(reg);
    return count && first < width && count <= width - first;
}

int hpclang_rpl_code_origins_assign(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_assignment *assignment,
                                    const uint32_t *next_pc)
{
    if (!state || !assignment)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    unsigned count = assignment->nibbles;
    if (!origin_field_valid(assignment->destination, assignment->destination_first, count))
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (assignment->source == HPCLANG_RPL_CODE_NEXT_PC) {
        if (assignment->source_first || assignment->destination_first || count != 5 ||
            (assignment->destination != HPCLANG_RPL_CODE_A &&
             assignment->destination != HPCLANG_RPL_CODE_C))
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (!next_pc)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
    } else if (assignment->source == HPCLANG_RPL_CODE_CONSTANT) {
        if (assignment->source_first)
            return HPCLANG_RPL_SPAN_MALFORMED;
    } else if (!origin_field_valid(assignment->source, assignment->source_first, count)) {
        return HPCLANG_RPL_SPAN_MALFORMED;
    }
    uint32_t source[16];
    for (unsigned i = 0; i < count; ++i)
        source[i] = assignment->source == HPCLANG_RPL_CODE_CONSTANT ? 0
                    : assignment->source == HPCLANG_RPL_CODE_NEXT_PC
                        ? next_pc[i]
                        : state->registers[assignment->source][assignment->source_first + i];
    for (unsigned i = 0; i < count; ++i)
        state->registers[assignment->destination][assignment->destination_first + i] = source[i];
    return 0;
}

int hpclang_rpl_code_origins_exchange(struct hpclang_rpl_code_origins *state,
                                      const struct hpclang_rpl_code_exchange *exchange)
{
    if (!state || !exchange)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!origin_field_valid(exchange->left, exchange->first, exchange->nibbles) ||
        !origin_field_valid(exchange->right, exchange->first, exchange->nibbles))
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t left[16], right[16];
    for (unsigned i = 0; i < exchange->nibbles; ++i) {
        left[i] = state->registers[exchange->left][exchange->first + i];
        right[i] = state->registers[exchange->right][exchange->first + i];
    }
    for (unsigned i = 0; i < exchange->nibbles; ++i) {
        state->registers[exchange->left][exchange->first + i] = right[i];
        state->registers[exchange->right][exchange->first + i] = left[i];
    }
    return 0;
}

int hpclang_rpl_code_origins_memory(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_memory *transfer,
                                    uint32_t *memory, unsigned count)
{
    if (!state || !transfer || !memory)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if ((transfer->data != HPCLANG_RPL_CODE_A && transfer->data != HPCLANG_RPL_CODE_C) ||
        (transfer->address != HPCLANG_RPL_CODE_D0 && transfer->address != HPCLANG_RPL_CODE_D1) ||
        transfer->load > 1 || transfer->nibbles != count ||
        !origin_field_valid(transfer->data, transfer->first, count))
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t source[16];
    uint32_t *registers = state->registers[transfer->data] + transfer->first;
    for (unsigned i = 0; i < count; ++i)
        source[i] = transfer->load ? memory[i] : registers[i];
    for (unsigned i = 0; i < count; ++i)
        if (transfer->load)
            registers[i] = source[i];
        else
            memory[i] = source[i];
    return 0;
}

int hpclang_rpl_code_origins_adjust(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_address_adjust *adjust)
{
    if (!state || !adjust)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if ((adjust->address != HPCLANG_RPL_CODE_D0 && adjust->address != HPCLANG_RPL_CODE_D1) ||
        !adjust->delta || adjust->delta < -16 || adjust->delta > 16)
        return HPCLANG_RPL_SPAN_MALFORMED;
    for (unsigned i = 0; i < 5; ++i)
        state->registers[adjust->address][i] = 0;
    return 0;
}

int hpclang_rpl_code_exchange_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                      uint32_t limit, unsigned p,
                                      struct hpclang_rpl_code_exchange *exchange)
{
    if (!reader || !reader->read_nibble || !exchange)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || !limit || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (p > 15)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct instruction_cursor cursor = {.reader = reader, .at = instruction, .limit = limit};
    unsigned size;
    int result = instruction_size(&cursor, &size);
    if (result || (result = instruction_take(&cursor, size)))
        return result;
    const uint8_t *op = cursor.code;
    struct hpclang_rpl_code_exchange next = {.instruction = instruction, .length = size};
    unsigned field;
    if (op[0] == 1 && op[1] == 3 && (op[2] & 2)) {
        next.left = (op[2] & 4) ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A;
        next.right = (op[2] & 1) ? HPCLANG_RPL_CODE_D1 : HPCLANG_RPL_CODE_D0;
        next.nibbles = (op[2] & 8) ? 4 : 5;
        *exchange = next;
        return 0;
    }
    if ((op[0] == 1 && op[1] == 2) || (op[0] == 8 && op[1] == 1 && op[2] == 10 && op[4] == 2)) {
        unsigned selector = op[0] == 1 ? op[2] : op[5];
        next.left = selector < 8 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C;
        next.right = HPCLANG_RPL_CODE_R0 + (selector & 7);
        field = op[0] == 1 ? 7 : op[3];
    } else if (op[0] == 13 || (op[0] == 10 && op[1] >= 8)) {
        unsigned selector = op[0] == 13 ? op[1] : op[2];
        if (selector < 12)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        static const uint8_t left[] = {0, 1, 0, 2};
        static const uint8_t right[] = {1, 2, 2, 3};
        next.left = (enum hpclang_rpl_code_register)left[selector - 12];
        next.right = (enum hpclang_rpl_code_register)right[selector - 12];
        field = op[0] == 13 ? 15 : op[1] - 8;
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    static const unsigned first[] = {0, 0, 2, 0, 15, 3, 0, 0};
    static const unsigned width[] = {1, 1, 1, 3, 1, 12, 2, 16};
    if (field >= 8 && field != 15)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    next.first = field == 15 ? 0 : field == 0 ? p : first[field];
    next.nibbles = field == 15 ? 5 : field == 1 ? p + 1 : width[field];
    *exchange = next;
    return 0;
}

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

int hpclang_rpl_code_pc_exchange_inspect(const struct hpclang_rpl_reader *reader,
                                         uint32_t instruction, uint32_t limit, uint32_t a_low,
                                         uint32_t c_low,
                                         struct hpclang_rpl_code_pc_exchange *exchange)
{
    if (!reader || !reader->read_nibble || !exchange)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction > 0xffffbu || limit < 4 || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(reader, instruction, 4, &opcode);
    if (result)
        return result;
    if (opcode != 0x6b18u && opcode != 0x7b18u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    int selects_c = opcode == 0x7b18u;
    struct hpclang_rpl_code_pc_exchange next = {
        .capture = {.instruction = instruction,
                    .length = 4,
                    .source = HPCLANG_RPL_CODE_NEXT_PC,
                    .destination = selects_c ? HPCLANG_RPL_CODE_C : HPCLANG_RPL_CODE_A,
                    .nibbles = 5,
                    .constant = instruction + 4},
        .target = (selects_c ? c_low : a_low) & 0xfffffu};
    *exchange = next;
    return 0;
}

int hpclang_rpl_code_indirect_inspect(const struct hpclang_rpl_reader *instructions,
                                      const struct hpclang_rpl_reader *pointers,
                                      uint32_t instruction, uint32_t limit, uint32_t a_low,
                                      uint32_t c_low, struct hpclang_rpl_code_indirect *transfer)
{
    if (!instructions || !instructions->read_nibble || !transfer)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (instruction >= 0x100000u || limit < 4 || limit > 0x100000u - instruction)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t opcode;
    int result = read_word(instructions, instruction, 4, &opcode);
    if (result)
        return result;
    struct hpclang_rpl_code_indirect next = {0};
    next.instruction = instruction;
    switch (opcode) {
    case 0x2b18:
        break; /* PC=A */
    case 0x3b18:
        next.register_c = 1;
        break;
    case 0xc808:
        next.via_memory = 1;
        break;
    case 0xe808:
        next.register_c = next.via_memory = 1;
        break;
    default:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    next.register_value = (next.register_c ? c_low : a_low) & 0xfffffu;
    next.target = next.register_value;
    if (next.via_memory) {
        if (!pointers || !pointers->read_nibble)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (next.register_value > 0xffffbu)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = read_word(pointers, next.register_value, 5, &next.target);
        if (result)
            return result;
    }
    *transfer = next;
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
        if (opcode >= 12) {
            ++next.field;
            next.width = (opcode & 1) ? 5 : 4;
            next.call = opcode >= 14;
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
    if (next.width == 5) {
        next.target = next.encoded;
        *branch = next;
        return 0;
    }
    next.base = (next.field + (next.call ? next.width : 0)) & 0xfffffu;
    uint32_t range = 1u << (4 * next.width);
    int32_t displacement = (int32_t)next.encoded;
    if (next.encoded >= range / 2)
        displacement -= (int32_t)range;
    next.target = (uint32_t)((int32_t)next.base + displacement) & 0xfffffu;
    *branch = next;
    return 0;
}

static int plan_shift(const struct hpclang_rpl_reader *reader, uint32_t instruction, uint32_t limit,
                      const struct hpclang_rpl_reference_mappings *mappings, uint32_t first,
                      uint32_t end, int32_t delta, struct hpclang_rpl_code_patch *patch,
                      int pointer)
{
    if (!mappings || !patch)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (first >= end || end > 0x100000u || (int64_t)first + delta < 0 ||
        (int64_t)end + delta > 0x100000)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_code_branch branch;
    int result;
    if (pointer) {
        if (!reader || !reader->read_nibble)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (instruction >= 0x100000u || limit < 7 || limit > 0x100000u - instruction)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t opcode, target;
        result = read_word(reader, instruction, 2, &opcode);
        if (result)
            return result;
        if (opcode != 0xb1 && opcode != 0xf1)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        result = read_word(reader, instruction + 2, 5, &target);
        branch = (struct hpclang_rpl_code_branch){
            instruction, 7, instruction + 2, 5, 0, target, target, 0, 0};
    } else {
        result = hpclang_rpl_code_branch_inspect(reader, instruction, limit, &branch);
    }
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
    if (branch.width == 5) {
        if (target == branch.encoded)
            return 0;
        *patch = (struct hpclang_rpl_code_patch){branch.field, 5, branch.encoded, target};
        return 1;
    }
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

int hpclang_rpl_code_branch_plan_shift(const struct hpclang_rpl_reader *reader,
                                       uint32_t instruction, uint32_t limit,
                                       const struct hpclang_rpl_reference_mappings *mappings,
                                       uint32_t first, uint32_t end, int32_t delta,
                                       struct hpclang_rpl_code_patch *patch)
{
    return plan_shift(reader, instruction, limit, mappings, first, end, delta, patch, 0);
}

int hpclang_rpl_code_pointer_plan_shift(const struct hpclang_rpl_reader *reader,
                                        uint32_t instruction, uint32_t limit,
                                        const struct hpclang_rpl_reference_mappings *mappings,
                                        uint32_t first, uint32_t end, int32_t delta,
                                        struct hpclang_rpl_code_patch *patch)
{
    return plan_shift(reader, instruction, limit, mappings, first, end, delta, patch, 1);
}
