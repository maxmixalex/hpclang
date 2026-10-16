#ifndef HPCLANG_RPL_FLASH_REPLAY_H
#define HPCLANG_RPL_FLASH_REPLAY_H

#include "rpl-flash-write.h"

struct hpclang_rpl_flash_replay_io
{
    struct hpclang_rpl_flash_io flash;
    int (*erase_sector)(void *context, uint32_t address);
};

/* Read-only whole-port sector-recovery preflight. Discover exactly one journal
 * through intact bank allocation chains, then overlay its image in RAM and
 * validate all six restored bank identities/chains/live CRCs/erased tails.
 * scratch has room for two SECTOR_RECORD_BYTES buffers, disjoint from IO state.
 * Returns phase READY (1), RETIRING (2), 0 for a clean port with no journal, or a
 * negative write result; outputs change only on positive results. RETIRING also
 * requires the current target to match exactly, never permission to replay.
 * The first scratch record is normalized to ready status in RAM for use as
 * retire_sector's expected record; durable status remains unchanged.
 * Multiple journals, unreadable storage
 * and damage outside the image fail closed. Keep Flash stable throughout.
 * Success proves structural consistency only, not transaction freshness,
 * name/ID uniqueness, reference correctness or authority to erase/return to
 * Stock. The journal must remain discoverable without reading a damaged chain;
 * this is not a startup hook or a replacement for the ownership protocol. */
int hpclang_rpl_flash_preflight_sector(const struct hpclang_rpl_flash_io *io, uint8_t *scratch,
                                       uint32_t capacity, uint32_t *slot, uint32_t *target);

/* Read-only preflight for a trusted installed authority/journal pair. Validate
 * both durable records, all restored banks and the authority's exact allocation
 * boundary (not a matching byte sequence inside another record). Return READY
 * for an armed/ready transaction, RETIRING for target-equal cleanup only, or 0
 * for a closed transaction with a retired journal and structurally clean port.
 * Unpublished bindings and incompatible phases fail closed. A closed transaction
 * never overlays its old image on later data. scratch needs two sector records;
 * target changes only on positive results. Installation-root trust, exclusive
 * ownership, startup ordering and reference safety remain caller obligations.
 */
int hpclang_rpl_flash_preflight_bound(const struct hpclang_rpl_flash_io *io,
                                      uint32_t authority_slot, uint32_t journal_slot,
                                      uint8_t *scratch, uint32_t capacity, uint32_t *target);

/* Replay a caller-authorized, fully published sector image from Flash. scratch
 * is caller-owned RAM of at least SECTOR_RECORD_BYTES, disjoint from IO state.
 * Read and validate the entire durable record before any target mutation.
 * If the target already matches, do not erase/program it. Otherwise erase,
 * verify, program and verify the complete target, retaining the journal.
 * For a bank's first sector, leave its identity word erased until every other
 * word verifies, then publish and verify the identity last. This prevents
 * command-boundary cuts from exposing that sector's incomplete chain through
 * a restored bank identity; it does not protect other sectors or a failed erase.
 * Callbacks must leave command mode before returning, even on failure.
 * DIRTY requires continued ownership/retry. INVALID never authorizes Stock
 * return: an earlier erase may already have occurred. WRITTEN establishes only
 * target equality; references, journal retirement and native return are caller
 * obligations. Caller proves the allocation boundary, exclusive ownership,
 * image semantics and transaction freshness. This is not whole-port discovery,
 * durable publication, startup recovery or permission to reclaim native data. */
int hpclang_rpl_flash_sector_replay(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                                    uint8_t *scratch, uint32_t capacity);

/* Replay only the exact ready journal previously inspected/authorized by the
 * caller. Reload and compare all durable bytes before any target mutation.
 * expected is a stable SECTOR_RECORD_BYTES RAM snapshot, disjoint from scratch
 * and IO state. A changed journal returns DIRTY without target writes. This
 * binds replay to the inspected image; it does not establish transaction
 * freshness or authority, nor relax any ownership/return rules above. */
int hpclang_rpl_flash_sector_replay_expected(const struct hpclang_rpl_flash_replay_io *io,
                                             uint32_t slot, const uint8_t *expected,
                                             uint32_t expected_bytes, uint8_t *scratch,
                                             uint32_t capacity);

/* Require an exact, durably ARMED one-use binding at authority_slot before
 * entering expected-journal replay. Recheck the complete durable journal too.
 * An unpublished, closing, closed, changed or unreadable binding causes no
 * target mutation. The caller supplies the trusted root's allocation address;
 * scanning for a convenient matching binding is not permitted. Keep exclusive
 * ownership throughout; no other Stock writer may have run since arming.
 * Native image/reference validation, startup ordering and root installation
 * remain caller obligations. Success does not retire or close either record.
 * Same expected/scratch separation and retry obligations as replay_expected.
 */
int hpclang_rpl_flash_sector_replay_bound(const struct hpclang_rpl_flash_replay_io *io,
                                          uint32_t authority_slot, uint32_t journal_slot,
                                          const uint8_t *expected, uint32_t expected_bytes,
                                          uint8_t *scratch, uint32_t capacity);

#endif
