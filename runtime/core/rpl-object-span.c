#include <stddef.h>
#include <stdint.h>

#include "rpl-object-span.h"

enum {
    RPL_DOBINT    = 0x02911u,
    RPL_DOREAL    = 0x02933u,
    RPL_DOEREL    = 0x02955u,
    RPL_DOCMP     = 0x02977u,
    RPL_DOECMP    = 0x0299du,
    RPL_DOCHAR    = 0x029bfu,
    RPL_DOARRY    = 0x029e8u,
    RPL_DOLNKARRY = 0x02a0au,
    RPL_DOCSTR    = 0x02a2cu,
    RPL_DOHSTR    = 0x02a4eu,
    RPL_DOLIST    = 0x02a74u,
    RPL_DORRP     = 0x02a96u,
    RPL_DOSYMB    = 0x02ab8u,
    RPL_DOEXT     = 0x02adau,
    RPL_DOTAG     = 0x02afcu,
    RPL_DOGROB    = 0x02b1eu,
    RPL_DOLIB     = 0x02b40u,
    RPL_DOBAK     = 0x02b62u,
    RPL_DOEXT0    = 0x02b88u,
    RPL_DOACPTR   = 0x02baau,
    RPL_DOEXT2    = 0x02bccu,
    RPL_DOEXT3    = 0x02beeu,
    RPL_DOEXT4    = 0x02c10u,
    RPL_DOCOL     = 0x02d9du,
    RPL_DOCODE    = 0x02dccu,
    RPL_DOIDNT    = 0x02e48u,
    RPL_DOLAM     = 0x02e6du,
    RPL_DOROMP    = 0x02e92u,
    RPL_SEMI      = 0x0312bu
};

enum {
    RPL_MAX_RECURSION = 32u
};

static int checked_add(uint32_t a, uint32_t b, uint32_t *out)
{
    if (out == NULL || a > UINT32_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int read_nibbles(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t count,
    uint32_t limit,
    uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL || value == NULL)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (count > 8u || count > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t result = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        uint32_t current;
        if (!checked_add(address, i, &current))
            return HPCLANG_RPL_SPAN_BOUNDS;

        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, current, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 0x0fu)
            return HPCLANG_RPL_SPAN_MALFORMED;

        result |= (uint32_t)nibble << (i * 4u);
    }

    *value = result;
    return HPCLANG_RPL_SPAN_OK;
}

static int fixed_span(uint32_t size, uint32_t limit, uint32_t *span)
{
    if (size > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *span = size;
    return HPCLANG_RPL_SPAN_OK;
}

static int length_prefixed_span(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    uint32_t *span)
{
    if (limit < 10u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t length = 0u;
    int result = read_nibbles(reader, address + 5u, 5u, limit - 5u, &length);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    /*
     * HP RPL length fields count from the length field itself to the end and
     * therefore exclude the five-nibble prologue. Even an empty variable-size
     * object needs the five-nibble length field.
     */
    if (length < 5u)
        return HPCLANG_RPL_SPAN_MALFORMED;

    uint32_t total;
    if (!checked_add(5u, length, &total) || total > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;

    *span = total;
    return HPCLANG_RPL_SPAN_OK;
}

static int name_span(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    uint32_t *span)
{
    if (limit < 7u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t characters = 0u;
    int result = read_nibbles(reader, address + 5u, 2u, limit - 5u, &characters);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    uint32_t payload;
    if (characters > (UINT32_MAX - 7u) / 2u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    payload = characters * 2u;

    uint32_t total;
    if (!checked_add(7u, payload, &total) || total > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;

    *span = total;
    return HPCLANG_RPL_SPAN_OK;
}

static int parse_object(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    uint32_t depth,
    uint32_t *span);

static int composite_span(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    uint32_t depth,
    uint32_t *span)
{
    if (depth >= RPL_MAX_RECURSION)
        return HPCLANG_RPL_SPAN_DEPTH;
    if (limit < 10u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t offset = 5u;
    for (;;) {
        if (limit - offset < 5u)
            return HPCLANG_RPL_SPAN_BOUNDS;

        uint32_t word = 0u;
        int result = read_nibbles(
            reader, address + offset, 5u, limit - offset, &word);
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;

        if (word == RPL_SEMI) {
            *span = offset + 5u;
            return HPCLANG_RPL_SPAN_OK;
        }

        uint32_t nested = 0u;
        result = parse_object(
            reader, address + offset, limit - offset, depth + 1u, &nested);
        if (result == HPCLANG_RPL_SPAN_UNSUPPORTED) {
            /*
             * Runstreams also contain five-nibble ROM/object references. A
             * word that is not a recognized inline object prologue is one
             * reference cell and is skipped as such.
             */
            offset += 5u;
            continue;
        }
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;
        if (nested == 0u || nested > limit - offset)
            return HPCLANG_RPL_SPAN_MALFORMED;

        offset += nested;
    }
}

static int parse_object(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    uint32_t depth,
    uint32_t *span)
{
    if (span == NULL)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (limit < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t prologue = 0u;
    int result = read_nibbles(reader, address, 5u, limit, &prologue);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    switch (prologue) {
    case RPL_DOBINT:
        return fixed_span(10u, limit, span);
    case RPL_DOREAL:
        return fixed_span(21u, limit, span);
    case RPL_DOEREL:
        return fixed_span(26u, limit, span);
    case RPL_DOCMP:
        return fixed_span(37u, limit, span);
    case RPL_DOECMP:
        return fixed_span(47u, limit, span);
    case RPL_DOCHAR:
        return fixed_span(7u, limit, span);
    case RPL_DOROMP:
        return fixed_span(11u, limit, span);

    case RPL_DOARRY:
    case RPL_DOLNKARRY:
    case RPL_DOCSTR:
    case RPL_DOHSTR:
    case RPL_DOGROB:
    case RPL_DOLIB:
    case RPL_DOBAK:
    case RPL_DOEXT0:
    case RPL_DOEXT2:
    case RPL_DOEXT3:
    case RPL_DOEXT4:
    case RPL_DOCODE:
        return length_prefixed_span(reader, address, limit, span);

    case RPL_DOIDNT:
    case RPL_DOLAM:
        return name_span(reader, address, limit, span);

    case RPL_DOLIST:
    case RPL_DOSYMB:
    case RPL_DOEXT:
    case RPL_DOCOL:
        return composite_span(reader, address, limit, depth, span);

    /*
     * Recognized prologues whose internal boundary is not yet frozen are
     * intentionally rejected rather than guessed.
     */
    case RPL_DORRP:
    case RPL_DOTAG:
    case RPL_DOACPTR:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;

    default:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
}

int hpclang_rpl_object_span(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    uint32_t *span)
{
    if (span == NULL)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    *span = 0u;
    return parse_object(reader, address, limit, 0u, span);
}
