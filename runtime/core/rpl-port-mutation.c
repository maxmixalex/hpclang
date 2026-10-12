#include <stddef.h>
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
    uint32_t start = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!io->read_nibble(io->context, 0x80716u + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        if (nibble > 15)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        start |= (uint32_t)nibble << (4u * i);
    }
    if (start < 0x80000u || start > 0xffffbu)
        return HPCLANG_RPL_MUTATION_MALFORMED;
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
        uint8_t nibble;
        read_packed(&source, i, &nibble);
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
