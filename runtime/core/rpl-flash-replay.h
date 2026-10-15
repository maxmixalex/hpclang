#ifndef HPCLANG_RPL_FLASH_REPLAY_H
#define HPCLANG_RPL_FLASH_REPLAY_H

#include "rpl-flash-write.h"

struct hpclang_rpl_flash_replay_io
{
    struct hpclang_rpl_flash_io flash;
    int (*erase_sector)(void *context, uint32_t address);
};

/* Replay a caller-authorized, fully published sector image from Flash. scratch
 * is caller-owned RAM of at least SECTOR_RECORD_BYTES, disjoint from IO state.
 * Read and validate the entire durable record before any target mutation.
 * If the target already matches, do not erase/program it. Otherwise erase,
 * verify, program and verify the complete target, retaining the journal.
 * Callbacks must leave command mode before returning, even on failure.
 * DIRTY requires continued ownership/retry. INVALID never authorizes Stock
 * return: an earlier erase may already have occurred. WRITTEN establishes only
 * target equality; references, journal retirement and native return are caller
 * obligations. Caller proves the allocation boundary, exclusive ownership,
 * image semantics and transaction freshness. This is not whole-port discovery,
 * durable publication, startup recovery or permission to reclaim native data. */
int hpclang_rpl_flash_sector_replay(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                                    uint8_t *scratch, uint32_t capacity);

#endif
