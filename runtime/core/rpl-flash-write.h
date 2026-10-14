#ifndef HPCLANG_RPL_FLASH_WRITE_H
#define HPCLANG_RPL_FLASH_WRITE_H

#include <stdint.h>

struct hpclang_rpl_flash_io
{
    void *context;
    int (*read16)(void *context, uint32_t address, uint16_t *value);
    int (*program16)(void *context, uint32_t address, uint16_t value);
    uint32_t first, end;
};

enum hpclang_rpl_flash_write_result
{
    HPCLANG_RPL_FLASH_WRITTEN = 0,
    HPCLANG_RPL_FLASH_INVALID = -1,
    HPCLANG_RPL_FLASH_OCCUPIED = -2,
    HPCLANG_RPL_FLASH_FAILED = -3,
    HPCLANG_RPL_FLASH_DIRTY = -4,
    HPCLANG_RPL_FLASH_UNSUPPORTED = -5
};

/* Append one CRC-valid native backup allocation into a reserved erased tail.
 * All coordinates are bytes. IO bounds are even and exclude firmware. Callbacks
 * return 1 on success; program16 may partially clear requested bits on failure,
 * but must wait for the chip to leave command mode before returning. The caller
 * owns the mapping and excludes other writers throughout this operation.
 * record stays stable in RAM. The native tail and name uniqueness must already
 * be validated; this is not the public Port create/replace service.
 * FAILED means no write occurred or the failed append was discarded. DIRTY
 * means cleanup could not be verified: do not return that bank to native STO.
 * No sector is erased. This covers returned IO errors, not power-loss recovery. */
int hpclang_rpl_flash_append(const struct hpclang_rpl_flash_io *io, uint32_t address,
                             const uint8_t *record, uint32_t bytes);

#endif
