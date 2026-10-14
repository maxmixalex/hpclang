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

int hpclang_rpl_library_command(const struct hpclang_rpl_reader *reader, uint32_t first,
                                uint32_t end, uint32_t header, uint32_t number, uint32_t command,
                                uint32_t *target)
{
    if (!reader || !reader->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (end > 0x100000u || first >= end || header < first || header > end || end - header < 23)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfff || command > 0xfff)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t id, relative;
    int result = word(reader, header, 3, &id);
    if (result)
        return result;
    if (id != number)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, header + 13, 5, &relative);
    if (result || !relative)
        return result;
    uint32_t table = (header + 13 + relative) & 0xfffffu;
    if (table < first || table > end || end - table < 10)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue, length;
    result = word(reader, table, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x02a4e)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    result = word(reader, table + 5, 5, &length);
    if (result)
        return result;
    if (length < 5 || (length - 5) % 5)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (length > end - table - 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (command >= (length - 5) / 5)
        return 0;
    uint32_t field = table + 10 + 5 * command;
    result = word(reader, field, 5, &relative);
    if (result || !relative)
        return result;
    uint32_t resolved = (field + relative) & 0xfffffu;
    if (resolved < first || resolved >= end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *target = resolved;
    return 1;
}

int hpclang_rpl_bank_lookup(const struct hpclang_rpl_reader *reader, uint32_t flash_table,
                            uint32_t eram_entry, uint32_t bank_switch,
                            struct hpclang_rpl_bank_binding *binding)
{
    if (!reader || !reader->read_nibble || !binding)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (flash_table > 0x100000u - 112 || eram_entry > 0x100000u - 5 || bank_switch >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    int found = 0;
    struct hpclang_rpl_bank_binding next = {0};
    for (uint32_t i = 0; i < 17; ++i) {
        uint32_t value;
        int result = word(reader, i < 16 ? flash_table + 7 * i : eram_entry, 5, &value);
        if (result)
            return result;
        if (!value)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (value == bank_switch) {
            if (found)
                return HPCLANG_RPL_SPAN_MALFORMED;
            next.kind = i < 16 ? HPCLANG_RPL_BANK_FLASH : HPCLANG_RPL_BANK_ERAM;
            next.index = i < 16 ? i : 0;
            found = 1;
        }
    }
    if (found)
        *binding = next;
    return found;
}

int hpclang_rpl_acptr_resolve(const struct hpclang_rpl_reader *source, uint32_t address,
                              uint32_t limit, const struct hpclang_rpl_reader *rom,
                              uint32_t flash_table, uint32_t eram_entry,
                              struct hpclang_rpl_banked_target *target)
{
    if (!source || !source->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 15u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue, pointer, bank_switch;
    int result = word(source, address, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x02baau)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    result = word(source, address + 5u, 5, &pointer);
    if (result)
        return result;
    if (pointer < 0x40000u || pointer >= 0x80000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = word(source, address + 10u, 5, &bank_switch);
    if (result)
        return result;
    struct hpclang_rpl_banked_target next;
    result = hpclang_rpl_bank_lookup(rom, flash_table, eram_entry, bank_switch, &next.bank);
    if (result != 1)
        return result;
    next.offset = pointer - 0x40000u;
    *target = next;
    return 1;
}

int hpclang_rpl_flashptr_decode(const struct hpclang_rpl_reader *source, uint32_t address,
                                uint32_t limit, struct hpclang_rpl_flashptr *pointer)
{
    if (!source || !source->read_nibble || !pointer)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 12u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue;
    int result = word(source, address, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x026acu)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_flashptr next;
    result = word(source, address + 5u, 3, &next.selector);
    if (result)
        return result;
    result = word(source, address + 8u, 4, &next.index);
    if (result)
        return result;
    *pointer = next;
    return 1;
}

int hpclang_rpl_flashptr_lookup(const struct hpclang_rpl_reader *mapped, uint32_t header,
                                uint32_t bank, uint32_t index, uint32_t *target)
{
    if (!mapped || !mapped->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (header < 0x40000u || header > 0x80000u - 34u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (bank >= 16u || index > 0xffffu)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t identity, count;
    int result = word(mapped, header, 4, &identity);
    if (result)
        return result;
    if (identity != (0x0fu | (bank << 8u)))
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(mapped, header + 29u, 5, &count);
    if (result)
        return result;
    if (count > 0x10000u || count > (0x80000u - header - 34u) / 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (index >= count)
        return 0;
    uint32_t value;
    result = word(mapped, header + 34u + 5u * index, 5, &value);
    if (result || !value)
        return result;
    if (value >= 0x80000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *target = value;
    return 1;
}

int hpclang_rpl_flashptr_bank(const struct hpclang_rpl_reader *reader, uint32_t selectors,
                              uint32_t banks, uint32_t selector, uint32_t *bank)
{
    if (!reader || !reader->read_nibble || !bank)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (selectors > 0x100000u - 80u || banks > 0x100000u - 80u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (selector >= 16u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t configuration;
    int result = word(reader, selectors + 5u * selector, 5, &configuration);
    if (result || !configuration)
        return result;
    uint32_t selected = 0;
    int found = 0;
    for (uint32_t i = 0; i < 16u; ++i) {
        uint32_t candidate;
        result = word(reader, banks + 5u * i, 5, &candidate);
        if (result)
            return result;
        if (candidate == configuration) {
            if (found)
                return HPCLANG_RPL_SPAN_MALFORMED;
            selected = i;
            found = 1;
        }
    }
    if (found)
        *bank = selected;
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
