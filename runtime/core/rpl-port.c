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
