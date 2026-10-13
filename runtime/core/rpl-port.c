#include <stddef.h>
#include "rpl-backup.h"
#include "rpl-port.h"

static int word(const struct hpclang_rpl_reader *reader, uint32_t at, uint32_t count,
                uint32_t *value)
{
    uint32_t result = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, at + i, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result |= (uint32_t)nibble << (4 * i);
    }
    *value = result;
    return 0;
}

int hpclang_rpl_library_lookup(const struct hpclang_rpl_reader *reader, uint32_t address,
                               uint32_t limit, uint32_t number,
                               struct hpclang_rpl_library_binding *binding)
{
    if (!reader || !reader->read_nibble || !binding)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 3)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfff)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t count;
    int result = word(reader, address, 3, &count);
    if (result)
        return result;
    if (count > (limit - 3) / 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    int found = 0;
    struct hpclang_rpl_library_binding next = {0};
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = address + 3 + 16 * i, id;
        result = word(reader, record, 3, &id);
        if (result)
            return result;
        if (id != number)
            continue;
        if (found)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result = word(reader, record + 3, 5, &next.address);
        if (result)
            return result;
        result = word(reader, record + 8, 5, &next.bank_switch);
        if (result)
            return result;
        result = word(reader, record + 13, 3, &next.flags);
        if (result)
            return result;
        found = 1;
    }
    if (found)
        *binding = next;
    return found;
}

static int library(const struct hpclang_rpl_reader *reader, uint32_t limit,
                   struct hpclang_rpl_port_entry *entry, uint32_t *name_address)
{
    int result = hpclang_rpl_object_span(reader, entry->address, limit, &entry->span);
    if (result)
        return result;
    /* Length, title prefix/suffix, library ID, four relative table offsets, CRC. */
    if (entry->span < 41)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, entry->address + 10, 2, &entry->name_length);
    if (result)
        return result;
    if (entry->name_length * 2 > entry->span - 41)
        return HPCLANG_RPL_SPAN_MALFORMED;
    *name_address = entry->address + 12;
    uint32_t repeated;
    result = word(reader, *name_address + entry->name_length * 2, 2, &repeated);
    if (result)
        return result;
    if (repeated != entry->name_length)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, *name_address + entry->name_length * 2 + 2, 3, &entry->library_number);
    if (result)
        return result;
    uint32_t crc = 0;
    for (uint32_t offset = 5; offset < entry->span - 4; ++offset) {
        uint32_t nibble;
        result = word(reader, entry->address + offset, 1, &nibble);
        if (result)
            return result;
        crc = (crc >> 4) ^ (((crc ^ nibble) & 15) * 0x1081);
    }
    uint32_t stored_crc;
    result = word(reader, entry->address + entry->span - 4, 4, &stored_crc);
    if (result)
        return result;
    if (stored_crc != crc)
        return HPCLANG_RPL_SPAN_MALFORMED;
    entry->value_address = entry->address;
    entry->value_span = entry->span;
    return 0;
}

static int library_target(const struct hpclang_rpl_reader *reader, uint32_t field, uint32_t first,
                          uint32_t end, uint32_t *target)
{
    uint32_t relative;
    int result = word(reader, field, 5, &relative);
    if (result)
        return result;
    if (!relative) {
        *target = 0;
        return 0;
    }
    uint32_t resolved = (field + relative) & 0xfffffu;
    if (resolved < first || resolved >= end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *target = resolved;
    return 0;
}

int hpclang_rpl_library_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t limit, hpclang_rpl_library_visit_fn visit, void *context)
{
    if (!reader || !reader->read_nibble || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue;
    int result = word(reader, address, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x2b40u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_port_entry entry = {.address = address};
    uint32_t name;
    result = library(reader, limit, &entry, &name);
    if (result)
        return result;
    uint32_t fields = name + entry.name_length * 2 + 5;
    uint32_t first = fields + 20, end = address + entry.span - 4;
    uint32_t targets[4];
    for (unsigned i = 0; i < 4; ++i) {
        result = library_target(reader, fields + i * 5, first, end, &targets[i]);
        if (result)
            return result;
    }
    uint32_t links_end = 0;
    if (targets[2]) {
        uint32_t type, span;
        if (end - targets[2] < 10)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = word(reader, targets[2], 5, &type);
        if (result)
            return result;
        if (type != 0x2a4eu)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        result = hpclang_rpl_object_span(reader, targets[2], end - targets[2], &span);
        if (result)
            return result;
        if ((span - 10) % 5)
            return HPCLANG_RPL_SPAN_MALFORMED;
        links_end = targets[2] + span;
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (targets[i]) {
            result = visit(context, (enum hpclang_rpl_library_reference_kind)i, fields + i * 5,
                           targets[i], end - targets[i]);
            if (result)
                return result;
        }
    }
    for (uint32_t field = targets[2] ? targets[2] + 10 : 0; field < links_end; field += 5) {
        uint32_t target;
        result = library_target(reader, field, first, end, &target);
        if (result)
            return result;
        if (target) {
            result = visit(context, HPCLANG_RPL_LIBRARY_FUNCTION, field, target, end - target);
            if (result)
                return result;
        }
    }
    return 0;
}

int hpclang_rpl_port_next(const struct hpclang_rpl_reader *reader,
                          struct hpclang_rpl_port_cursor *cursor,
                          struct hpclang_rpl_port_entry *entry)
{
    if (!reader || !reader->read_nibble || !cursor || !entry)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (cursor->end > 0x100000 || cursor->address > cursor->end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (cursor->finished)
        return 0;
    uint32_t limit = cursor->end - cursor->address;
    if (limit < 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_port_entry next = {.address = cursor->address};
    int result = word(reader, next.address, 5, &next.prologue);
    if (result)
        return result;
    if (!next.prologue) {
        cursor->finished = 1;
        return 0;
    }
    uint32_t name_address;
    if (next.prologue == 0x02b62) {
        struct hpclang_rpl_backup_view backup;
        result = hpclang_rpl_backup_view(reader, next.address, limit, &backup);
        if (result)
            return result;
        next.span = backup.span;
        next.value_address = backup.value_address;
        next.value_span = backup.value_span;
        next.name_length = backup.name_length;
        name_address = backup.name_address;
    } else if (next.prologue == 0x02b40) {
        result = library(reader, limit, &next, &name_address);
        if (result)
            return result;
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    for (uint32_t i = 0; i < next.name_length; ++i) {
        uint32_t byte;
        result = word(reader, name_address + 2 * i, 2, &byte);
        if (result)
            return result;
        next.name[i] = (uint8_t)byte;
    }
    cursor->address += next.span;
    *entry = next;
    return 1;
}

int hpclang_rpl_flash_next(const struct hpclang_rpl_reader *reader,
                           struct hpclang_rpl_port_cursor *cursor,
                           struct hpclang_rpl_port_entry *entry)
{
    if (!reader || !reader->read_nibble || !cursor || !entry)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (cursor->end > 0x100000u || cursor->address > cursor->end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (cursor->finished)
        return 0;
    struct hpclang_rpl_port_cursor scan = *cursor;
    for (;;) {
        uint32_t available = scan.end - scan.address;
        if (available < 2u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t status;
        int result = word(reader, scan.address, 2u, &status);
        if (result)
            return result;
        if (status == 0xffu) {
            scan.finished = 1;
            *cursor = scan;
            return 0;
        }
        if (status != 0x53u && status != 0u)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (available < 8u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t extent;
        result = word(reader, scan.address + 2u, 5u, &extent);
        if (result)
            return result;
        /* Extent is relative to its own field, not the slot or object prologue.
         * The final metadata nibble is not part of this size. */
        if (extent < 11u)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (extent > available - 2u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        if (status == 0u) {
            /* Native PURGE changes the status byte; payload is no longer live. */
            scan.address += 2u + extent;
            continue;
        }
        struct hpclang_rpl_port_cursor object = {scan.address + 8u, scan.address + 2u + extent, 0};
        struct hpclang_rpl_port_entry next;
        result = hpclang_rpl_port_next(reader, &object, &next);
        if (result != 1)
            return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        scan.address += 2u + extent;
        *cursor = scan;
        *entry = next;
        return 1;
    }
}
