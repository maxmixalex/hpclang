#include "rpl-flash-replay.h"

static uint16_t image_word(const uint8_t *image, uint32_t offset)
{
    return (uint16_t)(image[offset] | (uint16_t)image[offset + 1u] << 8u);
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
