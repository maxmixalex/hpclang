#include <stddef.h>
#include <stdint.h>

#include "rpl-object-span.h"

enum
{
    RPL_DOINT = 0x02614u,
    RPL_DOLNGREAL = 0x0263au,
    RPL_DOLNGCMP = 0x02660u,
    RPL_DOMATRIX = 0x02686u,
    RPL_DOFLASHP = 0x026acu,
    RPL_DOAPLET = 0x026d5u,
    RPL_DOMINIFONT = 0x026feu,
    RPL_DOBINT = 0x02911u,
    RPL_DOREAL = 0x02933u,
    RPL_DOEREL = 0x02955u,
    RPL_DOCMP = 0x02977u,
    RPL_DOECMP = 0x0299du,
    RPL_DOCHAR = 0x029bfu,
    RPL_DOARRY = 0x029e8u,
    RPL_DOLNKARRY = 0x02a0au,
    RPL_DOCSTR = 0x02a2cu,
    RPL_DOHSTR = 0x02a4eu,
    RPL_DOLIST = 0x02a74u,
    RPL_DORRP = 0x02a96u,
    RPL_DOSYMB = 0x02ab8u,
    RPL_DOEXT = 0x02adau,
    RPL_DOTAG = 0x02afcu,
    RPL_DOGROB = 0x02b1eu,
    RPL_DOLIB = 0x02b40u,
    RPL_DOBAK = 0x02b62u,
    RPL_DOEXT0 = 0x02b88u,
    RPL_DOACPTR = 0x02baau,
    RPL_DOEXT2 = 0x02bccu,
    RPL_DOEXT3 = 0x02beeu,
    RPL_DOEXT4 = 0x02c10u,
    RPL_DOCOL = 0x02d9du,
    RPL_DOCODE = 0x02dccu,
    RPL_DOIDNT = 0x02e48u,
    RPL_DOLAM = 0x02e6du,
    RPL_DOROMP = 0x02e92u,
    RPL_SEMI = 0x0312bu
};

enum
{
    RPL_MAX_RECURSION = 32u,
    RPL_USEROB = 0x00080711u
};

static int checked_add(uint32_t a, uint32_t b, uint32_t *out)
{
    if (out == NULL || a > UINT32_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int address_at_offset(uint32_t address, uint32_t offset, uint32_t limit, uint32_t *current,
                             uint32_t *remaining)
{
    if (offset > limit || !checked_add(address, offset, current))
        return 0;
    if (remaining != NULL)
        *remaining = limit - offset;
    return 1;
}

static int read_nibbles(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t count,
                        uint32_t limit, uint32_t *value)
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

static int length_prefixed_span(const struct hpclang_rpl_reader *reader, uint32_t address,
                                uint32_t limit, uint32_t *span)
{
    if (limit < 10u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t field_address = 0u;
    uint32_t field_limit = 0u;
    if (!address_at_offset(address, 5u, limit, &field_address, &field_limit))
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t length = 0u;
    int result = read_nibbles(reader, field_address, 5u, field_limit, &length);
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

static int name_span(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t limit,
                     uint32_t *span)
{
    if (limit < 7u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t field_address = 0u;
    uint32_t field_limit = 0u;
    if (!address_at_offset(address, 5u, limit, &field_address, &field_limit))
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t characters = 0u;
    int result = read_nibbles(reader, field_address, 2u, field_limit, &characters);
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

static int parse_object(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t limit,
                        uint32_t depth, uint32_t *span);

static int long_number_span(const struct hpclang_rpl_reader *reader, uint32_t address,
                            uint32_t limit, uint32_t component_count, uint32_t *span)
{
    uint32_t offset = 5u;

    for (uint32_t component = 0u; component < component_count; ++component) {
        uint32_t current = 0u;
        uint32_t remaining = 0u;
        if (!address_at_offset(address, offset, limit, &current, &remaining) || remaining < 5u)
            return HPCLANG_RPL_SPAN_BOUNDS;

        uint32_t component_size = 0u;
        int result = read_nibbles(reader, current, 5u, remaining, &component_size);
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;

        /*
         * Each infinite-integer component carries its own five-nibble size
         * field plus at least the sign nibble.
         */
        if (component_size < 6u)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (component_size > remaining || !checked_add(offset, component_size, &offset))
            return HPCLANG_RPL_SPAN_BOUNDS;
    }

    if (offset > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *span = offset;
    return HPCLANG_RPL_SPAN_OK;
}

static int tagged_span(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t limit,
                       uint32_t depth, uint32_t *span)
{
    if (depth >= RPL_MAX_RECURSION)
        return HPCLANG_RPL_SPAN_DEPTH;
    if (limit < 7u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t tag_address = 0u;
    uint32_t tag_limit = 0u;
    if (!address_at_offset(address, 5u, limit, &tag_address, &tag_limit))
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t characters = 0u;
    int result = read_nibbles(reader, tag_address, 2u, tag_limit, &characters);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    if (characters > (UINT32_MAX - 7u) / 2u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t object_offset = 7u + characters * 2u;
    uint32_t object_address = 0u;
    uint32_t object_limit = 0u;
    if (!address_at_offset(address, object_offset, limit, &object_address, &object_limit) ||
        object_limit < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t nested = 0u;
    result = parse_object(reader, object_address, object_limit, depth + 1u, &nested);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    if (!checked_add(object_offset, nested, span) || *span > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;
    return HPCLANG_RPL_SPAN_OK;
}

/*
 * Classic RPL directories do not carry a length field. HPGCC's
 * sat_objgetdir()/sat_dirfindfirst()/sat_sknameup()/sat_skipob() establish the
 * boundary: locate the newest item from the directory chain, then the end of
 * that item's stored object is the end of the whole DORRP.
 *
 * HOME alone has an attached-library prefix. The null-name encoding is the
 * native layout used by HP39's Hidden directory and by the generic store
 * walker: [len=0][object], with no duplicate trailing length field.
 */
static int directory_span(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t limit,
                          uint32_t depth, uint32_t *span)
{
    if (depth >= RPL_MAX_RECURSION)
        return HPCLANG_RPL_SPAN_DEPTH;
    if (limit < 13u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t home = 0u;
    int result = read_nibbles(reader, RPL_USEROB, 5u, 5u, &home);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    uint32_t chain_offset = 5u;
    if (address == home) {
        uint32_t count_address = 0u;
        uint32_t count_limit = 0u;
        if (!address_at_offset(address, chain_offset, limit, &count_address, &count_limit) ||
            count_limit < 3u)
            return HPCLANG_RPL_SPAN_BOUNDS;

        uint32_t libraries = 0u;
        result = read_nibbles(reader, count_address, 3u, count_limit, &libraries);
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;
        if (libraries > (UINT32_MAX - chain_offset) / 13u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        chain_offset += libraries * 13u;
    }

    if (!checked_add(chain_offset, 3u, &chain_offset) || chain_offset > limit ||
        limit - chain_offset < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t chain_address = 0u;
    uint32_t chain_limit = 0u;
    if (!address_at_offset(address, chain_offset, limit, &chain_address, &chain_limit))
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t newest_offset = 0u;
    result = read_nibbles(reader, chain_address, 5u, chain_limit, &newest_offset);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    if (newest_offset == 0u) {
        if (!checked_add(chain_offset, 5u, span) || *span > limit)
            return HPCLANG_RPL_SPAN_BOUNDS;
        return HPCLANG_RPL_SPAN_OK;
    }

    uint32_t item_offset = 0u;
    if (!checked_add(chain_offset, newest_offset, &item_offset) || item_offset > limit ||
        limit - item_offset < 2u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t item_address = 0u;
    uint32_t item_limit = 0u;
    if (!address_at_offset(address, item_offset, limit, &item_address, &item_limit))
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t name_length = 0u;
    result = read_nibbles(reader, item_address, 2u, item_limit, &name_length);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;

    uint32_t object_offset = 0u;
    if (name_length == 0u) {
        if (!checked_add(item_offset, 2u, &object_offset))
            return HPCLANG_RPL_SPAN_BOUNDS;
    } else {
        if (name_length > (UINT32_MAX - 4u) / 2u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t item_object_offset = 4u + name_length * 2u;
        if (!checked_add(item_offset, item_object_offset, &object_offset))
            return HPCLANG_RPL_SPAN_BOUNDS;

        if (object_offset > limit || object_offset < 2u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t trailing_address = 0u;
        uint32_t trailing_limit = 0u;
        if (!address_at_offset(address, object_offset - 2u, limit, &trailing_address,
                               &trailing_limit) ||
            trailing_limit < 2u)
            return HPCLANG_RPL_SPAN_BOUNDS;

        uint32_t trailing_length = 0u;
        result = read_nibbles(reader, trailing_address, 2u, trailing_limit, &trailing_length);
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;
        if (trailing_length != name_length)
            return HPCLANG_RPL_SPAN_MALFORMED;
    }

    if (object_offset > limit || limit - object_offset < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t object_address = 0u;
    uint32_t object_limit = 0u;
    if (!address_at_offset(address, object_offset, limit, &object_address, &object_limit))
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t nested = 0u;
    result = parse_object(reader, object_address, object_limit, depth + 1u, &nested);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    if (nested == 0u || nested > object_limit)
        return HPCLANG_RPL_SPAN_MALFORMED;

    if (!checked_add(object_offset, nested, span) || *span > limit)
        return HPCLANG_RPL_SPAN_BOUNDS;
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_object_prologue_known(uint32_t prologue)
{
    switch (prologue) {
    case RPL_DOINT:
    case RPL_DOLNGREAL:
    case RPL_DOLNGCMP:
    case RPL_DOMATRIX:
    case RPL_DOFLASHP:
    case RPL_DOAPLET:
    case RPL_DOMINIFONT:
    case RPL_DOBINT:
    case RPL_DOREAL:
    case RPL_DOEREL:
    case RPL_DOCMP:
    case RPL_DOECMP:
    case RPL_DOCHAR:
    case RPL_DOARRY:
    case RPL_DOLNKARRY:
    case RPL_DOCSTR:
    case RPL_DOHSTR:
    case RPL_DOLIST:
    case RPL_DORRP:
    case RPL_DOSYMB:
    case RPL_DOEXT:
    case RPL_DOTAG:
    case RPL_DOGROB:
    case RPL_DOLIB:
    case RPL_DOBAK:
    case RPL_DOEXT0:
    case RPL_DOACPTR:
    case RPL_DOEXT2:
    case RPL_DOEXT3:
    case RPL_DOEXT4:
    case RPL_DOCOL:
    case RPL_DOCODE:
    case RPL_DOIDNT:
    case RPL_DOLAM:
    case RPL_DOROMP:
        return 1;
    default:
        return 0;
    }
}

static int composite_span(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t limit,
                          uint32_t depth, uint32_t *span)
{
    if (depth >= RPL_MAX_RECURSION)
        return HPCLANG_RPL_SPAN_DEPTH;
    if (limit < 10u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t offset = 5u;
    for (;;) {
        uint32_t current = 0u;
        uint32_t remaining = 0u;
        if (!address_at_offset(address, offset, limit, &current, &remaining) || remaining < 5u)
            return HPCLANG_RPL_SPAN_BOUNDS;

        uint32_t word = 0u;
        int result = read_nibbles(reader, current, 5u, remaining, &word);
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;

        if (word == RPL_SEMI) {
            if (!checked_add(offset, 5u, span) || *span > limit)
                return HPCLANG_RPL_SPAN_BOUNDS;
            return HPCLANG_RPL_SPAN_OK;
        }

        /*
         * Runstreams contain both inline objects and five-nibble references.
         * Only a word that is not any recognized object prologue may be
         * treated as a reference cell. A known-but-unfrozen prologue must
         * stop parsing rather than silently desynchronize the stream.
         */
        if (!hpclang_rpl_object_prologue_known(word)) {
            if (!checked_add(offset, 5u, &offset) || offset > limit)
                return HPCLANG_RPL_SPAN_BOUNDS;
            continue;
        }

        uint32_t nested = 0u;
        result = parse_object(reader, current, remaining, depth + 1u, &nested);
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;
        if (nested == 0u || nested > remaining)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (!checked_add(offset, nested, &offset) || offset > limit)
            return HPCLANG_RPL_SPAN_BOUNDS;
    }
}

static int parse_object(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t limit,
                        uint32_t depth, uint32_t *span)
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
    case RPL_DOFLASHP:
        return fixed_span(12u, limit, span);
    case RPL_DOACPTR:
        return fixed_span(15u, limit, span);
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

    case RPL_DOINT:
    case RPL_DOARRY:
    case RPL_DOLNKARRY:
    case RPL_DOCSTR:
    case RPL_DOHSTR:
    case RPL_DOGROB:
    case RPL_DOLIB:
    case RPL_DOBAK:
    case RPL_DOEXT0:
    case RPL_DOEXT2:
    case RPL_DOCODE:
    case RPL_DOMINIFONT:
        return length_prefixed_span(reader, address, limit, span);

    case RPL_DOLNGREAL:
        return long_number_span(reader, address, limit, 2u, span);
    case RPL_DOLNGCMP:
        return long_number_span(reader, address, limit, 4u, span);

    case RPL_DOIDNT:
    case RPL_DOLAM:
        return name_span(reader, address, limit, span);

    case RPL_DOLIST:
    case RPL_DOSYMB:
    case RPL_DOEXT:
    case RPL_DOCOL:
    case RPL_DOMATRIX:
        return composite_span(reader, address, limit, depth, span);

    case RPL_DOTAG:
        return tagged_span(reader, address, limit, depth, span);

    case RPL_DORRP:
        return directory_span(reader, address, limit, depth, span);

    /*
     * Recognized prologues whose internal boundary is not yet frozen are
     * intentionally rejected rather than guessed.
     */
    case RPL_DOAPLET:
    case RPL_DOEXT3:
    case RPL_DOEXT4:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;

    default:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
}

int hpclang_rpl_object_span(const struct hpclang_rpl_reader *reader, uint32_t address,
                            uint32_t limit, uint32_t *span)
{
    if (span == NULL)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    *span = 0u;
    return parse_object(reader, address, limit, 0u, span);
}
