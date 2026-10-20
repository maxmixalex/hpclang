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

/* Validate the exact native allocation boundaries of an already loaded bound
 * pair, including their bank identities, allocation chains and erased tails.
 * Snapshots and IO must remain stable and describe the same durable bytes.
 * A pending journal overlays its target only for this read-only structural
 * check; a retired journal never overlays later data. Returns 0 on success.
 * No extra sector buffer or Flash writes. This is suitable before releasing
 * recovery storage, but does not prove installed identity, freshness, RAM
 * ownership, global port consistency or permission to replay/return to Stock. */
int hpclang_rpl_flash_bound_membership(const struct hpclang_rpl_flash_io *io,
                                       uint32_t authority_slot, const uint8_t *authority,
                                       uint32_t authority_bytes, uint32_t journal_slot,
                                       const uint8_t *journal, uint32_t journal_bytes);

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

struct hpclang_rpl_flash_recovery_layout
{
    const uint8_t *record;
    uint32_t bytes;
    /* Nibble offsets in the expected native record, obtained from owned ELF
     * packaging, not signature searches in untrusted Flash. */
    uint32_t recovery_binding, admission_binding;
};

struct hpclang_rpl_flash_installed_recovery
{
    uint32_t library_slot, journal_slot, authority_slot, target;
};

/* Read-only initial transaction planning, before publishing either record.
 * Derive both slots from the owned installed layout. Require exact library and
 * catalog identity on intact Flash, untouched journal/authority storage and a
 * valid native allocation chain after virtually adding both record envelopes.
 * Validate the proposed journal's desired target image too; the installed root
 * must survive outside that target sector. Holes in an erased tail, interior
 * erased bytes of existing allocations and previously used slots are rejected.
 * journal is the caller's complete READY capsule for the derived slot.
 * Return 0 and publish output only on success. No writes or heap allocation.
 *
 * Inputs and IO remain stable and disjoint from output throughout. This neither
 * reserves the slots against another writer nor creates/owns recovery RAM.
 * Namespace exclusion, durable resource ownership, reference repair, startup
 * order and transaction publication remain required before any target write.
 * This initial planner cannot resume partial publication or reuse old records.
 */
int hpclang_rpl_flash_plan_installed(const struct hpclang_rpl_flash_io *io,
                                     const struct hpclang_rpl_flash_recovery_layout *layout,
                                     const struct hpclang_rpl_flash_catalog *catalog,
                                     const uint8_t *journal, uint32_t journal_bytes,
                                     struct hpclang_rpl_flash_installed_recovery *installed);

/* Cancel an unpublished transaction under the SAME trusted one-use installation
 * claim, without needing the lost desired image. Caller must have established
 * initial empty-slot ownership and prevented reuse/other writers since then.
 * Derive both slots from the owned layout; validate the installed library/catalog
 * and exact allocation boundaries with only the two envelopes virtually made
 * into tombstones. Reject an armed authority or ordinary retired transaction.
 * Finish/verify the journal's zero-metadata tombstone before cancelling authority.
 * This ordering also permits retry after a torn cancellation status write.
 * Clear a native live-status bit before clearing remaining status bits, so a
 * cancellation cut cannot expose an incomplete native live object.
 *
 * Only the two four-byte envelopes change. No erase, target write, payload
 * destruction, RAM release, allocation reuse or root retirement. Return 0 only
 * after both tombstones and the actual installed library/catalog validate;
 * DIRTY requires retaining ownership and retry/recovery, never permission to
 * return to Stock. Keep expected layout/catalog/mappings stable throughout.
 * Matching bytes or a discoverable library alone do not establish the required
 * durable claim; this does not supply startup ordering or a public installer.
 */
int hpclang_rpl_flash_cancel_installed(const struct hpclang_rpl_flash_io *io,
                                       const struct hpclang_rpl_flash_recovery_layout *layout,
                                       const struct hpclang_rpl_flash_catalog *catalog);

/* Connect exact installed library content/catalog identity to its two HPCBIND1
 * descriptors and the bound transaction preflight. Both descriptors must agree.
 * Validate the library against the restored port view for a pending transaction;
 * require its entire allocation outside the target sector, so the journal cannot
 * supply missing installation bytes. Closed transactions use actual Flash only.
 * Return preflight_bound's READY/RETIRING/closed result; output changes only on
 * success (including closed). Scratch has the same two-record requirement.
 * Expected record/layout, mappings and catalog are trusted stable caller inputs,
 * disjoint from scratch/output. This is read-only structural/identity validation,
 * not root publication, freshness, RAM ownership or startup-order authorization.
 */
int hpclang_rpl_flash_preflight_installed(const struct hpclang_rpl_flash_io *io,
                                          const struct hpclang_rpl_flash_recovery_layout *layout,
                                          const struct hpclang_rpl_flash_catalog *catalog,
                                          uint8_t *scratch, uint32_t capacity,
                                          struct hpclang_rpl_flash_installed_recovery *installed);

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
