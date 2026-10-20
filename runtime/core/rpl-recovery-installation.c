#include "rpl-recovery-installation.h"

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
