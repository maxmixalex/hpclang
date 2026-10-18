#include "rpl-flash-replay.h"
#include <stddef.h>
#include <string.h>

static uint16_t image_word(const uint8_t *image, uint32_t offset)
{
    return (uint16_t)(image[offset] | (uint16_t)image[offset + 1u] << 8u);
}

struct sector_overlay
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t target;
    const uint8_t *image;
};

static int overlay_read(void *context, uint32_t address, uint16_t *value)
{
    const struct sector_overlay *overlay = context;
    if (address >= overlay->target && address - overlay->target < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
        *value = image_word(overlay->image, address - overlay->target);
        return 1;
    }
    return overlay->io->read16(overlay->io->context, address, value);
}

int hpclang_rpl_flash_preflight_sector(const struct hpclang_rpl_flash_io *io, uint8_t *scratch,
                                       uint32_t capacity, uint32_t *slot, uint32_t *target)
{
    if (!io || !io->read16 || !scratch || !slot || !target ||
        capacity < 2u * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES || (io->first & 0x1ffffu) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t journal = io->end, destination = 0;
    int damaged = 0, phase = 0;
    for (uint32_t bank = io->first; bank < io->end; bank += 0x20000u) {
        uint32_t found, candidate_target;
        int result =
            hpclang_rpl_flash_find_sector(io, bank, scratch, capacity, &found, &candidate_target);
        if (result == HPCLANG_RPL_FLASH_DIRTY) {
            damaged = 1;
            continue;
        }
        if (result < 0)
            return result;
        if (result > 0) {
            if (journal != io->end)
                return HPCLANG_RPL_FLASH_DIRTY;
            journal = found;
            destination = candidate_target;
            phase = result;
        }
    }
    if (journal == io->end)
        return damaged ? HPCLANG_RPL_FLASH_DIRTY : 0;
    /* Later discovery may have overwritten scratch. Reload the selected image
     * and keep it separate from the validation scanner's workspace. */
    uint32_t found, candidate_target;
    int result = hpclang_rpl_flash_find_sector(io, journal & ~0x1ffffu, scratch, capacity, &found,
                                               &candidate_target);
    if (result != phase || found != journal || candidate_target != destination)
        return result < 0 ? result : HPCLANG_RPL_FLASH_DIRTY;
    if (phase == HPCLANG_RPL_FLASH_SECTOR_RETIRING) {
        for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
            uint16_t word;
            if (!io->read16(io->context, destination + offset, &word))
                return HPCLANG_RPL_FLASH_FAILED;
            if (word != image_word(scratch + 24u, offset))
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    struct sector_overlay overlay = {io, destination, scratch + 24u};
    const struct hpclang_rpl_flash_io restored = {&overlay, overlay_read, NULL, io->first, io->end};
    unsigned journals = 0;
    for (uint32_t bank = io->first; bank < io->end; bank += 0x20000u) {
        result = hpclang_rpl_flash_find_sector(
            &restored, bank, scratch + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
            HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &found, &candidate_target);
        if (result < 0)
            return result;
        if (result > 0) {
            if (result != phase || found != journal || candidate_target != destination ||
                ++journals != 1)
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    if (journals != 1)
        return HPCLANG_RPL_FLASH_DIRTY;
    *slot = journal;
    *target = destination;
    /* Normalize only the owned RAM copy for retire_sector's expected-record
     * contract. Durable status is unchanged; the returned phase controls use. */
    scratch[0] = 0x7eu;
    return phase;
}

struct bound_overlay
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t authority, journal, target, bank;
    const uint8_t *image;
    int hide_journal;
};

static int bound_read(void *context, uint32_t address, uint16_t *value)
{
    const struct bound_overlay *view = context;
    if (view->image && address >= view->target &&
        address - view->target < HPCLANG_RPL_FLASH_SECTOR_BYTES)
        *value = image_word(view->image, address - view->target);
    else if (!view->io->read16(view->io->context, address, value))
        return 0;
    if (address == (view->authority & ~1u))
        *value &= (uint16_t)~(255u << (8u * (view->authority & 1u)));
    if (view->hide_journal && address == (view->journal & ~1u))
        *value &= (uint16_t)~(255u << (8u * (view->journal & 1u)));
    return 1;
}

static int bound_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct bound_overlay *view = context;
    uint32_t physical = view->bank + address / 2u;
    uint16_t word;
    if (address >= 0x40000u || !bound_read(view, physical & ~1u, &word))
        return 0;
    *value = (uint8_t)((word >> (4u * (address & 3u))) & 15u);
    return 1;
}

static int load_bytes(const struct hpclang_rpl_flash_io *io, uint32_t address, uint8_t *data,
                      uint32_t bytes)
{
    uint16_t word = 0;
    for (uint32_t i = 0; i < bytes; ++i) {
        uint32_t at = address + i;
        if ((!i || !(at & 1u)) && !io->read16(io->context, at & ~1u, &word))
            return 0;
        data[i] = (uint8_t)(word >> (8u * (at & 1u)));
    }
    return 1;
}

static int bound_membership(struct bound_overlay *view)
{
    const struct hpclang_rpl_reader reader = {bound_nibble, view};
    struct hpclang_rpl_flash_allocation allocation;
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t slot = index ? view->journal : view->authority;
        uint32_t size =
            index ? HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES : HPCLANG_RPL_FLASH_AUTHORITY_BYTES;
        view->bank = slot & ~0x1ffffu;
        uint16_t identity;
        if (!bound_read(view, view->bank + 0x100u, &identity))
            return HPCLANG_RPL_FLASH_FAILED;
        if (identity != (0xf0u | ((8u + (view->bank - view->io->first) / 0x20000u) << 8u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        uint32_t address = (slot - view->bank) * 2u;
        int membership =
            hpclang_rpl_flash_allocation_find(&reader, 0x20au, 0x40000u, address, &allocation);
        if (membership != 1 || allocation.address != address || allocation.nibbles != 2u * size)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return 0;
}

int hpclang_rpl_flash_bound_membership(const struct hpclang_rpl_flash_io *io,
                                       uint32_t authority_slot, const uint8_t *authority,
                                       uint32_t authority_bytes, uint32_t journal_slot,
                                       const uint8_t *journal, uint32_t journal_bytes)
{
    if (!io || !io->read16 || (io->first & 0x1ffffu) || io->first > io->end ||
        io->end - io->first != 6u * 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    enum hpclang_rpl_flash_authority_phase phase;
    enum hpclang_rpl_flash_sector_phase journal_phase;
    uint32_t target;
    const uint8_t *image;
    if (hpclang_rpl_flash_authority_inspect(io->first, io->end, authority_slot, authority,
                                            authority_bytes, journal_slot, journal, journal_bytes,
                                            &phase) ||
        hpclang_rpl_flash_sector_inspect(io->first, io->end, journal_slot, journal, journal_bytes,
                                         &target, &image, &journal_phase) ||
        phase == HPCLANG_RPL_FLASH_AUTHORITY_UNPUBLISHED ||
        (phase != HPCLANG_RPL_FLASH_AUTHORITY_ARMED &&
         journal_phase != HPCLANG_RPL_FLASH_SECTOR_RETIRED))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct bound_overlay view = {io,
                                 authority_slot,
                                 journal_slot,
                                 target,
                                 0,
                                 journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED ? NULL : image,
                                 1};
    return bound_membership(&view);
}

int hpclang_rpl_flash_preflight_bound(const struct hpclang_rpl_flash_io *io,
                                      uint32_t authority_slot, uint32_t journal_slot,
                                      uint8_t *scratch, uint32_t capacity, uint32_t *target)
{
    if (!io || !io->read16 || !scratch || !target ||
        capacity < 2u * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES || (io->first & 0x1ffffu) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u || authority_slot < io->first ||
        authority_slot >= io->end ||
        HPCLANG_RPL_FLASH_AUTHORITY_BYTES >= io->end - authority_slot || journal_slot < io->first ||
        journal_slot >= io->end || HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES >= io->end - journal_slot)
        return HPCLANG_RPL_FLASH_INVALID;
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    const uint32_t bytes = HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES;
    if (!load_bytes(io, authority_slot, authority, sizeof authority) ||
        !load_bytes(io, journal_slot, scratch, bytes))
        return HPCLANG_RPL_FLASH_FAILED;
    enum hpclang_rpl_flash_authority_phase phase;
    enum hpclang_rpl_flash_sector_phase journal_phase;
    uint32_t destination;
    const uint8_t *image;
    if (hpclang_rpl_flash_authority_inspect(io->first, io->end, authority_slot, authority,
                                            sizeof authority, journal_slot, scratch, bytes,
                                            &phase) ||
        hpclang_rpl_flash_sector_inspect(io->first, io->end, journal_slot, scratch, bytes,
                                         &destination, &image, &journal_phase) ||
        phase == HPCLANG_RPL_FLASH_AUTHORITY_UNPUBLISHED ||
        (phase != HPCLANG_RPL_FLASH_AUTHORITY_ARMED &&
         journal_phase != HPCLANG_RPL_FLASH_SECTOR_RETIRED))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct bound_overlay view = {
        io, authority_slot, journal_slot, destination, authority_slot & ~0x1ffffu, NULL, 0};
    struct hpclang_rpl_flash_io masked = {&view, bound_read, NULL, io->first, io->end};
    uint32_t found, restored;
    int result = hpclang_rpl_flash_preflight_sector(&masked, scratch, capacity, &found, &restored);
    if (result < 0)
        return result;
    if (journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED) {
        if (result != 0)
            return HPCLANG_RPL_FLASH_DIRTY;
    } else if (result <= 0 || result != (int)journal_phase || found != journal_slot ||
               restored != destination) {
        return HPCLANG_RPL_FLASH_DIRTY;
    }
    if (!load_bytes(io, journal_slot, scratch, bytes))
        return HPCLANG_RPL_FLASH_FAILED;
    enum hpclang_rpl_flash_sector_phase reloaded_phase;
    uint32_t reloaded_target;
    if (hpclang_rpl_flash_authority_inspect(io->first, io->end, authority_slot, authority,
                                            sizeof authority, journal_slot, scratch, bytes,
                                            &phase) ||
        hpclang_rpl_flash_sector_inspect(io->first, io->end, journal_slot, scratch, bytes,
                                         &reloaded_target, &image, &reloaded_phase) ||
        reloaded_phase != journal_phase || reloaded_target != destination)
        return HPCLANG_RPL_FLASH_DIRTY;
    /* Resolve allocation membership on the restored view. Hide only the two
     * already validated transaction statuses; payload bytes stay unchanged. */
    view.hide_journal = 1;
    view.image = journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED ? NULL : image;
    int membership = bound_membership(&view);
    if (membership)
        return membership;
    if (phase == HPCLANG_RPL_FLASH_AUTHORITY_CLOSED)
        return 0;
    if (journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED) {
        for (uint32_t i = 0; i < HPCLANG_RPL_FLASH_SECTOR_BYTES; i += 2u) {
            uint16_t word;
            if (!io->read16(io->context, destination + i, &word))
                return HPCLANG_RPL_FLASH_FAILED;
            if (word != image_word(scratch + 24u, i))
                return HPCLANG_RPL_FLASH_DIRTY;
        }
        result = HPCLANG_RPL_FLASH_SECTOR_RETIRING;
    }
    scratch[0] = 0x7e;
    *target = destination;
    return result;
}

static int sector_replay(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                         const uint8_t *expected_record, uint8_t *scratch, uint32_t capacity)
{
    if (!io || !io->flash.read16 || !io->flash.program16 || !io->erase_sector || !scratch ||
        capacity < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES)
        return HPCLANG_RPL_FLASH_INVALID;
    const struct hpclang_rpl_flash_io *flash = &io->flash;
    if ((flash->first & 0x1ffffu) || flash->first > flash->end ||
        flash->end - flash->first != 6u * 0x20000u || slot < flash->first || slot >= flash->end ||
        HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES >= flash->end - slot)
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t word = 0;
    for (uint32_t i = 0; i < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES; ++i) {
        uint32_t at = slot + i;
        if ((!i || !(at & 1u)) && !flash->read16(flash->context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        scratch[i] = (uint8_t)(word >> (8u * (at & 1u)));
    }
    if (expected_record && memcmp(expected_record, scratch, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint32_t target;
    const uint8_t *image;
    if (hpclang_rpl_flash_sector_decode(flash->first, flash->end, slot, scratch,
                                        HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &target, &image))
        return HPCLANG_RPL_FLASH_INVALID;
    int matches = 1;
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        if (!flash->read16(flash->context, target + offset, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        if (word != image_word(image, offset))
            matches = 0;
    }
    if (matches)
        return HPCLANG_RPL_FLASH_WRITTEN;
    if (!io->erase_sector(flash->context, target))
        return HPCLANG_RPL_FLASH_DIRTY;
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        if (!flash->read16(flash->context, target + offset, &word) || word != UINT16_MAX)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    /* Keep a bank's identity erased until its first sector is complete. Stock
     * may rebuild its library catalog after a reset between program commands;
     * an early identity would expose a partly restored allocation chain. */
    uint32_t identity = (target & 0x1ffffu) ? HPCLANG_RPL_FLASH_SECTOR_BYTES : 0x100u;
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        uint16_t expected = image_word(image, offset);
        if (offset != identity && expected != UINT16_MAX &&
            !flash->program16(flash->context, target + offset, expected))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        uint16_t expected = offset == identity ? UINT16_MAX : image_word(image, offset);
        if (!flash->read16(flash->context, target + offset, &word) || word != expected)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    if (identity < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
        uint16_t expected = image_word(image, identity);
        if ((expected != UINT16_MAX &&
             !flash->program16(flash->context, target + identity, expected)) ||
            !flash->read16(flash->context, target + identity, &word) || word != expected)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}

int hpclang_rpl_flash_sector_replay(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                                    uint8_t *scratch, uint32_t capacity)
{
    return sector_replay(io, slot, NULL, scratch, capacity);
}

int hpclang_rpl_flash_sector_replay_expected(const struct hpclang_rpl_flash_replay_io *io,
                                             uint32_t slot, const uint8_t *expected,
                                             uint32_t expected_bytes, uint8_t *scratch,
                                             uint32_t capacity)
{
    if (!expected || expected_bytes != HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES)
        return HPCLANG_RPL_FLASH_INVALID;
    uintptr_t a = (uintptr_t)expected, b = (uintptr_t)scratch;
    if ((a >= b ? a - b : b - a) < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES)
        return HPCLANG_RPL_FLASH_INVALID;
    return sector_replay(io, slot, expected, scratch, capacity);
}

int hpclang_rpl_flash_sector_replay_bound(const struct hpclang_rpl_flash_replay_io *io,
                                          uint32_t authority_slot, uint32_t journal_slot,
                                          const uint8_t *expected, uint32_t expected_bytes,
                                          uint8_t *scratch, uint32_t capacity)
{
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    if (!io || !io->flash.read16 || !io->flash.program16 || !io->erase_sector || !scratch ||
        capacity < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES ||
        hpclang_rpl_flash_authority_encode(io->flash.first, io->flash.end, authority_slot,
                                           journal_slot, expected, expected_bytes, authority,
                                           sizeof authority))
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t word = 0;
    for (uint32_t i = 0; i < sizeof authority; ++i) {
        uint32_t at = authority_slot + i;
        if ((!i || !(at & 1u)) && !io->flash.read16(io->flash.context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        if ((uint8_t)(word >> (8u * (at & 1u))) != authority[i])
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return hpclang_rpl_flash_sector_replay_expected(io, journal_slot, expected, expected_bytes,
                                                    scratch, capacity);
}
