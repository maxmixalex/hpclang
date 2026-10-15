#include "rpl-flash-replay.h"
#include <stddef.h>

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
    int damaged = 0;
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
        if (result == 1) {
            if (journal != io->end)
                return HPCLANG_RPL_FLASH_DIRTY;
            journal = found;
            destination = candidate_target;
        }
    }
    if (journal == io->end)
        return damaged ? HPCLANG_RPL_FLASH_DIRTY : 0;
    /* Later discovery may have overwritten scratch. Reload the selected image
     * and keep it separate from the validation scanner's workspace. */
    uint32_t found, candidate_target;
    int result = hpclang_rpl_flash_find_sector(io, journal & ~0x1ffffu, scratch, capacity, &found,
                                               &candidate_target);
    if (result != 1 || found != journal || candidate_target != destination)
        return result < 0 ? result : HPCLANG_RPL_FLASH_DIRTY;
    struct sector_overlay overlay = {io, destination, scratch + 24u};
    const struct hpclang_rpl_flash_io restored = {&overlay, overlay_read, NULL, io->first, io->end};
    unsigned journals = 0;
    for (uint32_t bank = io->first; bank < io->end; bank += 0x20000u) {
        result = hpclang_rpl_flash_find_sector(
            &restored, bank, scratch + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
            HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &found, &candidate_target);
        if (result < 0)
            return result;
        if (result == 1) {
            if (found != journal || candidate_target != destination || ++journals != 1)
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    if (journals != 1)
        return HPCLANG_RPL_FLASH_DIRTY;
    *slot = journal;
    *target = destination;
    return 1;
}

int hpclang_rpl_flash_sector_replay(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                                    uint8_t *scratch, uint32_t capacity)
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
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        uint16_t expected = image_word(image, offset);
        if (expected != UINT16_MAX && !flash->program16(flash->context, target + offset, expected))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        if (!flash->read16(flash->context, target + offset, &word) ||
            word != image_word(image, offset))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}
