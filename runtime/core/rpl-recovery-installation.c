#include "rpl-recovery-installation.h"
#include <stddef.h>

struct object_reader
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t first;
};

static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    const struct object_reader *reader = context;
    if (at / 2u >= reader->io->end - reader->first)
        return 0;
    uint32_t address = reader->first + at / 2u;
    uint16_t word;
    if (!reader->io->read16(reader->io->context, address & ~1u, &word))
        return 0;
    *value = (uint8_t)((word >> (8u * (address & 1u) + 4u * (at & 1u))) & 15u);
    return 1;
}

int hpclang_rpl_recovery_installation_match(const struct hpclang_rpl_flash_io *io,
                                            uint32_t library_number,
                                            const struct hpclang_rpl_flash_catalog *catalog,
                                            const uint8_t expected[48], uint32_t *slot)
{
    if (!expected || !slot)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t found;
    int result = hpclang_rpl_flash_locate_installation(io, library_number, catalog, &found);
    if (result)
        return result;
    struct object_reader mapped = {io, found + 4u};
    const struct hpclang_rpl_reader reader = {read_nibble, &mapped};
    result = hpclang_rpl_recovery_identity_match(&reader, 0, expected);
    if (result != 1)
        return result < 0 ? HPCLANG_RPL_FLASH_FAILED : HPCLANG_RPL_FLASH_DIRTY;
    *slot = found;
    return 0;
}

struct restored_reader
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t target, journal, authority;
    const uint8_t *image;
};

static int restored_read(void *context, uint32_t at, uint16_t *value)
{
    const struct restored_reader *view = context;
    if (view->image && at >= view->target && at - view->target < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
        uint32_t offset = at - view->target;
        *value = view->image[offset] | (uint16_t)view->image[offset + 1] << 8;
    } else if (!view->io->read16(view->io->context, at, value)) {
        return 0;
    }
    if (at == (view->journal & ~1u))
        *value &= (uint16_t)~(255u << (8 * (view->journal & 1)));
    if (at == (view->authority & ~1u))
        *value &= (uint16_t)~(255u << (8 * (view->authority & 1)));
    return 1;
}

int hpclang_rpl_recovery_installation_preflight(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected[48], uint32_t authority,
    uint32_t journal, uint8_t *scratch, uint32_t capacity,
    struct hpclang_rpl_flash_installed_recovery *installed)
{
    if (!installed || !expected)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t target = 0;
    int phase =
        hpclang_rpl_flash_preflight_bound(io, authority, journal, scratch, capacity, &target);
    if (phase < 0)
        return phase;
    enum hpclang_rpl_flash_sector_phase journal_phase;
    const uint8_t *image;
    if (hpclang_rpl_flash_sector_inspect(io->first, io->end, journal, scratch,
                                         HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &target, &image,
                                         &journal_phase))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct restored_reader view = {io, target, journal, authority, phase ? image : NULL};
    const struct hpclang_rpl_flash_io restored = {&view, restored_read, NULL, io->first, io->end};
    uint32_t library;
    int result = hpclang_rpl_recovery_installation_match(&restored, library_number, catalog,
                                                         expected, &library);
    if (result)
        return result;
    /* Read the actual allocation extent, including native padding. A valid
     * object supplied only by the journal cannot serve as its recovery root. */
    struct object_reader allocation = {&restored, library};
    uint32_t extent = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!read_nibble(&allocation, 2 + i, &nibble))
            return HPCLANG_RPL_FLASH_FAILED;
        extent |= (uint32_t)nibble << (4 * i);
    }
    uint32_t bytes = (extent + 2) / 2;
    if (library < target + HPCLANG_RPL_FLASH_SECTOR_BYTES && target < library + bytes)
        return HPCLANG_RPL_FLASH_DIRTY;
    *installed = (struct hpclang_rpl_flash_installed_recovery){library, journal, authority, target};
    return phase;
}
