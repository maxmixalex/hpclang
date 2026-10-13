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

int hpclang_rpl_port1_create_backup(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                    const uint8_t *packed, uint32_t span)
{
    if (!io || !io->read_nibble || !io->write_nibble)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (!packed || span > 0xfffffu || limit > 0x40000u)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    struct packed_source source = {packed, span};
    const struct hpclang_rpl_reader input = {read_packed, &source};
    struct hpclang_rpl_port_cursor input_cursor = {.end = span};
    struct hpclang_rpl_port_entry added;
    int result = hpclang_rpl_port_next(&input, &input_cursor, &added);
    if (result != 1 || added.prologue != 0x02b62u || added.span != span)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {0, 0x40000u, 0};
    struct hpclang_rpl_port_entry existing;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &existing)) > 0) {
        if (existing.prologue == added.prologue && existing.name_length == added.name_length &&
            !memcmp(existing.name, added.name, added.name_length))
            return HPCLANG_RPL_MUTATION_EXISTS;
    }
    if (result < 0)
        return parse_error(result);
    uint32_t start = cursor.address;
    /* Keep the six-nibble terminator/padding expected by the native allocator. */
    if (start > limit || limit - start < span + 6u)
        return HPCLANG_RPL_MUTATION_NOSPACE;
    for (uint32_t i = 0; i < span + 6u; ++i) {
        uint8_t nibble;
        if (!io->read_nibble(io->context, start + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        if (nibble > 15u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
    }
    /* Prepare the body and next terminator while the current terminator still
     * hides the append. The stable-mapping commit contract matches Port 0. */
    for (uint32_t i = 5; i < span + 6u; ++i) {
        uint8_t nibble = i < span ? (packed[i / 2u] >> (4u * (i & 1u))) & 15u : 0;
        if (!io->write_nibble(io->context, start + i, nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    for (uint32_t i = 0; i < 5; ++i) {
        uint8_t nibble = (packed[i / 2u] >> (4u * (i & 1u))) & 15u;
        if (!io->write_nibble(io->context, start + i, nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    return HPCLANG_RPL_MUTATION_OK;
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

int hpclang_rpl_port0_shift_backups(const struct hpclang_rpl_mutation_io *io, uint32_t position,
                                    int32_t delta)
{
    if (!io || !io->read_nibble || !io->write_nibble)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    uint32_t start;
    int result = port_start(io, &start);
    if (result)
        return result;
    if (!delta || position < start || position > 0xffffbu)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t removed = delta < 0 ? (uint32_t)(-(int64_t)delta) : 0;
    if (removed > position - start)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t move_end = position - removed;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {start, 0x100000u, 0};
    struct hpclang_rpl_port_entry entry;
    int position_found = position == start;
    int move_end_found = move_end == start;
    int library_affected = 0;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &entry)) > 0) {
        position_found |= entry.address + entry.span == position;
        move_end_found |= entry.address + entry.span == move_end;
        if (entry.prologue == 0x02b40u && entry.address < position)
            library_affected = 1;
    }
    if (result < 0)
        return parse_error(result);
    if (!position_found || !move_end_found)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    if (library_affected)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    return hpclang_rpl_mutation_port0_shift_before(io, position, delta);
}

int hpclang_rpl_port0_remove_backup(const struct hpclang_rpl_mutation_io *io, const uint8_t *name,
                                    uint32_t name_length)
{
    if (!io || !io->read_nibble || !io->write_nibble)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if ((!name && name_length) || name_length > 255)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t start;
    int result = port_start(io, &start);
    if (result)
        return result;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {start, 0x100000u, 0};
    struct hpclang_rpl_port_entry entry, target = {0};
    uint32_t first_library = 0x100000u;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &entry)) > 0) {
        if (entry.prologue == 0x02b40u && first_library == 0x100000u)
            first_library = entry.address;
        if (entry.prologue == 0x02b62u && entry.name_length == name_length &&
            (!name_length || !memcmp(entry.name, name, name_length))) {
            if (target.span)
                return HPCLANG_RPL_MUTATION_MALFORMED;
            target = entry;
        }
    }
    if (result < 0)
        return parse_error(result);
    if (!target.span)
        return HPCLANG_RPL_MUTATION_NOT_FOUND;
    if (first_library < target.address)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    return hpclang_rpl_mutation_port0_remove_record(io, target.address, target.span);
}

static int prepend_record(const struct hpclang_rpl_mutation_io *io, const uint8_t *packed,
                          uint32_t span, uint32_t prologue)
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
    if (result != 1 || added.prologue != prologue || added.span != span)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t start;
    result = port_start(io, &start);
    if (result)
        return result;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {start, 0x100000u, 0};
    struct hpclang_rpl_port_entry existing;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &existing)) > 0) {
        if (existing.prologue == added.prologue &&
            (prologue == 0x02b40u ? existing.library_number == added.library_number
                                  : existing.name_length == added.name_length &&
                                        !memcmp(existing.name, added.name, added.name_length)))
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
    /* Publish only after the complete record is present. Existing port objects
     * and installed library addresses have not moved. */
    for (uint32_t i = 0; i < 5; ++i)
        if (!io->write_nibble(io->context, 0x80716u + i,
                              (uint8_t)((destination >> (4u * i)) & 15u)))
            return HPCLANG_RPL_MUTATION_IO;
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_port0_create_backup(const struct hpclang_rpl_mutation_io *io, const uint8_t *packed,
                                    uint32_t span)
{
    return prepend_record(io, packed, span, 0x02b62u);
}

int hpclang_rpl_port0_create_library(const struct hpclang_rpl_mutation_io *io,
                                     const uint8_t *packed, uint32_t span)
{
    return prepend_record(io, packed, span, 0x02b40u);
}

int hpclang_rpl_port0_replace_backup(const struct hpclang_rpl_mutation_io *io,
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
    uint32_t first_library = 0x100000u;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &existing)) > 0) {
        if (existing.prologue == 0x02b40u && first_library == 0x100000u)
            first_library = existing.address;
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
    if (target.span != span && first_library < target.address)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    return hpclang_rpl_mutation_port0_replace_record(io, target.address, target.span, packed, span);
}
