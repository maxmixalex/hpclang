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

static int same_record(const struct hpclang_rpl_port_entry *a,
                       const struct hpclang_rpl_port_entry *b)
{
    return a->prologue == b->prologue &&
           (a->prologue == 0x02b40u
                ? a->library_number == b->library_number
                : a->name_length == b->name_length && !memcmp(a->name, b->name, a->name_length));
}

static int port1_append(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                        const uint8_t *packed, uint32_t span, uint32_t prologue)
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
    if (result != 1 || added.prologue != prologue || added.span != span)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {0, 0x40000u, 0};
    struct hpclang_rpl_port_entry existing;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &existing)) > 0) {
        if (same_record(&existing, &added))
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
            return HPCLANG_RPL_MUTATION_INCOMPLETE;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_port1_create_backup(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                    const uint8_t *packed, uint32_t span)
{
    return port1_append(io, limit, packed, span, 0x02b62u);
}

int hpclang_rpl_port1_create_library(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                     const uint8_t *packed, uint32_t span)
{
    return port1_append(io, limit, packed, span, 0x02b40u);
}

static int port1_edit(const struct hpclang_rpl_mutation_io *io, uint32_t limit, const uint8_t *name,
                      uint32_t name_length, const uint8_t *packed, uint32_t span,
                      const struct hpclang_rpl_port1_guard *guard)
{
    if (!io || !io->read_nibble || !io->write_nibble || !guard || !guard->check)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (limit > 0x40000u || span > 0xfffffu || (!name && name_length) || name_length > 255)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    struct hpclang_rpl_port_entry replacement;
    if (packed) {
        struct packed_source source = {packed, span};
        const struct hpclang_rpl_reader input = {read_packed, &source};
        struct hpclang_rpl_port_cursor cursor = {.end = span};
        if (hpclang_rpl_port_next(&input, &cursor, &replacement) != 1 ||
            replacement.prologue != 0x02b62u || replacement.span != span)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        name = replacement.name;
        name_length = replacement.name_length;
    }
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_port_cursor cursor = {0, 0x40000u, 0};
    struct hpclang_rpl_port_entry entry, target = {0};
    uint32_t last_library = 0;
    int result;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &entry)) > 0) {
        if (entry.prologue == 0x02b40u)
            last_library = entry.address;
        else if (entry.name_length == name_length &&
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
    if (span != target.span && last_library > target.address)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    uint32_t old_end = cursor.address;
    uint32_t new_end = old_end - target.span + span;
    if (old_end > limit || limit - old_end < 6u || new_end > limit || limit - new_end < 6u)
        return HPCLANG_RPL_MUTATION_NOSPACE;
    uint32_t affected_end = (old_end > new_end ? old_end : new_end) + 6u;
    for (uint32_t address = target.address; address < affected_end; ++address) {
        uint8_t nibble;
        if (!io->read_nibble(io->context, address, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        if (nibble > 15u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
    }
    uint32_t from = target.address + target.span;
    uint32_t to = target.address + span;
    /* Same-size replacement invalidates references to the old record only;
     * a size change also moves every following record, up to the terminator. */
    result = guard->check(guard->context, target.address, to == from ? from : old_end);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    uint32_t tail = old_end - from + 6u;
    if (to != from) {
        for (uint32_t n = 0; n < tail; ++n) {
            uint32_t offset = to > from ? tail - n - 1u : n;
            uint8_t nibble;
            if (!io->read_nibble(io->context, from + offset, &nibble) ||
                !io->write_nibble(io->context, to + offset, nibble))
                return HPCLANG_RPL_MUTATION_INCOMPLETE;
        }
    }
    for (uint32_t n = 0; n < span; ++n) {
        uint8_t nibble = (packed[n / 2u] >> (4u * (n & 1u))) & 15u;
        if (!io->write_nibble(io->context, target.address + n, nibble))
            return HPCLANG_RPL_MUTATION_INCOMPLETE;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_port1_replace_backup(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                     const uint8_t *packed, uint32_t span,
                                     const struct hpclang_rpl_port1_guard *guard)
{
    if (!packed)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    return port1_edit(io, limit, NULL, 0, packed, span, guard);
}

int hpclang_rpl_port1_remove_backup(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                    const uint8_t *name, uint32_t name_length,
                                    const struct hpclang_rpl_port1_guard *guard)
{
    return port1_edit(io, limit, name, name_length, NULL, 0, guard);
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
        if (same_record(&existing, &added))
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

int hpclang_rpl_catalog_remove_repaired(const struct hpclang_rpl_mutation_io *io, uint32_t number,
                                        const struct hpclang_rpl_catalog_removal *expected,
                                        const struct hpclang_rpl_catalog_repairs *repairs)
{
    if (!io || !io->read_nibble || !io->write_nibble || !expected)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (io->library_table < 0x80000 || io->library_table > 0xffffd)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_catalog_removal plan;
    int result = hpclang_rpl_catalog_plan_remove(&reader, io->library_table,
                                                 0x100000 - io->library_table, number, &plan);
    if (result != 1)
        return result ? parse_error(result) : HPCLANG_RPL_MUTATION_NOT_FOUND;
    if (plan.slot != expected->slot || plan.end != expected->end || plan.count != expected->count ||
        plan.binding.address != expected->binding.address ||
        plan.binding.bank_switch != expected->binding.bank_switch ||
        plan.binding.flags != expected->binding.flags)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t home = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!io->read_nibble(io->context, 0x80711u + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        if (nibble > 15)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        home |= (uint32_t)nibble << (4 * i);
    }
    result = hpclang_rpl_home_library_attached(&reader, home, 0x100000u - home, number);
    if (result)
        return result == 1 ? HPCLANG_RPL_MUTATION_BUSY : parse_error(result);
    result = hpclang_rpl_mutation_catalog_shrink_repaired(io, plan.slot, repairs);
    if (result)
        return result;
    for (unsigned nibble = 0; nibble < 3; ++nibble)
        if (!io->write_nibble(io->context, io->library_table + nibble,
                              ((plan.count - 1) >> (4 * nibble)) & 15))
            return HPCLANG_RPL_MUTATION_INCOMPLETE;
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_catalog_insert_repaired(const struct hpclang_rpl_mutation_io *io, uint32_t number,
                                        const struct hpclang_rpl_library_binding *binding,
                                        const struct hpclang_rpl_catalog_insertion *expected,
                                        const struct hpclang_rpl_catalog_repairs *repairs)
{
    if (!io || !io->read_nibble || !io->write_nibble || !binding || !expected)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    const struct hpclang_rpl_library_binding added = *binding;
    if (io->library_table < 0x80000 || io->library_table > 0xffffd || !added.address ||
        added.address > 0xfffff || added.bank_switch > 0xfffff || added.flags > 0xfff)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_catalog_insertion plan;
    int result = hpclang_rpl_catalog_plan_insert(&reader, io->library_table,
                                                 0x100000 - io->library_table, number, &plan);
    if (result != 1)
        return result ? parse_error(result) : HPCLANG_RPL_MUTATION_EXISTS;
    if (plan.slot != expected->slot || plan.end != expected->end || plan.count != expected->count)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    result = hpclang_rpl_mutation_catalog_grow_repaired(io, plan.slot, repairs);
    if (result)
        return result;
    const uint32_t values[] = {number, added.address, added.bank_switch, added.flags,
                               plan.count + 1};
    const uint32_t widths[] = {3, 5, 5, 3, 3};
    uint32_t at = plan.slot;
    for (unsigned field = 0; field < 5; ++field) {
        if (field == 4)
            at = io->library_table;
        for (unsigned nibble = 0; nibble < widths[field]; ++nibble)
            if (!io->write_nibble(io->context, at++, (values[field] >> (4 * nibble)) & 15))
                return HPCLANG_RPL_MUTATION_INCOMPLETE;
    }
    return HPCLANG_RPL_MUTATION_OK;
}
