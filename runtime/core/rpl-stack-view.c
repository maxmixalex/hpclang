#include <stddef.h>
#include <stdint.h>

#include "rpl-stack-view.h"

enum
{
    RPL_SATURN_MAX = 0x000fffffu,
    RPL_DSKTOP = 0x000806f8u,
    RPL_EDITLINE = 0x000806fdu
};

static int checked_add(uint32_t a, uint32_t b, uint32_t *out)
{
    if (out == NULL || a > UINT32_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int read_word5(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL || value == NULL)
        return HPCLANG_RPL_STACK_UNAVAILABLE;
    if (address > RPL_SATURN_MAX - 4u)
        return HPCLANG_RPL_STACK_MALFORMED;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble))
            return HPCLANG_RPL_STACK_UNAVAILABLE;
        if (nibble > 0x0fu)
            return HPCLANG_RPL_STACK_MALFORMED;
        out |= (uint32_t)nibble << (i * 4u);
    }

    *value = out;
    return HPCLANG_RPL_STACK_OK;
}

static int stack_bounds(const struct hpclang_rpl_reader *reader, uint32_t *top, uint32_t *depth)
{
    uint32_t top_value = 0u;
    uint32_t editline_value = 0u;

    int result = read_word5(reader, RPL_DSKTOP, &top_value);
    if (result != HPCLANG_RPL_STACK_OK)
        return result;
    result = read_word5(reader, RPL_EDITLINE, &editline_value);
    if (result != HPCLANG_RPL_STACK_OK)
        return result;

    if (top_value > RPL_SATURN_MAX || editline_value > RPL_SATURN_MAX || editline_value < top_value)
        return HPCLANG_RPL_STACK_MALFORMED;

    uint32_t delta = editline_value - top_value;
    if (delta < 5u || delta % 5u != 0u)
        return HPCLANG_RPL_STACK_MALFORMED;

    uint32_t cells = delta / 5u;
    if (cells == 0u)
        return HPCLANG_RPL_STACK_MALFORMED;

    if (top != NULL)
        *top = top_value;
    if (depth != NULL)
        *depth = cells - 1u;
    return HPCLANG_RPL_STACK_OK;
}

int hpclang_rpl_stack_depth(const struct hpclang_rpl_reader *reader, uint32_t *depth)
{
    if (depth == NULL)
        return HPCLANG_RPL_STACK_UNAVAILABLE;

    *depth = 0u;
    return stack_bounds(reader, NULL, depth);
}

int hpclang_rpl_stack_object_address(const struct hpclang_rpl_reader *reader, uint32_t level,
                                     uint32_t *object_address)
{
    if (object_address == NULL)
        return HPCLANG_RPL_STACK_UNAVAILABLE;
    *object_address = 0u;

    if (level == 0u)
        return HPCLANG_RPL_STACK_LEVEL;

    uint32_t top = 0u;
    uint32_t depth = 0u;
    int result = stack_bounds(reader, &top, &depth);
    if (result != HPCLANG_RPL_STACK_OK)
        return result;
    if (level > depth)
        return HPCLANG_RPL_STACK_LEVEL;

    uint32_t zero_based = level - 1u;
    if (zero_based > UINT32_MAX / 5u)
        return HPCLANG_RPL_STACK_LEVEL;
    uint32_t offset = zero_based * 5u;

    uint32_t slot = 0u;
    if (!checked_add(top, offset, &slot) || slot > RPL_SATURN_MAX - 4u)
        return HPCLANG_RPL_STACK_MALFORMED;

    uint32_t object = 0u;
    result = read_word5(reader, slot, &object);
    if (result != HPCLANG_RPL_STACK_OK)
        return result;
    if (object > RPL_SATURN_MAX)
        return HPCLANG_RPL_STACK_MALFORMED;

    *object_address = object;
    return HPCLANG_RPL_STACK_OK;
}
