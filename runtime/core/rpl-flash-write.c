#include "rpl-flash-write.h"
#include "rpl-backup.h"
#include "rpl-flash-record.h"

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
    struct hpclang_rpl_backup_view view;
    if (hpclang_rpl_backup_view(&reader, 8u, bytes * 2u - 8u, &view) ||
        ((view.span + 11u) & ~3u) != bytes * 2u)
        return 0;
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

struct recovery_reader
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t first;
};

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
    uint8_t metadata;
    if (!io_nibble(&reader_io, plan.address + 7u, &metadata))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint8_t header[] = {(uint8_t)plan.extent, (uint8_t)(plan.extent >> 8u),
                        (uint8_t)((plan.extent >> 16u) | (uint32_t)metadata << 4u)};
    const uint8_t deleted = 0;
    if (!write_bytes(io, first + plan.address / 2u + 1u, header, sizeof header) ||
        !write_bytes(io, first + plan.address / 2u, &deleted, 1u))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct hpclang_rpl_flash_space space;
    return hpclang_rpl_flash_tail(&reader, 0, (end - first) * 2u, &space)
               ? HPCLANG_RPL_FLASH_DIRTY
               : HPCLANG_RPL_FLASH_WRITTEN;
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
    const uint8_t deleted = 0;
    if (!write_bytes(io, address + 1u, record + 1u, 3u) || !write_bytes(io, address, &deleted, 1u))
        return HPCLANG_RPL_FLASH_DIRTY;
    return HPCLANG_RPL_FLASH_FAILED;
}
