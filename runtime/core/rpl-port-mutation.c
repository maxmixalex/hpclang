#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "rpl-object-store.h"
#include "rpl-port.h"
#include "rpl-port-mutation.h"

struct packed_source
{
    const uint8_t *data;
    uint32_t span;
};

static int read_packed(void *context, uint32_t address, uint8_t *value)
{
    const struct packed_source *source = context;
    if (address >= source->span)
        return 0;
    *value = (source->data[address / 2u] >> ((address & 1u) * 4u)) & 15u;
    return 1;
}

static int parse_error(int result)
{
    return result == HPCLANG_RPL_SPAN_UNAVAILABLE   ? HPCLANG_RPL_MUTATION_IO
           : result == HPCLANG_RPL_SPAN_UNSUPPORTED ? HPCLANG_RPL_MUTATION_UNSUPPORTED
                                                    : HPCLANG_RPL_MUTATION_MALFORMED;
}

static int port_start(const struct hpclang_rpl_mutation_io *io, uint32_t *start)
{
    *start = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!io->read_nibble(io->context, 0x80716u + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        if (nibble > 15)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        *start |= (uint32_t)nibble << (4u * i);
    }
    if (*start < 0x80000u || *start > 0xffffbu)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_port0_create_backup(const struct hpclang_rpl_mutation_io *io, const uint8_t *packed,
                                    uint32_t span)
{
    if (!io || !io->read_nibble || !io->write_nibble)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (!packed || span > 0xfffffu)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    struct packed_source source = {packed, span};
    const struct hpclang_rpl_reader input = {read_packed, &source};
    struct hpclang_rpl_port_cursor input_cursor = {.end = span};
    struct hpclang_rpl_port_entry added;
    int result = hpclang_rpl_port_next(&input, &input_cursor, &added);
    if (result != 1 || added.prologue != 0x02b62u || added.span != span)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t start;
    result = port_start(io, &start);
    if (result)
        return result;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {start, 0x100000u, 0};
    struct hpclang_rpl_port_entry existing;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &existing)) > 0) {
        if (existing.prologue == added.prologue && existing.name_length == added.name_length &&
            !memcmp(existing.name, added.name, added.name_length))
            return HPCLANG_RPL_MUTATION_EXISTS;
    }
    if (result < 0)
        return parse_error(result);
    uint32_t home, home_span;
    if (hpclang_rpl_store_home_object(&reader, &home) != HPCLANG_RPL_STORE_OK)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    result = hpclang_rpl_object_span(&reader, home, 0x100000u - home, &home_span);
    if (result)
        return parse_error(result);
    if (home + home_span != start)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    if (span > start - 0x80000u)
        return HPCLANG_RPL_MUTATION_NOSPACE;
    uint32_t destination = start - span;
    for (uint32_t i = 0; i < span; ++i) {
        uint8_t nibble;
        if (!io->read_nibble(io->context, destination + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    result = hpclang_rpl_mutation_userob_shift_before(io, start, (int32_t)span);
    if (result)
        return result;
    for (uint32_t i = 0; i < span; ++i) {
        uint8_t nibble = (packed[i / 2u] >> ((i & 1u) * 4u)) & 15u;
        if (!io->write_nibble(io->context, destination + i, nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    /* Publish only after the complete backup is present. Existing port objects
     * and installed library addresses have not moved. */
    for (uint32_t i = 0; i < 5; ++i)
        if (!io->write_nibble(io->context, 0x80716u + i,
                              (uint8_t)((destination >> (4u * i)) & 15u)))
            return HPCLANG_RPL_MUTATION_IO;
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_port0_overwrite_backup(const struct hpclang_rpl_mutation_io *io,
                                       const uint8_t *packed, uint32_t span)
{
    if (!io || !io->read_nibble || !io->write_nibble)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (!packed || span > 0xfffffu)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    struct packed_source source = {packed, span};
    const struct hpclang_rpl_reader input = {read_packed, &source};
    struct hpclang_rpl_port_cursor input_cursor = {.end = span};
    struct hpclang_rpl_port_entry replacement;
    int result = hpclang_rpl_port_next(&input, &input_cursor, &replacement);
    if (result != 1 || replacement.prologue != 0x02b62u || replacement.span != span)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t start;
    result = port_start(io, &start);
    if (result)
        return result;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {start, 0x100000u, 0};
    struct hpclang_rpl_port_entry existing, target = {0};
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &existing)) > 0) {
        if (existing.prologue == replacement.prologue &&
            existing.name_length == replacement.name_length &&
            !memcmp(existing.name, replacement.name, replacement.name_length)) {
            if (target.span)
                return HPCLANG_RPL_MUTATION_MALFORMED;
            target = existing;
        }
    }
    if (result < 0)
        return parse_error(result);
    if (!target.span)
        return HPCLANG_RPL_MUTATION_NOT_FOUND;
    if (target.span != span)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    struct hpclang_object *value = NULL;
    if (hpclang_object_snapshot_copy_encoded(&input, replacement.value_address,
                                             replacement.value_span, &value))
        return errno == ENOMEM ? HPCLANG_RPL_MUTATION_NOMEM : HPCLANG_RPL_MUTATION_MALFORMED;
    result = hpclang_rpl_mutation_overwrite_value(io, target.value_address, value);
    free(value);
    if (result)
        return result;
    /* Names and extents are identical; only the contained value and CRC change.
     * Full port validation above proved these checksum bytes readable before
     * reference preservation or the first write. */
    for (uint32_t i = span - 5u; i < span; ++i) {
        uint8_t nibble = (packed[i / 2u] >> ((i & 1u) * 4u)) & 15u;
        if (!io->write_nibble(io->context, target.address + i, nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    return HPCLANG_RPL_MUTATION_OK;
}
