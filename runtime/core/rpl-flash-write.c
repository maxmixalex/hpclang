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

static int publish_erased(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                          const uint8_t *record, uint32_t bytes)
{
    uint16_t word = 0;
    for (uint32_t i = 0; i <= bytes; ++i) {
        uint32_t at = slot + i;
        if ((!i || !(at & 1u)) && !io->read16(io->context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_FAILED;
        if (((word >> (8u * (at & 1u))) & 255u) != 255u)
            return HPCLANG_RPL_FLASH_OCCUPIED;
    }
    if (!write_bytes(io, slot + 1u, record + 1u, bytes - 1u) || !write_bytes(io, slot, record, 1u))
        return HPCLANG_RPL_FLASH_DIRTY;
    return HPCLANG_RPL_FLASH_WRITTEN;
}

static int creation_intent(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                           const uint8_t *record, uint32_t bytes, int retry)
{
    enum hpclang_rpl_flash_creation_phase phase;
    if (!io || !io->read16 || !io->program16 || !record ||
        bytes != HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES ||
        hpclang_rpl_flash_creation_inspect(io->first, io->end, slot, record, bytes, record + 20,
                                           record + 100, record[17], &phase) ||
        phase != HPCLANG_RPL_FLASH_CREATION_INTENT)
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t word;
    if (!io->read16(io->context, (slot + 16u) & ~1u, &word))
        return retry ? HPCLANG_RPL_FLASH_DIRTY : HPCLANG_RPL_FLASH_FAILED;
    uint8_t current = (uint8_t)(word >> (8u * ((slot + 16u) & 1u)));
    if (retry && current != 0xffu && current != 0x7fu)
        return HPCLANG_RPL_FLASH_DIRTY;
    for (uint32_t i = 0; i <= bytes; ++i) {
        uint32_t at = slot + i;
        if (!io->read16(io->context, at & ~1u, &word))
            return retry ? HPCLANG_RPL_FLASH_DIRTY : HPCLANG_RPL_FLASH_FAILED;
        uint8_t value = (uint8_t)(word >> (8u * (at & 1u)));
        int matches = !retry || i == bytes ? value == 0xffu
                      : i == 16            ? value == current
                      : current == 0xffu   ? (value & record[i]) == record[i]
                                           : value == record[i];
        if (!matches)
            return retry ? HPCLANG_RPL_FLASH_DIRTY : HPCLANG_RPL_FLASH_OCCUPIED;
    }
    if (current == 0x7fu)
        return 0;
    /* Publish the native deleted envelope only after its immutable body, then
     * publish intent last. No partial result may return to Stock or create RAM. */
    if (!write_bytes(io, slot + 1u, record + 1u, 15u) ||
        !write_bytes(io, slot + 17u, record + 17u, bytes - 17u) ||
        !write_bytes(io, slot, record, 1u) || !write_bytes(io, slot + 16u, record + 16u, 1u))
        return HPCLANG_RPL_FLASH_DIRTY;
    return 0;
}

int hpclang_rpl_flash_publish_creation_intent(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                              const uint8_t *record, uint32_t bytes)
{
    return creation_intent(io, slot, record, bytes, 0);
}

int hpclang_rpl_flash_finish_creation_intent(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                             const uint8_t *record, uint32_t bytes)
{
    return creation_intent(io, slot, record, bytes, 1);
}

static int transition_creation(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                               const uint8_t *intent, uint32_t bytes, int closing)
{
    enum hpclang_rpl_flash_creation_phase expected_phase;
    if (!io || !io->read16 || !io->program16 || !intent ||
        bytes != HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES ||
        hpclang_rpl_flash_creation_inspect(io->first, io->end, slot, intent, bytes, intent + 20,
                                           intent + 100, intent[17], &expected_phase) ||
        expected_phase != HPCLANG_RPL_FLASH_CREATION_INTENT)
        return HPCLANG_RPL_FLASH_INVALID;
    uint8_t phase = 0xff;
    for (uint32_t i = 0; i < bytes; ++i) {
        uint32_t at = slot + i;
        uint16_t word;
        if (!io->read16(io->context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        uint8_t value = (uint8_t)(word >> (8u * (at & 1u)));
        if (i == 16) {
            if (closing ? (value & ~0x7eu) != 0 : value != 0x7fu && value != 0x7eu)
                return HPCLANG_RPL_FLASH_DIRTY;
            phase = value;
        } else if (value != intent[i]) {
            return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    const uint8_t target = closing ? 0 : 0x7e;
    return phase == target || write_bytes(io, slot + 16u, &target, 1u) ? HPCLANG_RPL_FLASH_WRITTEN
                                                                       : HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_flash_complete_creation(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                        const uint8_t *intent, uint32_t bytes)
{
    return transition_creation(io, slot, intent, bytes, 0);
}

int hpclang_rpl_flash_close_creation(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                     const uint8_t *intent, uint32_t bytes)
{
    return transition_creation(io, slot, intent, bytes, 1);
}

int hpclang_rpl_flash_publish_sector(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                     const uint8_t *record, uint32_t bytes)
{
    uint32_t target;
    const uint8_t *image;
    if (!io || !io->read16 || !io->program16 ||
        hpclang_rpl_flash_sector_decode(io->first, io->end, slot, record, bytes, &target, &image))
        return HPCLANG_RPL_FLASH_INVALID;
    return publish_erased(io, slot, record, bytes);
}

/* Compare a complete durable binding, with a separately constrained status.
 * Byte zero is excluded from body identity, never from phase validation. */
static int matching_record(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                           const uint8_t *record, uint32_t bytes, int retiring,
                           uint8_t required_status)
{
    uint16_t word = 0;
    for (uint32_t i = 0; i < bytes; ++i) {
        uint32_t at = slot + i;
        if ((!i || !(at & 1u)) && !io->read16(io->context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_FAILED;
        uint8_t value = (uint8_t)(word >> (8u * (at & 1u)));
        if (i ? value != record[i]
              : (retiring ? (value & 0x7eu) != value : value != required_status))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}

static int publish_authority_status(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                    uint32_t journal_slot, const uint8_t *journal,
                                    uint32_t journal_bytes, uint8_t status)
{
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    if (!io || !io->read16 || !io->program16 ||
        hpclang_rpl_flash_authority_encode(io->first, io->end, slot, journal_slot, journal,
                                           journal_bytes, authority, sizeof authority))
        return HPCLANG_RPL_FLASH_INVALID;
    int result = matching_record(io, journal_slot, journal, journal_bytes, 0, 0x7e);
    if (result != HPCLANG_RPL_FLASH_WRITTEN)
        return result;
    authority[0] = status;
    return publish_erased(io, slot, authority, sizeof authority);
}

int hpclang_rpl_flash_publish_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                        uint32_t journal_slot, const uint8_t *journal,
                                        uint32_t journal_bytes)
{
    return publish_authority_status(io, slot, journal_slot, journal, journal_bytes, 0x7eu);
}

int hpclang_rpl_flash_stage_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                      uint32_t journal_slot, const uint8_t *journal,
                                      uint32_t journal_bytes)
{
    return publish_authority_status(io, slot, journal_slot, journal, journal_bytes, 0x7fu);
}

static int finish_hidden_record(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                const uint8_t *record, uint32_t bytes)
{
    uint16_t word = 0;
    uint8_t status = 0;
    for (uint32_t i = 0; i <= bytes; ++i) {
        uint32_t at = slot + i;
        if ((!i || !(at & 1u)) && !io->read16(io->context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        uint8_t value = (uint8_t)(word >> (8u * (at & 1u)));
        if (!i) {
            status = value;
            if (status != 0xffu && status != 0x7fu &&
                (record[0] != 0x7eu || (status != 0xfeu && status != 0x7eu)))
                return HPCLANG_RPL_FLASH_DIRTY;
        } else if (i == bytes) {
            if (value != 0xffu)
                return HPCLANG_RPL_FLASH_DIRTY;
        } else if (status == 0xffu ? (value & record[i]) != record[i] : value != record[i]) {
            return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    if (status == record[0])
        return HPCLANG_RPL_FLASH_WRITTEN;
    return write_bytes(io, slot + 1u, record + 1u, bytes - 1u) && write_bytes(io, slot, record, 1u)
               ? HPCLANG_RPL_FLASH_WRITTEN
               : HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_flash_finish_sector(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                    const uint8_t *record, uint32_t bytes)
{
    uint32_t target;
    const uint8_t *image;
    if (!io || !io->read16 || !io->program16 ||
        hpclang_rpl_flash_sector_decode(io->first, io->end, slot, record, bytes, &target, &image))
        return HPCLANG_RPL_FLASH_INVALID;
    return finish_hidden_record(io, slot, record, bytes);
}

static int finish_authority_status(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                   uint32_t journal_slot, const uint8_t *journal,
                                   uint32_t journal_bytes, uint8_t status)
{
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    if (!io || !io->read16 || !io->program16 ||
        hpclang_rpl_flash_authority_encode(io->first, io->end, slot, journal_slot, journal,
                                           journal_bytes, authority, sizeof authority))
        return HPCLANG_RPL_FLASH_INVALID;
    if (matching_record(io, journal_slot, journal, journal_bytes, 0, 0x7e) !=
        HPCLANG_RPL_FLASH_WRITTEN)
        return HPCLANG_RPL_FLASH_DIRTY;
    authority[0] = status;
    return finish_hidden_record(io, slot, authority, sizeof authority);
}

int hpclang_rpl_flash_finish_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                       uint32_t journal_slot, const uint8_t *journal,
                                       uint32_t journal_bytes)
{
    return finish_authority_status(io, slot, journal_slot, journal, journal_bytes, 0x7eu);
}

int hpclang_rpl_flash_finish_staged_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                              uint32_t journal_slot, const uint8_t *journal,
                                              uint32_t journal_bytes)
{
    return finish_authority_status(io, slot, journal_slot, journal, journal_bytes, 0x7fu);
}

int hpclang_rpl_flash_activate_staged_authority(const struct hpclang_rpl_flash_io *io,
                                                uint32_t slot, uint32_t journal_slot,
                                                const uint8_t *journal, uint32_t journal_bytes)
{
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    if (!io || !io->read16 || !io->program16 ||
        hpclang_rpl_flash_authority_encode(io->first, io->end, slot, journal_slot, journal,
                                           journal_bytes, authority, sizeof authority))
        return HPCLANG_RPL_FLASH_INVALID;
    if (matching_record(io, journal_slot, journal, journal_bytes, 0, 0x7e) !=
        HPCLANG_RPL_FLASH_WRITTEN)
        return HPCLANG_RPL_FLASH_DIRTY;
    uint16_t word;
    if (!io->read16(io->context, slot & ~1u, &word))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint8_t phase = (uint8_t)(word >> (8u * (slot & 1u)));
    if ((phase != 0x7f && phase != 0x7e) || matching_record(io, slot, authority, sizeof authority,
                                                            0, phase) != HPCLANG_RPL_FLASH_WRITTEN)
        return HPCLANG_RPL_FLASH_DIRTY;
    /* The allocation was already published. Its successor may now be occupied;
     * activate only its phase byte, never resume body or tail publication. */
    return write_bytes(io, slot, authority, 1) ? HPCLANG_RPL_FLASH_WRITTEN
                                               : HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_flash_close_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                      uint32_t journal_slot, const uint8_t *journal,
                                      uint32_t journal_bytes)
{
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    uint32_t target;
    const uint8_t *image;
    if (!io || !io->read16 || !io->program16 ||
        hpclang_rpl_flash_authority_encode(io->first, io->end, slot, journal_slot, journal,
                                           journal_bytes, authority, sizeof authority) ||
        hpclang_rpl_flash_sector_decode(io->first, io->end, journal_slot, journal, journal_bytes,
                                        &target, &image))
        return HPCLANG_RPL_FLASH_INVALID;
    if (matching_record(io, slot, authority, sizeof authority, 1, 0) != HPCLANG_RPL_FLASH_WRITTEN ||
        matching_record(io, journal_slot, journal, journal_bytes, 0, 0) !=
            HPCLANG_RPL_FLASH_WRITTEN)
        return HPCLANG_RPL_FLASH_DIRTY;
    for (uint32_t i = 0; i < HPCLANG_RPL_FLASH_SECTOR_BYTES; i += 2u) {
        uint16_t word;
        uint16_t expected = (uint16_t)(image[i] | (uint16_t)image[i + 1u] << 8u);
        if (!io->read16(io->context, target + i, &word) || word != expected)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    const uint8_t closed = 0;
    return write_bytes(io, slot, &closed, 1u) ? HPCLANG_RPL_FLASH_WRITTEN : HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_flash_retire_sector(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                    const uint8_t *record, uint32_t bytes)
{
    uint32_t target;
    const uint8_t *image;
    if (!io || !io->read16 || !io->program16 ||
        hpclang_rpl_flash_sector_decode(io->first, io->end, slot, record, bytes, &target, &image))
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t word = 0;
    for (uint32_t i = 0; i < bytes; ++i) {
        uint32_t at = slot + i;
        if ((!i || !(at & 1u)) && !io->read16(io->context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        uint8_t value = (uint8_t)(word >> (8u * (at & 1u)));
        if (i ? value != record[i] : (value & 0x7eu) != value)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        uint16_t expected = (uint16_t)(image[offset] | (uint16_t)image[offset + 1u] << 8u);
        if (!io->read16(io->context, target + offset, &word) || word != expected)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    const uint8_t retired = 0;
    return write_bytes(io, slot, &retired, 1u) ? HPCLANG_RPL_FLASH_WRITTEN
                                               : HPCLANG_RPL_FLASH_DIRTY;
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

int hpclang_rpl_flash_find_sector(const struct hpclang_rpl_flash_io *io, uint32_t bank,
                                  uint8_t *scratch, uint32_t capacity, uint32_t *slot,
                                  uint32_t *target)
{
    if (!io || !io->read16 || !scratch || !slot || !target ||
        capacity < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES || (io->first & 0x1ffffu) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u || bank < io->first ||
        bank >= io->end || (bank & 0x1ffffu))
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t identity;
    if (!io->read16(io->context, bank + 0x100u, &identity))
        return HPCLANG_RPL_FLASH_FAILED;
    if (identity != (0xf0u | ((8u + (bank - io->first) / 0x20000u) << 8u)))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct recovery_reader mapped = {io, bank};
    const struct hpclang_rpl_reader reader = {io_nibble, &mapped};
    uint32_t found = io->end, destination = 0;
    enum hpclang_rpl_flash_sector_phase phase = HPCLANG_RPL_FLASH_SECTOR_READY;
    for (uint32_t at = 0x20au; at <= 0x3fffeu;) {
        uint32_t status, header;
        int result = recovery_word(&reader, at, 2u, &status);
        if (result)
            return result;
        if (status == 0xffu) {
            for (uint32_t tail = at + 2u; tail < 0x40000u; ++tail) {
                uint32_t value;
                result = recovery_word(&reader, tail, 1u, &value);
                if (result)
                    return result;
                if (value != 15u)
                    return HPCLANG_RPL_FLASH_DIRTY;
            }
            if (found == io->end)
                return 0;
            *slot = found;
            *target = destination;
            return phase;
        }
        if (at > 0x3fff8u)
            return HPCLANG_RPL_FLASH_DIRTY;
        result = recovery_word(&reader, at + 2u, 6u, &header);
        if (result)
            return result;
        uint32_t extent = header & 0xfffffu;
        if (extent < 11u || extent >= 0x40000u - at - 2u)
            return HPCLANG_RPL_FLASH_DIRTY;
        if (status && (status & 0x7eu) == status && (header >> 20u) == 3u) {
            if (found != io->end || (at & 1u) ||
                extent + 2u != HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES * 2u)
                return HPCLANG_RPL_FLASH_DIRTY;
            for (uint32_t i = 0; i < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES; ++i) {
                uint32_t value;
                result = recovery_word(&reader, at + i * 2u, 2u, &value);
                if (result)
                    return result;
                scratch[i] = (uint8_t)value;
            }
            const uint8_t *image;
            found = bank + at / 2u;
            if (hpclang_rpl_flash_sector_inspect(io->first, io->end, found, scratch,
                                                 HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
                                                 &destination, &image, &phase))
                return HPCLANG_RPL_FLASH_DIRTY;
        } else if (status == 0x53u) {
            struct hpclang_rpl_port_cursor cursor = {at, 0x40000u, 0};
            struct hpclang_rpl_port_entry entry;
            result = hpclang_rpl_flash_next(&reader, &cursor, &entry);
            if (result != 1)
                return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                              : HPCLANG_RPL_FLASH_DIRTY;
        } else if (status) {
            return HPCLANG_RPL_FLASH_DIRTY;
        }
        at += extent + 2u;
    }
    return HPCLANG_RPL_FLASH_DIRTY;
}

static int allocation_start(const struct hpclang_rpl_flash_io *io, uint32_t slot)
{
    uint32_t bank = io->first + ((slot - io->first) / 0x20000u) * 0x20000u;
    struct recovery_reader mapped = {io, bank};
    const struct hpclang_rpl_reader reader = {io_nibble, &mapped};
    uint32_t target = (slot - bank) * 2u, at = 0x20au;
    while (at < target) {
        uint32_t status, extent;
        int result = recovery_word(&reader, at, 2u, &status);
        if (result)
            return result;
        if (status == 0xffu)
            return HPCLANG_RPL_FLASH_DIRTY;
        result = recovery_word(&reader, at + 2u, 5u, &extent);
        if (result)
            return result;
        if (extent < 11u || extent + 2u > target - at)
            return HPCLANG_RPL_FLASH_DIRTY;
        at += extent + 2u;
    }
    return at == target ? HPCLANG_RPL_FLASH_WRITTEN : HPCLANG_RPL_FLASH_DIRTY;
}

static int intent_backup(const struct hpclang_rpl_flash_io *io, uint32_t slot, uint32_t bytes,
                         uint32_t expected_crc, struct hpclang_rpl_port_entry *entry)
{
    int result = allocation_start(io, slot);
    if (result)
        return result;
    uint32_t bank = io->first + ((slot - io->first) / 0x20000u) * 0x20000u;
    struct recovery_reader mapped = {io, bank};
    const struct hpclang_rpl_reader reader = {io_nibble, &mapped};
    uint32_t at = (slot - bank) * 2u;
    struct recovery_status view = {&reader, at, 0x53u};
    const struct hpclang_rpl_reader committed = {committed_status_nibble, &view};
    struct hpclang_rpl_port_cursor cursor = {at, 0x40000u, 0};
    result = hpclang_rpl_flash_next(&committed, &cursor, entry);
    if (result != 1)
        return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                      : HPCLANG_RPL_FLASH_DIRTY;
    if (entry->prologue != 0x02b62u || cursor.address - at != bytes * 2u)
        return HPCLANG_RPL_FLASH_DIRTY;
    uint32_t crc;
    result = hpclang_rpl_flash_record_crc(&reader, at, bytes, &crc);
    if (result)
        return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                      : HPCLANG_RPL_FLASH_DIRTY;
    return crc == expected_crc ? HPCLANG_RPL_FLASH_WRITTEN : HPCLANG_RPL_FLASH_DIRTY;
}

static int recover_intent(const struct hpclang_rpl_flash_io *io, uint32_t bank, uint32_t at,
                          int apply)
{
    struct recovery_reader mapped = {io, bank};
    const struct hpclang_rpl_reader reader = {io_nibble, &mapped};
    uint8_t encoded[HPCLANG_RPL_FLASH_INTENT_BYTES];
    for (uint32_t i = 0; i < sizeof encoded; ++i) {
        uint32_t byte;
        int result = recovery_word(&reader, at + i * 2u, 2u, &byte);
        if (result)
            return result;
        encoded[i] = (uint8_t)byte;
    }
    struct hpclang_rpl_flash_intent intent;
    enum hpclang_rpl_flash_intent_phase phase;
    if (hpclang_rpl_flash_intent_decode(io->first, io->end, bank + at / 2u, encoded, sizeof encoded,
                                        &intent, &phase))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint32_t old_bank = io->first + ((intent.old_slot - io->first) / 0x20000u) * 0x20000u;
    struct recovery_reader old_mapped = {io, old_bank};
    const struct hpclang_rpl_reader old_reader = {io_nibble, &old_mapped};
    uint32_t old_at = (intent.old_slot - old_bank) * 2u, status;
    int result = recovery_word(&old_reader, old_at, 2u, &status);
    if (result)
        return result;
    if (status & ~0x53u)
        return HPCLANG_RPL_FLASH_DIRTY;
    struct hpclang_rpl_port_entry old_entry;
    result = intent_backup(io, intent.old_slot, intent.old_bytes, intent.old_crc, &old_entry);
    if (result)
        return result;
    result = allocation_start(io, intent.new_slot);
    if (result)
        return result;
    uint16_t new_word;
    if (!io->read16(io->context, intent.new_slot & ~1u, &new_word))
        return HPCLANG_RPL_FLASH_FAILED;
    uint32_t new_status = (new_word >> ((intent.new_slot & 1u) * 8u)) & 255u;
    if (new_status != 0xffu && (new_status & 0x53u) == 0x53u) {
        if (phase == HPCLANG_RPL_FLASH_INTENT_UNPUBLISHED ||
            (phase == HPCLANG_RPL_FLASH_INTENT_RETIRING && status) ||
            (new_status != 0x53u && (phase != HPCLANG_RPL_FLASH_INTENT_READY || status != 0x53u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        struct hpclang_rpl_port_entry new_entry;
        result = intent_backup(io, intent.new_slot, intent.new_bytes, intent.new_crc, &new_entry);
        if (result)
            return result;
        if (old_entry.name_length != new_entry.name_length ||
            memcmp(old_entry.name, new_entry.name, old_entry.name_length))
            return HPCLANG_RPL_FLASH_DIRTY;
        const uint8_t live = 0x53u;
        if (apply && (!write_bytes(io, intent.new_slot, &live, 1u) ||
                      hpclang_rpl_flash_finish_remove(io, intent.old_slot) ||
                      !discard_status(io, bank + at / 2u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        return HPCLANG_RPL_FLASH_WRITTEN;
    }
    /* Roll back an unpublished body while retaining the exact reservation.
     * Zero metadata is made durable before any status bit is cleared, so a
     * second interruption is distinguishable from an incomplete backup. */
    if (status != 0x53u)
        return HPCLANG_RPL_FLASH_DIRTY;
    int erased = 1;
    uint32_t header = 0;
    for (uint32_t i = 0; i < intent.new_bytes; ++i) {
        uint32_t address = intent.new_slot + i;
        uint16_t word;
        if (!io->read16(io->context, address & ~1u, &word))
            return HPCLANG_RPL_FLASH_FAILED;
        uint32_t byte = (word >> ((address & 1u) * 8u)) & 255u;
        if (byte != 255u)
            erased = 0;
        if (i >= 1u && i <= 3u)
            header |= byte << ((i - 1u) * 8u);
    }
    if (!erased) {
        uint32_t extent = intent.new_bytes * 2u - 2u;
        int discarded = !(new_status & 1u) && header == extent;
        if (phase == HPCLANG_RPL_FLASH_INTENT_UNPUBLISHED ||
            (phase == HPCLANG_RPL_FLASH_INTENT_RETIRING && new_status) ||
            (!discarded && (new_status != 0xffu || (header & extent) != extent)))
            return HPCLANG_RPL_FLASH_DIRTY;
        /* The reservation was an erased tail when planned. Reject stale
         * evidence if another allocation or dirty data now follows it. */
        uint32_t end = io->first + ((intent.new_slot - io->first) / 0x20000u + 1u) * 0x20000u;
        for (uint32_t address = intent.new_slot + intent.new_bytes; address < end; ++address) {
            uint16_t word;
            if (!io->read16(io->context, address & ~1u, &word))
                return HPCLANG_RPL_FLASH_FAILED;
            if (((word >> ((address & 1u) * 8u)) & 255u) != 255u)
                return HPCLANG_RPL_FLASH_DIRTY;
        }
        const uint8_t marker[] = {(uint8_t)extent, (uint8_t)(extent >> 8u),
                                  (uint8_t)(extent >> 16u)};
        if (apply && (!write_bytes(io, intent.new_slot + 1u, marker, sizeof marker) ||
                      !discard_status(io, intent.new_slot)))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    if (apply && !discard_status(io, bank + at / 2u))
        return HPCLANG_RPL_FLASH_DIRTY;
    return HPCLANG_RPL_FLASH_WRITTEN;
}

static int recover_bank(const struct hpclang_rpl_flash_io *io, uint32_t bank, int apply,
                        uint32_t *intent_slot)
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
        if (status && status != 0x53u && (header >> 20u) == 2u) {
            if ((at & 1u) || extent + 2u != HPCLANG_RPL_FLASH_INTENT_BYTES * 2u)
                return HPCLANG_RPL_FLASH_DIRTY;
            if (intent_slot) {
                if (*intent_slot != io->end)
                    return HPCLANG_RPL_FLASH_DIRTY;
                *intent_slot = bank + at / 2u;
            }
            result = recover_intent(io, bank, at, apply);
            if (result)
                return result;
            at += extent + 2u;
            continue;
        }
        int unpublished = status != 0x53u && (status & 0x53u) == 0x53u;
        if (status && (!(status & 1u) || unpublished) && !(header >> 20u)) {
            if (unpublished) {
                /* Only library cancellation creates an odd zero-metadata
                 * marker. Do not broaden backup corruption recovery. */
                struct recovery_status view = {&reader, at, 0x53u};
                const struct hpclang_rpl_reader committed = {committed_status_nibble, &view};
                struct hpclang_rpl_port_cursor cursor = {at, 0x40000u, 0};
                struct hpclang_rpl_port_entry entry;
                result = hpclang_rpl_flash_next(&committed, &cursor, &entry);
                if (result != 1 || entry.prologue != 0x02b40u)
                    return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                                  : HPCLANG_RPL_FLASH_DIRTY;
            }
            /* Tail cleanup verifies zero metadata and the extent before
             * clearing bit zero. A cancelled library first clears metadata
             * while its status is still an unpublished superset of 53.
             * Every subsequent status remains even.
             * Require a bounded allocation covering the discard plan and an
             * erased remainder; a failed append may have reserved more space
             * than its partial body needs. Do not
             * interpret an incomplete body as a CRC-valid backup. */
            struct recovery_status view = {&reader, at, 0xffu};
            const struct hpclang_rpl_reader erased = {committed_status_nibble, &view};
            struct hpclang_rpl_flash_discard plan;
            result = hpclang_rpl_flash_discard_plan(&erased, at, 0x40000u, &plan);
            if (result != 1 || plan.address != at || plan.extent > extent || ((extent + 2u) & 3u))
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
                int library = entry.prologue == 0x02b40u;
                if ((entry.prologue != 0x02b62u && !(library && unpublished)) ||
                    (header >> 20u) != 1u ||
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
                if (library) {
                    /* Only abandon an unpublished final allocation. Never
                     * complete it or treat a partially removed live library
                     * as fresh installation. Zero metadata before changing
                     * status so every interrupted cleanup is recognizable by
                     * the bounded discard path above. Preserve the payload. */
                    struct hpclang_rpl_flash_space tail;
                    uint32_t next = at + extent + 2u;
                    result = hpclang_rpl_flash_tail(&reader, next, 0x40000u, &tail);
                    if (result || tail.address != next)
                        return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                                      : HPCLANG_RPL_FLASH_DIRTY;
                    const uint8_t metadata = (uint8_t)((header >> 16u) & 15u);
                    if (apply && !write_bytes(io, bank + at / 2u + 3u, &metadata, 1u))
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
    uint32_t intent_slot = io->end;
    for (uint32_t index = 0; index < 6u; ++index) {
        uint32_t bank = io->first + index * 0x20000u;
        uint16_t identity;
        if (!io->read16(io->context, bank + 0x100u, &identity))
            return HPCLANG_RPL_FLASH_FAILED;
        if (identity != (0xf0u | ((index + 8u) << 8u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        int result = recover_bank(io, bank, 0, &intent_slot);
        if (result)
            return result;
    }
    /* Re-read under the caller's stable mapping/ownership. No potentially
     * repairable early bank is changed before every later bank is checked. */
    if (intent_slot != io->end) {
        uint32_t bank = io->first + ((intent_slot - io->first) / 0x20000u) * 0x20000u;
        /* Resolve the transaction before generic cleanup can change either
         * target, including when the journal lives in a later bank. */
        if (recover_intent(io, bank, (intent_slot - bank) * 2u, 1))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    for (uint32_t index = 0; index < 6u; ++index) {
        uint32_t bank = io->first + index * 0x20000u;
        int result = recover_bank(io, bank, 1, NULL);
        if (result)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}

struct recovery_owner
{
    const struct hpclang_rpl_flash_io *io;
    int started;
};

static int owned_read(void *context, uint32_t address, uint16_t *value)
{
    const struct recovery_owner *owner = context;
    return owner->io->read16(owner->io->context, address, value);
}

static int owned_program(void *context, uint32_t address, uint16_t value)
{
    struct recovery_owner *owner = context;
    owner->started = 1;
    return owner->io->program16(owner->io->context, address, value);
}

int hpclang_rpl_flash_prepare_write(const struct hpclang_rpl_flash_io *io)
{
    if (!io || !io->read16 || !io->program16)
        return HPCLANG_RPL_FLASH_INVALID;
    struct recovery_owner owner = {io, 0};
    const struct hpclang_rpl_flash_io tracked = {&owner, owned_read, owned_program, io->first,
                                                 io->end};
    int result;
    do {
        result = hpclang_rpl_flash_recover_tails(&tracked);
    } while (result != HPCLANG_RPL_FLASH_WRITTEN && owner.started);
    return result;
}

struct record_selection
{
    uint32_t free_slot, match_slot, matches, match_bytes;
};

struct erased_tail
{
    uint32_t slot, bytes;
};

static int scan_records(const struct hpclang_rpl_flash_io *io, const uint8_t *name,
                        uint32_t name_length, uint32_t prologue, uint32_t library_number,
                        uint32_t bytes, struct record_selection *selection,
                        struct erased_tail *tails)
{
    if (!io || !io->read16 || ((io->first | io->end) & 1u) || io->first > io->end ||
        io->end - io->first != 6u * 0x20000u || (!name && name_length) || name_length > 255u)
        return HPCLANG_RPL_FLASH_INVALID;
    *selection = (struct record_selection){io->end, io->end, 0, 0};
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
                if (tails && ((entry.address | cursor.address) & 1u))
                    return HPCLANG_RPL_FLASH_UNSUPPORTED;
                selection->match_bytes = (cursor.address - entry.address + 8u) / 2u;
                if (tails && (selection->match_bytes & 1u))
                    return HPCLANG_RPL_FLASH_UNSUPPORTED;
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
        if (tails)
            tails[index] = (struct erased_tail){bank + space.address / 2u, space.nibbles / 2u};
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}

static int select_record(const struct hpclang_rpl_flash_io *io, const uint8_t *name,
                         uint32_t name_length, uint32_t prologue, uint32_t library_number,
                         uint32_t bytes, struct record_selection *selection,
                         struct erased_tail *tails)
{
    if (!io || !io->program16)
        return HPCLANG_RPL_FLASH_INVALID;
    return scan_records(io, name, name_length, prologue, library_number, bytes, selection, tails);
}

int hpclang_rpl_flash_match_library(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                    uint32_t bytes, uint32_t *slot)
{
    if (!slot || !valid_record(record, bytes))
        return HPCLANG_RPL_FLASH_INVALID;
    struct packed_record packed = {record, bytes};
    const struct hpclang_rpl_reader source = {packed_read, &packed};
    struct hpclang_rpl_port_cursor cursor = {8u, bytes * 2u, 0};
    struct hpclang_rpl_port_entry wanted;
    if (hpclang_rpl_port_next(&source, &cursor, &wanted) != 1 || wanted.prologue != 0x02b40u)
        return HPCLANG_RPL_FLASH_INVALID;
    struct record_selection selection;
    int result =
        scan_records(io, NULL, 0, wanted.prologue, wanted.library_number, bytes, &selection, NULL);
    if (result)
        return result;
    if (!selection.matches)
        return HPCLANG_RPL_FLASH_NOT_FOUND;
    if (selection.matches != 1 || selection.match_bytes != bytes)
        return HPCLANG_RPL_FLASH_DIRTY;
    uint32_t nibbles = 8u + wanted.span;
    result = matching_record(io, selection.match_slot, record, nibbles / 2u, 0, 0x53u);
    if (!result && (nibbles & 1u)) {
        uint32_t last = selection.match_slot + nibbles / 2u;
        uint16_t word;
        if (!io->read16(io->context, last & ~1u, &word))
            return HPCLANG_RPL_FLASH_FAILED;
        if (((word >> (8u * (last & 1u))) & 15u) != (record[nibbles / 2u] & 15u))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    if (!result)
        *slot = selection.match_slot;
    return result;
}

static int match_catalog(const struct hpclang_rpl_flash_io *io, uint32_t matched,
                         const struct hpclang_rpl_port_entry *wanted,
                         const struct hpclang_rpl_flash_catalog *catalog)
{
    struct hpclang_rpl_library_binding binding;
    int result = hpclang_rpl_library_lookup(catalog->reader, catalog->address, catalog->nibbles,
                                            wanted->library_number, &binding);
    if (result != 1)
        return result == 0                              ? HPCLANG_RPL_FLASH_NOT_FOUND
               : result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                        : HPCLANG_RPL_FLASH_DIRTY;
    uint32_t offset = (matched - io->first) % 0x20000u;
    uint32_t header = 0x40000u + 2u * (offset + 4u) + 14u + 2u * wanted->name_length;
    if (binding.address != header)
        return HPCLANG_RPL_FLASH_DIRTY;
    uint32_t expected_bank = 8u + (matched - io->first) / 0x20000u;
    unsigned matches = 0;
    for (uint32_t bank = 0; bank < 16; ++bank) {
        uint32_t value;
        result = recovery_word(catalog->reader, catalog->flash_table + 7u * bank, 5u, &value);
        if (result)
            return result;
        if (!value)
            return HPCLANG_RPL_FLASH_DIRTY;
        if (value == binding.bank_switch) {
            if (bank != expected_bank || ++matches != 1)
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    if (matches != 1)
        return HPCLANG_RPL_FLASH_DIRTY;
    return 0;
}

static int valid_catalog(const struct hpclang_rpl_flash_catalog *catalog)
{
    return catalog && catalog->reader && catalog->reader->read_nibble &&
           catalog->flash_table <= 0x100000u - 112u;
}

int hpclang_rpl_flash_match_installation(const struct hpclang_rpl_flash_io *io,
                                         const uint8_t *record, uint32_t bytes,
                                         const struct hpclang_rpl_flash_catalog *catalog,
                                         uint32_t *slot)
{
    if (!slot || !valid_catalog(catalog))
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t matched;
    int result = hpclang_rpl_flash_match_library(io, record, bytes, &matched);
    if (result)
        return result;
    struct packed_record packed = {record, bytes};
    const struct hpclang_rpl_reader source = {packed_read, &packed};
    struct hpclang_rpl_port_cursor cursor = {8u, bytes * 2u, 0};
    struct hpclang_rpl_port_entry wanted;
    if (hpclang_rpl_port_next(&source, &cursor, &wanted) != 1)
        return HPCLANG_RPL_FLASH_INVALID;
    result = match_catalog(io, matched, &wanted, catalog);
    if (!result)
        *slot = matched;
    return result;
}

int hpclang_rpl_flash_locate_installation(const struct hpclang_rpl_flash_io *io,
                                          uint32_t library_number,
                                          const struct hpclang_rpl_flash_catalog *catalog,
                                          uint32_t *slot)
{
    if (!slot || library_number > 0xfffu || !valid_catalog(catalog))
        return HPCLANG_RPL_FLASH_INVALID;
    struct record_selection selection;
    int result = scan_records(io, NULL, 0, 0x2b40u, library_number, 0, &selection, NULL);
    if (result)
        return result;
    if (!selection.matches)
        return HPCLANG_RPL_FLASH_NOT_FOUND;
    if (selection.matches != 1)
        return HPCLANG_RPL_FLASH_DIRTY;
    struct recovery_reader flash = {io, selection.match_slot};
    const struct hpclang_rpl_reader reader = {io_nibble, &flash};
    struct hpclang_rpl_port_cursor cursor = {8u, selection.match_bytes * 2u, 0};
    struct hpclang_rpl_port_entry wanted;
    if (hpclang_rpl_port_next(&reader, &cursor, &wanted) != 1)
        return HPCLANG_RPL_FLASH_DIRTY;
    uint8_t metadata;
    if (!io_nibble(&flash, 7u, &metadata))
        return HPCLANG_RPL_FLASH_FAILED;
    uint32_t extent;
    result = recovery_word(&reader, 2u, 5u, &extent);
    if (result)
        return result;
    if (metadata != 1u || extent + 2u != selection.match_bytes * 2u ||
        (((wanted.span + 15u) & ~7u) != selection.match_bytes * 2u &&
         ((wanted.span + 11u) & ~3u) != selection.match_bytes * 2u))
        return HPCLANG_RPL_FLASH_DIRTY;
    result = match_catalog(io, selection.match_slot, &wanted, catalog);
    if (!result)
        *slot = selection.match_slot;
    return result;
}

static int select_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *name,
                         uint32_t name_length, uint32_t bytes, struct record_selection *selection)
{
    return select_record(io, name, name_length, 0x02b62u, 0, bytes, selection, NULL);
}

static int plan_record(const struct hpclang_rpl_flash_io *io, const uint8_t *record, uint32_t bytes,
                       uint32_t prologue, uint32_t *slot, struct erased_tail *tails)
{
    if (!slot || !valid_record(record, bytes))
        return HPCLANG_RPL_FLASH_INVALID;
    struct packed_record packed = {record, bytes};
    const struct hpclang_rpl_reader source = {packed_read, &packed};
    struct hpclang_rpl_port_cursor source_cursor = {8u, bytes * 2u, 0};
    struct hpclang_rpl_port_entry wanted;
    if (hpclang_rpl_port_next(&source, &source_cursor, &wanted) != 1 || wanted.prologue != prologue)
        return HPCLANG_RPL_FLASH_INVALID;
    struct record_selection selection;
    int result = scan_records(io, wanted.name, wanted.name_length, wanted.prologue,
                              wanted.library_number, bytes, &selection, tails);
    if (result)
        return result;
    if (selection.matches)
        return HPCLANG_RPL_FLASH_EXISTS;
    uint32_t selected = selection.free_slot;
    if (selected == io->end)
        return HPCLANG_RPL_FLASH_NOSPACE;
    *slot = selected;
    return HPCLANG_RPL_FLASH_WRITTEN;
}

int hpclang_rpl_flash_plan_library(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                   uint32_t bytes, uint32_t *slot)
{
    return plan_record(io, record, bytes, 0x02b40u, slot, NULL);
}

static int take_recovery_tail(struct erased_tail *tail, uint32_t bytes, uint32_t target,
                              uint32_t *slot)
{
    if ((tail->slot & 3u) != 1u || tail->bytes < bytes ||
        (tail->slot < target + HPCLANG_RPL_FLASH_SECTOR_BYTES && target < tail->slot + bytes))
        return 0;
    *slot = tail->slot;
    tail->slot += bytes;
    tail->bytes -= bytes;
    return 1;
}

static int plan_recovery_space(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                               uint32_t bytes, uint32_t target,
                               struct hpclang_rpl_flash_recovery_space *space, uint32_t *claim_slot)
{
    if (!space || !io || (io->first & 0x1ffffu) || io->first > io->end ||
        io->end - io->first != 6u * 0x20000u || target < io->first ||
        target > io->end - HPCLANG_RPL_FLASH_SECTOR_BYTES ||
        (target & (HPCLANG_RPL_FLASH_SECTOR_BYTES - 1u)) || (bytes & 3u))
        return HPCLANG_RPL_FLASH_INVALID;
    struct erased_tail tails[6];
    uint32_t unused;
    int result = plan_record(io, record, bytes, 0x02b40u, &unused, tails);
    if (result)
        return result;
    for (unsigned library = 0; library < 6; ++library)
        for (unsigned journal = 0; journal < 6; ++journal)
            for (unsigned authority = 0; authority < 6; ++authority) {
                struct erased_tail available[6];
                memcpy(available, tails, sizeof available);
                struct hpclang_rpl_flash_recovery_space selected;
                if (take_recovery_tail(&available[library], bytes, target,
                                       &selected.library_slot) &&
                    take_recovery_tail(&available[journal], HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
                                       target, &selected.journal_slot) &&
                    take_recovery_tail(&available[authority], HPCLANG_RPL_FLASH_AUTHORITY_BYTES,
                                       target, &selected.authority_slot)) {
                    if (!claim_slot) {
                        *space = selected;
                        return 0;
                    }
                    for (unsigned claim = 0; claim < 6; ++claim) {
                        uint32_t slot;
                        if (take_recovery_tail(&available[claim],
                                               HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES, target,
                                               &slot)) {
                            *space = selected;
                            *claim_slot = slot;
                            return 0;
                        }
                    }
                }
            }
    return HPCLANG_RPL_FLASH_NOSPACE;
}

int hpclang_rpl_flash_plan_recovery_space(const struct hpclang_rpl_flash_io *io,
                                          const uint8_t *record, uint32_t bytes, uint32_t target,
                                          struct hpclang_rpl_flash_recovery_space *space)
{
    return plan_recovery_space(io, record, bytes, target, space, NULL);
}

int hpclang_rpl_flash_plan_creation_space(const struct hpclang_rpl_flash_io *io,
                                          const uint8_t *record, uint32_t bytes, uint32_t target,
                                          struct hpclang_rpl_flash_recovery_space *space,
                                          uint32_t *claim_slot)
{
    if (!claim_slot)
        return HPCLANG_RPL_FLASH_INVALID;
    return plan_recovery_space(io, record, bytes, target, space, claim_slot);
}

static int create_record(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                         uint32_t bytes, uint32_t prologue, uint32_t *attempted_slot)
{
    if (!io || !io->program16)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t selected;
    int result = plan_record(io, record, bytes, prologue, &selected, NULL);
    if (result)
        return result;
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

int hpclang_rpl_flash_create_library_at(const struct hpclang_rpl_flash_io *io,
                                        const uint8_t *record, uint32_t bytes, uint32_t slot)
{
    if (!io || !io->program16 || slot < io->first || slot >= io->end)
        return HPCLANG_RPL_FLASH_INVALID;
    struct erased_tail tails[6];
    uint32_t unused;
    int result = plan_record(io, record, bytes, 0x02b40u, &unused, tails);
    if (result)
        return result;
    uint32_t index = (slot - io->first) / 0x20000u;
    if (slot != tails[index].slot)
        return HPCLANG_RPL_FLASH_OCCUPIED;
    if (bytes > tails[index].bytes)
        return HPCLANG_RPL_FLASH_NOSPACE;
    struct hpclang_rpl_flash_io reserved = *io;
    uint32_t bank = io->first + index * 0x20000u;
    reserved.first = bank + 0x104u;
    reserved.end = bank + 0x20000u;
    return hpclang_rpl_flash_append(&reserved, slot, record, bytes);
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

int hpclang_rpl_flash_plan_replace(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                   uint32_t bytes, struct hpclang_rpl_flash_replace_plan *plan)
{
    if (!plan || !valid_record(record, bytes))
        return HPCLANG_RPL_FLASH_INVALID;
    struct packed_record packed = {record, bytes};
    const struct hpclang_rpl_reader source = {packed_read, &packed};
    struct hpclang_rpl_port_cursor cursor = {8u, bytes * 2u, 0};
    struct hpclang_rpl_port_entry wanted;
    if (hpclang_rpl_port_next(&source, &cursor, &wanted) != 1 || wanted.prologue != 0x02b62u)
        return HPCLANG_RPL_FLASH_INVALID;
    struct record_selection selection;
    struct erased_tail tails[6];
    int result = select_record(io, wanted.name, wanted.name_length, wanted.prologue, 0, bytes,
                               &selection, tails);
    if (result)
        return result;
    if (!selection.matches)
        return HPCLANG_RPL_FLASH_NOT_FOUND;
    if (selection.matches != 1u)
        return HPCLANG_RPL_FLASH_DIRTY;
    struct hpclang_rpl_flash_replace_plan next = {0};
    int found = 0;
    for (unsigned journal = 0; journal < 6 && !found; ++journal) {
        if (tails[journal].bytes < HPCLANG_RPL_FLASH_INTENT_BYTES)
            continue;
        for (unsigned value = 0; value < 6; ++value) {
            uint32_t prefix = journal == value ? HPCLANG_RPL_FLASH_INTENT_BYTES : 0;
            if (tails[value].bytes < prefix || tails[value].bytes - prefix < bytes)
                continue;
            next.intent_slot = tails[journal].slot;
            next.intent.new_slot = tails[value].slot + prefix;
            found = 1;
            break;
        }
    }
    if (!found)
        return HPCLANG_RPL_FLASH_NOSPACE;
    next.intent.old_slot = selection.match_slot;
    next.intent.old_bytes = selection.match_bytes;
    next.intent.new_bytes = bytes;
    uint32_t bank = io->first + ((selection.match_slot - io->first) / 0x20000u) * 0x20000u;
    struct recovery_reader mapped = {io, bank};
    const struct hpclang_rpl_reader reader = {io_nibble, &mapped};
    result = hpclang_rpl_flash_record_crc(&reader, (selection.match_slot - bank) * 2u,
                                          selection.match_bytes, &next.intent.old_crc);
    if (!result)
        result = hpclang_rpl_flash_record_crc(&source, 0, bytes, &next.intent.new_crc);
    if (result)
        return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                      : HPCLANG_RPL_FLASH_DIRTY;
    *plan = next;
    return HPCLANG_RPL_FLASH_WRITTEN;
}

int hpclang_rpl_flash_publish_intent(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                     uint32_t bytes, struct hpclang_rpl_flash_replace_plan *plan)
{
    if (!plan)
        return HPCLANG_RPL_FLASH_INVALID;
    struct hpclang_rpl_flash_replace_plan next;
    int result = hpclang_rpl_flash_plan_replace(io, record, bytes, &next);
    if (result)
        return result;
    uint8_t encoded[HPCLANG_RPL_FLASH_INTENT_BYTES];
    if (hpclang_rpl_flash_intent_encode(io->first, io->end, next.intent_slot, &next.intent, encoded,
                                        sizeof encoded))
        return HPCLANG_RPL_FLASH_INVALID;
    *plan = next;
    if (!write_bytes(io, next.intent_slot + 1u, encoded + 1u, sizeof encoded - 1u) ||
        !write_bytes(io, next.intent_slot, encoded, 1u))
        return HPCLANG_RPL_FLASH_DIRTY;
    return HPCLANG_RPL_FLASH_WRITTEN;
}

int hpclang_rpl_flash_replace_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                     uint32_t bytes,
                                     struct hpclang_rpl_flash_replacement *transaction)
{
    if (!transaction)
        return HPCLANG_RPL_FLASH_INVALID;
    struct hpclang_rpl_flash_replace_plan plan = {.intent_slot = UINT32_MAX};
    int result = hpclang_rpl_flash_publish_intent(io, record, bytes, &plan);
    if (plan.intent_slot == UINT32_MAX)
        return result;
    *transaction =
        (struct hpclang_rpl_flash_replacement){plan.intent.old_slot, plan.intent.new_slot, 0};
    /* Do not use append's legacy failure cleanup: the durable intent decides
     * whether an uncertain publication rolls back or finishes committing. */
    if (result || !write_bytes(io, transaction->new_slot + 1u, record + 1u, bytes - 1u) ||
        !write_bytes(io, transaction->new_slot, record, 1u))
        return HPCLANG_RPL_FLASH_DIRTY;
    return hpclang_rpl_flash_finish_replace(io, transaction);
}

int hpclang_rpl_flash_finish_replace(const struct hpclang_rpl_flash_io *io,
                                     struct hpclang_rpl_flash_replacement *transaction)
{
    if (!io || !transaction || !io->read16 || transaction->new_slot < io->first ||
        transaction->new_slot >= io->end || transaction->old_slot < io->first ||
        transaction->old_slot >= io->end)
        return HPCLANG_RPL_FLASH_INVALID;
    if (hpclang_rpl_flash_recover_tails(io))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint16_t new_word, old_word;
    if (!io->read16(io->context, transaction->new_slot & ~1u, &new_word) ||
        !io->read16(io->context, transaction->old_slot & ~1u, &old_word))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint32_t new_status = (new_word >> ((transaction->new_slot & 1u) * 8u)) & 255u;
    uint32_t old_status = (old_word >> ((transaction->old_slot & 1u) * 8u)) & 255u;
    if (new_status == 0x53u && !old_status) {
        transaction->committed = 1;
        return HPCLANG_RPL_FLASH_WRITTEN;
    }
    if ((new_status == 0 || new_status == 0xffu) && old_status == 0x53u) {
        transaction->committed = 0;
        return HPCLANG_RPL_FLASH_FAILED;
    }
    return HPCLANG_RPL_FLASH_DIRTY;
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
    uint16_t status_word, metadata_word;
    if (!io->read16(io->context, address & ~1u, &status_word) ||
        !io->read16(io->context, (address + 3u) & ~1u, &metadata_word))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint8_t status = (uint8_t)(status_word >> ((address & 1u) * 8u));
    uint8_t metadata = (uint8_t)(metadata_word >> (((address + 3u) & 1u) * 8u));
    /* Before publication, a failed append can have an incomplete body. Mark
     * its reserved extent as a discard while FF still hides it. Preserve that
     * marker when the owner retries cleanup after an uncertain status write.
     * A published record retains its ordinary metadata during deletion. */
    uint8_t header[] = {
        record[1], record[2],
        (uint8_t)((record[3] & 15u) | (status == 0xffu ? 0u : (record[3] & metadata & 0xf0u)))};
    if (!write_bytes(io, address + 1u, header, sizeof header) || !discard_status(io, address))
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
