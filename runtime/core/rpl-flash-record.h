#ifndef HPCLANG_RPL_FLASH_RECORD_H
#define HPCLANG_RPL_FLASH_RECORD_H

#include "rpl-backup.h"

/* Encode the allocation envelope around an exact, CRC-validated backup.
 * Coordinates in the input reader are nibbles; the owned output is bytes.
 * Matches native HP49/50 STO records: status 53, relative extent, metadata 1,
 * packed object, then erased padding to a whole number of halfwords.
 * This only builds bytes; it neither reserves space nor commits a record.
 * The source stays stable throughout validation/copy. Outputs are unchanged
 * on failure. Free *record after use. */
int hpclang_rpl_flash_backup_record(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t span, uint8_t **record, uint32_t *bytes);

struct hpclang_rpl_flash_space
{
    uint32_t address;
    uint32_t nibbles;
};

struct hpclang_rpl_flash_allocation
{
    uint32_t address;
    uint32_t nibbles;
    int deleted;
};

/* Locate the allocation containing target, including interior addresses,
 * headers and padding. Deleted allocations remain candidates: native PURGE
 * does not invalidate outstanding references. Validate the entire chain and
 * erased tail before reporting a match. Deleted payloads are opaque and need
 * not contain a valid object. Caller supplies one stable, validated bank
 * mapping; all coordinates are nibbles in that mapping. Returns 1 found,
 * 0 outside allocations, or a negative span result. Output changes only on 1.
 * This establishes ownership bounds, not permission to reclaim or relocate. */
int hpclang_rpl_flash_allocation_find(const struct hpclang_rpl_reader *reader, uint32_t first,
                                      uint32_t end, uint32_t target,
                                      struct hpclang_rpl_flash_allocation *allocation);

/* Find the erased tail after validated live/deleted records. first/end are
 * nibble coordinates after a caller-validated bank header. nibbles excludes
 * the final erased status byte and is rounded down to a whole halfword.
 * Check the entire tail: an FF status followed by partially programmed data
 * is not free space. Such interrupted writes require recovery, not overwrite.
 * No mutation or reservation occurs; mapping/data must remain stable through
 * the eventual commit. Output is unchanged on failure. */
int hpclang_rpl_flash_tail(const struct hpclang_rpl_reader *reader, uint32_t first, uint32_t end,
                           struct hpclang_rpl_flash_space *space);

struct hpclang_rpl_flash_discard
{
    uint32_t address;
    uint32_t extent;
};

/* Plan rollback of data behind an FF end marker into a deleted allocation.
 * The extent covers every programmed nibble without requiring any 0-to-1 bit
 * transition in its five-nibble field. The caller must program/verify that
 * field before clearing the status byte to 00, preserving neighboring bytes.
 * Returns 1 with a plan, 0 for an already clean tail, or a negative span result.
 * Output changes only on 1. This neither writes nor reclaims space, and does
 * not handle interrupted status writes, unknown slot states or sector erase.
 * The bank must remain stable from planning through commit. */
int hpclang_rpl_flash_discard_plan(const struct hpclang_rpl_reader *reader, uint32_t first,
                                   uint32_t end, struct hpclang_rpl_flash_discard *plan);

#endif
