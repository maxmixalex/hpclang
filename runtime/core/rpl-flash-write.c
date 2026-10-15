#include "rpl-flash-write.h"
#include "rpl-backup.h"
#include "rpl-flash-record.h"
#include "rpl-port.h"
#include <string.h>

struct packed_record
{
    const uint8_t *data;
    uint32_t bytes;
};

static int packed_read(void *context, uint32_t address, uint8_t *value)
{
    const struct packed_record *record = context;
    if (address / 2u >= record->bytes)
        return 0;
    *value = (record->data[address / 2u] >> (4u * (address & 1u))) & 15u;
    return 1;
}

static int valid_record(const uint8_t *record, uint32_t bytes)
{
    if (!record || bytes < 8u || bytes > 0x1fefau || (bytes & 1u) || record[0] != 0x53u ||
        (record[3] >> 4u) != 1u)
        return 0;
    uint32_t extent = record[1] | (uint32_t)record[2] << 8u | (uint32_t)(record[3] & 15u) << 16u;
    if (extent != bytes * 2u - 2u)
        return 0;
    struct packed_record packed = {record, bytes};
    const struct hpclang_rpl_reader reader = {packed_read, &packed};
    struct hpclang_rpl_port_cursor cursor = {8u, bytes * 2u, 0};
    struct hpclang_rpl_port_entry view;
    if (hpclang_rpl_port_next(&reader, &cursor, &view) != 1 ||
        (((view.span + 15u) & ~7u) != bytes * 2u && ((view.span + 11u) & ~3u) != bytes * 2u))
        return 0;
    /* Accept old SDK halfword-sized envelopes for pending-write cleanup;
     * newly encoded records follow native STO's four-byte allocation size. */
    for (uint32_t at = 8u + view.span; at < bytes * 2u; ++at) {
        uint8_t nibble;
        if (!packed_read(&packed, at, &nibble) || nibble != 15u)
            return 0;
    }
    return 1;
}

static int write_bytes(const struct hpclang_rpl_flash_io *io, uint32_t address, const uint8_t *data,
                       uint32_t bytes)
{
    uint32_t end = address + bytes;
    for (uint32_t at = address & ~1u; at < end; at += 2u) {
        uint16_t old;
        if (!io->read16(io->context, at, &old))
            return 0;
        uint16_t value = old;
        for (uint32_t half = 0; half < 2; ++half) {
            if (at + half >= address && at + half < end) {
                uint32_t shift = half * 8u;
                value = (uint16_t)((value & ~(255u << shift)) | (uint32_t)data[at + half - address]
                                                                    << shift);
            }
        }
        if ((old & value) != value || (old != value && !io->program16(io->context, at, value)))
            return 0;
        if (!io->read16(io->context, at, &old) || old != value)
            return 0;
    }
    return 1;
}

static int discard_status(const struct hpclang_rpl_flash_io *io, uint32_t address)
{
    uint16_t word;
    if (!io->read16(io->context, address & ~1u, &word))
        return 0;
    uint8_t status = (uint8_t)(word >> ((address & 1u) * 8u));
    /* Clearing a hidden superset of 0x53 straight to zero can transiently
     * publish it. First clear one required live bit, leaving all other bits
     * unchanged, and verify it before clearing the rest. A power cut during
     * either step then cannot expose an uncommitted (possibly partial) body.
     * Some resulting hidden states still require explicit recovery evidence. */
    if (status != 0x53u && (status & 0x53u) == 0x53u) {
        uint8_t hidden = status & (uint8_t)~1u;
        if (!write_bytes(io, address, &hidden, 1u))
            return 0;
    }
    const uint8_t deleted = 0;
    return write_bytes(io, address, &deleted, 1u);
}

struct recovery_reader
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t first;
};

int hpclang_rpl_flash_finish_commit(const struct hpclang_rpl_flash_io *io, uint32_t address,
                                    const uint8_t *record, uint32_t bytes)
{
    if (!io || !io->read16 || !io->program16 || ((io->first | io->end) & 1u) ||
        address < io->first || address >= io->end || !valid_record(record, bytes) ||
        bytes >= io->end - address)
        return HPCLANG_RPL_FLASH_INVALID;
    for (uint32_t offset = 0; offset < bytes; ++offset) {
        uint16_t word;
        if (!io->read16(io->context, (address + offset) & ~1u, &word))
            return HPCLANG_RPL_FLASH_FAILED;
        uint8_t value = (uint8_t)(word >> (((address + offset) & 1u) * 8u));
        if (offset ? value != record[offset] : (value & record[0]) != record[0])
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return write_bytes(io, address, record, 1u) ? HPCLANG_RPL_FLASH_WRITTEN
                                                : HPCLANG_RPL_FLASH_DIRTY;
}

static int io_nibble(void *context, uint32_t address, uint8_t *value)
{
    const struct recovery_reader *reader = context;
    const struct hpclang_rpl_flash_io *io = reader->io;
    uint32_t byte = reader->first + address / 2u;
    uint16_t word;
    if (byte < io->first || byte >= io->end || !io->read16(io->context, byte & ~1u, &word))
        return 0;
    *value = (uint8_t)((word >> (((byte & 1u) * 2u + (address & 1u)) * 4u)) & 15u);
    return 1;
}

int hpclang_rpl_flash_recover_tail(const struct hpclang_rpl_flash_io *io, uint32_t first,
                                   uint32_t end)
{
    if (!io || !io->read16 || !io->program16 || ((io->first | io->end) & 1u) || first < io->first ||
        end > io->end || first >= end || end - first > 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    /* Native readers use 20-bit nibble coordinates. Translate physical Flash
     * addresses to a bounded local origin, preserving odd-byte alignment. */
    struct recovery_reader reader_io = {io, first};
    const struct hpclang_rpl_reader reader = {io_nibble, &reader_io};
    struct hpclang_rpl_flash_discard plan;
    int result = hpclang_rpl_flash_discard_plan(&reader, 0, (end - first) * 2u, &plan);
    if (result < 0)
        return HPCLANG_RPL_FLASH_DIRTY;
    if (!result)
        return HPCLANG_RPL_FLASH_WRITTEN;
    /* Zero metadata durably distinguishes a discarded incomplete body from
     * an ordinary backup (metadata one). Publish it with the verified extent
     * while FF still hides the tail, before changing any status bits. */
    uint8_t header[] = {(uint8_t)plan.extent, (uint8_t)(plan.extent >> 8u),
                        (uint8_t)(plan.extent >> 16u)};
    if (!write_bytes(io, first + plan.address / 2u + 1u, header, sizeof header) ||
        !discard_status(io, first + plan.address / 2u))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct hpclang_rpl_flash_space space;
    return hpclang_rpl_flash_tail(&reader, 0, (end - first) * 2u, &space)
               ? HPCLANG_RPL_FLASH_DIRTY
               : HPCLANG_RPL_FLASH_WRITTEN;
}

struct recovery_status
{
    const struct hpclang_rpl_reader *reader;
    uint32_t address;
    uint8_t status;
};

static int committed_status_nibble(void *context, uint32_t address, uint8_t *value)
{
    const struct recovery_status *view = context;
    if (address == view->address || address == view->address + 1u) {
        *value = (view->status >> (address == view->address ? 0u : 4u)) & 15u;
        return 1;
    }
    return view->reader->read_nibble(view->reader->context, address, value);
}

static int recovery_word(const struct hpclang_rpl_reader *reader, uint32_t address, unsigned width,
                         uint32_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, address + i, &nibble))
            return HPCLANG_RPL_FLASH_FAILED;
        if (nibble > 15u)
            return HPCLANG_RPL_FLASH_DIRTY;
        *value |= (uint32_t)nibble << (4u * i);
    }
    return 0;
}

static int recover_bank(const struct hpclang_rpl_flash_io *io, uint32_t bank, int apply)
{
    struct recovery_reader mapped = {io, bank};
    const struct hpclang_rpl_reader reader = {io_nibble, &mapped};
    uint32_t at = 0x20au;
    while (at <= 0x3fffeu) {
        uint32_t status, header;
        int result = recovery_word(&reader, at, 2u, &status);
        if (result)
            return result;
        if (status == 0xffu) {
            struct hpclang_rpl_flash_discard plan;
            result = hpclang_rpl_flash_discard_plan(&reader, at, 0x40000u, &plan);
            if (result < 0)
                return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                              : HPCLANG_RPL_FLASH_DIRTY;
            return apply ? hpclang_rpl_flash_recover_tail(io, bank + 0x105u, bank + 0x20000u)
                         : HPCLANG_RPL_FLASH_WRITTEN;
        }
        if (at > 0x3fff8u)
            return HPCLANG_RPL_FLASH_DIRTY;
        result = recovery_word(&reader, at + 2u, 6u, &header);
        if (result)
            return result;
        uint32_t extent = header & 0xfffffu;
        if (extent < 11u || extent > 0x40000u - at - 2u)
            return HPCLANG_RPL_FLASH_DIRTY;
        if (status && !(status & 1u) && !(header >> 20u)) {
            /* Tail cleanup verifies zero metadata and the extent before
             * clearing bit zero. Every subsequent status remains even.
             * Require the exact discard plan and an erased remainder; do not
             * interpret an incomplete body as a CRC-valid backup. */
            struct recovery_status view = {&reader, at, 0xffu};
            const struct hpclang_rpl_reader erased = {committed_status_nibble, &view};
            struct hpclang_rpl_flash_discard plan;
            result = hpclang_rpl_flash_discard_plan(&erased, at, 0x40000u, &plan);
            if (result != 1 || plan.address != at || plan.extent != extent)
                return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                              : HPCLANG_RPL_FLASH_DIRTY;
            if (apply && !discard_status(io, bank + at / 2u))
                return HPCLANG_RPL_FLASH_DIRTY;
            at += extent + 2u;
            continue;
        }
        if (status) {
            int partial = status != 0x53u;
            if (partial && (at & 1u))
                return HPCLANG_RPL_FLASH_DIRTY;
            /* Read-only status substitution lets the ordinary parser validate
             * the full object and CRC; it never publishes a live record. */
            struct recovery_status view = {&reader, at, 0x53u};
            const struct hpclang_rpl_reader committed = {committed_status_nibble, &view};
            struct hpclang_rpl_port_cursor cursor = {at, 0x40000u, 0};
            struct hpclang_rpl_port_entry entry;
            result = hpclang_rpl_flash_next(&committed, &cursor, &entry);
            if (result != 1)
                return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                              : HPCLANG_RPL_FLASH_DIRTY;
            if (partial) {
                if (entry.prologue != 0x02b62u || (header >> 20u) != 1u ||
                    (((entry.span + 15u) & ~7u) != extent + 2u &&
                     ((entry.span + 11u) & ~3u) != extent + 2u))
                    return HPCLANG_RPL_FLASH_DIRTY;
                for (uint32_t pad = at + 8u + entry.span; pad < at + extent + 2u; ++pad) {
                    uint32_t value;
                    result = recovery_word(&reader, pad, 1u, &value);
                    if (result)
                        return result;
                    if (value != 15u)
                        return HPCLANG_RPL_FLASH_DIRTY;
                }
                if (apply && !discard_status(io, bank + at / 2u))
                    return HPCLANG_RPL_FLASH_DIRTY;
            }
        }
        at += extent + 2u;
    }
    return HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_flash_recover_tails(const struct hpclang_rpl_flash_io *io)
{
    if (!io || !io->read16 || !io->program16 || ((io->first | io->end) & 1u) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    for (uint32_t index = 0; index < 6u; ++index) {
        uint32_t bank = io->first + index * 0x20000u;
        uint16_t identity;
        if (!io->read16(io->context, bank + 0x100u, &identity))
            return HPCLANG_RPL_FLASH_FAILED;
        if (identity != (0xf0u | ((index + 8u) << 8u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        int result = recover_bank(io, bank, 0);
        if (result)
            return result;
    }
    /* Re-read under the caller's stable mapping/ownership. No potentially
     * repairable early bank is changed before every later bank is checked. */
    for (uint32_t index = 0; index < 6u; ++index) {
        uint32_t bank = io->first + index * 0x20000u;
        int result = recover_bank(io, bank, 1);
        if (result)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}

struct record_selection
{
    uint32_t free_slot, match_slot, matches;
};

static int select_record(const struct hpclang_rpl_flash_io *io, const uint8_t *name,
                         uint32_t name_length, uint32_t prologue, uint32_t library_number,
                         uint32_t bytes, struct record_selection *selection)
{
    if (!io || !io->read16 || !io->program16 || ((io->first | io->end) & 1u) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u || (!name && name_length) ||
        name_length > 255u)
        return HPCLANG_RPL_FLASH_INVALID;
    *selection = (struct record_selection){io->end, io->end, 0};
    for (uint32_t index = 0; index < 6u; ++index) {
        uint32_t bank = io->first + index * 0x20000u;
        uint16_t identity;
        if (!io->read16(io->context, bank + 0x100u, &identity))
            return HPCLANG_RPL_FLASH_FAILED;
        if (identity != (0xf0u | ((index + 8u) << 8u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        struct recovery_reader mapped = {io, bank};
        const struct hpclang_rpl_reader reader = {io_nibble, &mapped};
        struct hpclang_rpl_port_cursor cursor = {0x20au, 0x40000u, 0};
        struct hpclang_rpl_port_entry entry;
        int result;
        while ((result = hpclang_rpl_flash_next(&reader, &cursor, &entry)) > 0)
            if (entry.prologue == prologue &&
                (prologue == 0x02b40u
                     ? entry.library_number == library_number
                     : entry.name_length == name_length &&
                           (!name_length || !memcmp(entry.name, name, name_length)))) {
                selection->match_slot = bank + entry.address / 2u - 4u;
                ++selection->matches;
            }
        if (result)
            return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                          : HPCLANG_RPL_FLASH_DIRTY;
        struct hpclang_rpl_flash_space space;
        result = hpclang_rpl_flash_tail(&reader, cursor.address, cursor.end, &space);
        if (result)
            return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                          : HPCLANG_RPL_FLASH_DIRTY;
        if (selection->free_slot == io->end && space.nibbles >= bytes * 2u)
            selection->free_slot = bank + space.address / 2u;
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}

static int select_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *name,
                         uint32_t name_length, uint32_t bytes, struct record_selection *selection)
{
    return select_record(io, name, name_length, 0x02b62u, 0, bytes, selection);
}

static int create_record(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                         uint32_t bytes, uint32_t prologue, uint32_t *attempted_slot)
{
    if (!valid_record(record, bytes))
        return HPCLANG_RPL_FLASH_INVALID;
    struct packed_record packed = {record, bytes};
    const struct hpclang_rpl_reader source = {packed_read, &packed};
    struct hpclang_rpl_port_cursor source_cursor = {8u, bytes * 2u, 0};
    struct hpclang_rpl_port_entry wanted;
    if (hpclang_rpl_port_next(&source, &source_cursor, &wanted) != 1 || wanted.prologue != prologue)
        return HPCLANG_RPL_FLASH_INVALID;
    struct record_selection selection;
    int result = select_record(io, wanted.name, wanted.name_length, wanted.prologue,
                               wanted.library_number, bytes, &selection);
    if (result)
        return result;
    if (selection.matches)
        return HPCLANG_RPL_FLASH_EXISTS;
    uint32_t selected = selection.free_slot;
    if (selected == io->end)
        return HPCLANG_RPL_FLASH_NOSPACE;
    struct hpclang_rpl_flash_io reserved = *io;
    uint32_t bank = io->first + ((selected - io->first) / 0x20000u) * 0x20000u;
    reserved.first = bank + 0x104u;
    reserved.end = bank + 0x20000u;
    if (attempted_slot)
        *attempted_slot = selected;
    return hpclang_rpl_flash_append(&reserved, selected, record, bytes);
}

int hpclang_rpl_flash_create_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                    uint32_t bytes, uint32_t *attempted_slot)
{
    return create_record(io, record, bytes, 0x02b62u, attempted_slot);
}

int hpclang_rpl_flash_create_library(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                     uint32_t bytes, uint32_t *attempted_slot)
{
    return create_record(io, record, bytes, 0x02b40u, attempted_slot);
}

int hpclang_rpl_flash_finish_remove(const struct hpclang_rpl_flash_io *io, uint32_t address)
{
    if (!io || !io->read16 || !io->program16 || ((io->first | io->end) & 1u) ||
        address < io->first || address >= io->end)
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t word;
    if (!io->read16(io->context, address & ~1u, &word))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint8_t status = (uint8_t)(word >> ((address & 1u) * 8u));
    if (status & (uint8_t)~0x53u)
        return HPCLANG_RPL_FLASH_DIRTY;
    const uint8_t deleted = 0;
    return write_bytes(io, address, &deleted, 1u) ? HPCLANG_RPL_FLASH_WRITTEN
                                                  : HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_flash_replace_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                     uint32_t bytes,
                                     struct hpclang_rpl_flash_replacement *transaction)
{
    if (!transaction || !valid_record(record, bytes))
        return HPCLANG_RPL_FLASH_INVALID;
    struct packed_record packed = {record, bytes};
    const struct hpclang_rpl_reader source = {packed_read, &packed};
    struct hpclang_rpl_port_cursor cursor = {8u, bytes * 2u, 0};
    struct hpclang_rpl_port_entry wanted;
    if (hpclang_rpl_port_next(&source, &cursor, &wanted) != 1 || wanted.prologue != 0x02b62u)
        return HPCLANG_RPL_FLASH_INVALID;
    struct record_selection selection;
    int result = select_backup(io, wanted.name, wanted.name_length, bytes, &selection);
    if (result)
        return result;
    if (!selection.matches)
        return HPCLANG_RPL_FLASH_NOT_FOUND;
    if (selection.matches != 1u)
        return HPCLANG_RPL_FLASH_DIRTY;
    if (selection.free_slot == io->end)
        return HPCLANG_RPL_FLASH_NOSPACE;
    struct hpclang_rpl_flash_io reserved = *io;
    uint32_t bank = io->first + ((selection.free_slot - io->first) / 0x20000u) * 0x20000u;
    reserved.first = bank + 0x104u;
    reserved.end = bank + 0x20000u;
    *transaction =
        (struct hpclang_rpl_flash_replacement){selection.match_slot, selection.free_slot, 0};
    result = hpclang_rpl_flash_append(&reserved, transaction->new_slot, record, bytes);
    if (result)
        return result;
    transaction->committed = 1;
    return hpclang_rpl_flash_finish_remove(io, transaction->old_slot);
}

int hpclang_rpl_flash_remove_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *name,
                                    uint32_t name_length, uint32_t *attempted_slot)
{
    struct record_selection selection;
    int result = select_backup(io, name, name_length, 0, &selection);
    if (result)
        return result;
    if (!selection.matches)
        return HPCLANG_RPL_FLASH_NOT_FOUND;
    if (selection.matches != 1u)
        return HPCLANG_RPL_FLASH_DIRTY;
    if (attempted_slot)
        *attempted_slot = selection.match_slot;
    return hpclang_rpl_flash_finish_remove(io, selection.match_slot);
}

int hpclang_rpl_flash_discard_record(const struct hpclang_rpl_flash_io *io, uint32_t address,
                                     const uint8_t *record, uint32_t bytes)
{
    if (!io || !io->read16 || !io->program16 || ((io->first | io->end) & 1u) ||
        address < io->first || address >= io->end || !valid_record(record, bytes) ||
        bytes >= io->end - address)
        return HPCLANG_RPL_FLASH_INVALID;
    if (!write_bytes(io, address + 1u, record + 1u, 3u) || !discard_status(io, address))
        return HPCLANG_RPL_FLASH_DIRTY;
    return HPCLANG_RPL_FLASH_WRITTEN;
}

int hpclang_rpl_flash_append(const struct hpclang_rpl_flash_io *io, uint32_t address,
                             const uint8_t *record, uint32_t bytes)
{
    if (!io || !io->read16 || !io->program16 || ((io->first | io->end) & 1u) ||
        address < io->first || address >= io->end || !valid_record(record, bytes) ||
        bytes >= io->end - address)
        return HPCLANG_RPL_FLASH_INVALID;
    /* Include the next erased status byte; preserve each shared halfword's
     * neighboring byte when the allocation starts at an odd address. */
    for (uint32_t offset = 0; offset <= bytes; ++offset) {
        uint16_t value;
        if (!io->read16(io->context, (address + offset) & ~1u, &value))
            return HPCLANG_RPL_FLASH_FAILED;
        if (((value >> (((address + offset) & 1u) * 8u)) & 255u) != 255u)
            return HPCLANG_RPL_FLASH_OCCUPIED;
    }
    if (write_bytes(io, address + 1u, record + 1u, bytes - 1u) &&
        write_bytes(io, address, record, 1u))
        return HPCLANG_RPL_FLASH_WRITTEN;

    /* The reserved extent is known even if its original programming failed.
     * Finish it before publishing a native tombstone. This also discards a
     * status write that completed but whose acknowledgement/readback failed. */
    if (hpclang_rpl_flash_discard_record(io, address, record, bytes))
        return HPCLANG_RPL_FLASH_DIRTY;
    return HPCLANG_RPL_FLASH_FAILED;
}
