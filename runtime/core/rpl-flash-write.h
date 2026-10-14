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

/* Recover an interrupted body behind an erased FF end marker. first/end are
 * byte bounds of one caller-validated bank's allocation chain, excluding its
 * header. Validate all preceding records, finish a NOR-compatible extent and
 * verify it before publishing status 00. Preserve all other bytes. Returns
 * WRITTEN for a clean or recovered tail, INVALID for bad arguments, DIRTY if
 * recovery cannot be verified. DIRTY must not be exposed to native STO.
 * Own the mapping/exclude writers throughout. No erase or space reclamation;
 * unknown/partially programmed status bytes are rejected without mutation.
 * This is a recovery primitive, not a complete power-loss transaction. */
int hpclang_rpl_flash_recover_tail(const struct hpclang_rpl_flash_io *io, uint32_t first,
                                   uint32_t end);

/* Finish a known backup commit after interruption/uncertain acknowledgement.
 * The caller retains the exact expected record and owns this reserved slot.
 * Validate that complete record, compare every stored body/padding byte, and
 * accept only status bits compatible with programming 53. Only the status
 * byte may change. WRITTEN includes an already completed commit; FAILED means
 * a preflight read failed without mutation; DIRTY means mismatched contents or
 * an unverified status write. This does not infer transaction intent from a
 * damaged bank: restart integration must supply durable expected contents. */
int hpclang_rpl_flash_finish_commit(const struct hpclang_rpl_flash_io *io, uint32_t address,
                                    const uint8_t *record, uint32_t bytes);

#endif
