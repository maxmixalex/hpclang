#ifndef HPCLANG_RPL_FLASH_WRITE_H
#define HPCLANG_RPL_FLASH_WRITE_H

#include <stdint.h>
#include "rpl-flash-record.h"

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
    HPCLANG_RPL_FLASH_UNSUPPORTED = -5,
    HPCLANG_RPL_FLASH_EXISTS = -6,
    HPCLANG_RPL_FLASH_NOSPACE = -7,
    HPCLANG_RPL_FLASH_NOT_FOUND = -8
};

/* Append one CRC-valid native backup/library allocation into a reserved erased tail.
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
 * verify it with zero metadata before publishing status 00. Preserve all bytes
 * outside that four-byte discard header. Returns
 * WRITTEN for a clean or recovered tail, INVALID for bad arguments, DIRTY if
 * recovery cannot be verified. DIRTY must not be exposed to native STO.
 * Own the mapping/exclude writers throughout. No erase or space reclamation;
 * unknown/partially programmed status bytes are rejected without mutation.
 * This is a recovery primitive, not a complete power-loss transaction. */
int hpclang_rpl_flash_recover_tail(const struct hpclang_rpl_flash_io *io, uint32_t first,
                                   uint32_t end);

/* Recover interrupted backups and erased-marker tails across all six user banks. Validate every
 * bank identity and discard plan before the first write. Invalid/unknown states
 * leave the whole port unchanged. Once commit starts, a backend failure may
 * leave earlier banks recovered; retry under the same exclusive ownership.
 * FF tails are discarded. Backups with any hidden non-FF status value
 * are tombstoned only after validating their complete CRC, extent, metadata and
 * padding. Their bodies remain unchanged and never become visible.
 * Zero metadata with an even hidden status identifies an interrupted discard
 * at the bank tail, accepted only with a bounded halfword allocation covering
 * the revalidated discard plan and an erased remainder.
 * This permits repeated recovery cuts without retaining an in-memory plan.
 * A CRC-valid replacement intent can be retired before replacement starts:
 * require the original live backup/extent/CRC and an entirely erased new slot.
 * A committed new backup requires both exact allocation starts, lengths,
 * checksums and matching names; recovery finishes old-status deletion before
 * retiring the intent. Only one active intent is accepted per port. Changed
 * targets and impossible phase ordering remain DIRTY. An incomplete new body
 * behind FF is discarded using the exact reservation and zero metadata while
 * keeping the old value live; repeated cuts resume from this discard marker.
 * Resolve the intent before ordinary tail cleanup, regardless of bank order.
 * A partially published new status compatible with FF-to-53 requires a ready
 * intent, live old value and fully verified new body; finish its publication
 * before deleting the old value. This excludes FF, which rolls back instead.
 * Clearing a hidden status
 * first clears and verifies a required live bit, so interruption cannot publish
 * the record. Partial libraries and other unknown encodings remain DIRTY.
 * This cannot infer replacement intent or perform sector erasure, and does not
 * implement automatic startup recovery.
 * The IO extent is the same six-bank extent used by create_backup. */
int hpclang_rpl_flash_recover_tails(const struct hpclang_rpl_flash_io *io);

/* Prepare a whole port for a normal mutation under continued exclusive ownership.
 * Preflight errors return without writes. Once any programming is attempted,
 * retry recovery until the native chain is verified; never release ownership
 * with uncertain cleanup. Persistent hardware failure consequently does not
 * return, matching the Stock backend's append/remove cleanup policy. */
int hpclang_rpl_flash_prepare_write(const struct hpclang_rpl_flash_io *io);

/* Finish a known backup/library commit after interruption/uncertain acknowledgement.
 * The caller retains the exact expected record and owns this reserved slot.
 * Validate that complete record, compare every stored body/padding byte, and
 * accept only status bits compatible with programming 53. Only the status
 * byte may change. WRITTEN includes an already completed commit; FAILED means
 * a preflight read failed without mutation; DIRTY means mismatched contents or
 * an unverified status write. This does not infer transaction intent from a
 * damaged bank: restart integration must supply durable expected contents. */
int hpclang_rpl_flash_finish_commit(const struct hpclang_rpl_flash_io *io, uint32_t address,
                                    const uint8_t *record, uint32_t bytes);

/* Create one native backup across the six user Flash banks. IO spans exactly
 * six 128-KiB banks; its first address corresponds to native bank 8. Validate
 * all bank identities, records and erased tails before mutation, even after
 * finding space. Exact backup names must be unique across the whole port.
 * Existing records stay put; deleted slots are not reclaimed. The caller owns
 * the mapping and excludes other writers throughout selection and append.
 * Uses append's error/dirty contract, plus EXISTS and NOSPACE. No erase or
 * automatic recovery is attempted by this operation. */
int hpclang_rpl_flash_create_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                    uint32_t bytes, uint32_t *attempted_slot);

/* Same selection/append contract as create_backup, with library-ID uniqueness
 * across all six banks. Stores the record only; no registration or attachment.
 * Each typed create function rejects the other object's encoding. */
int hpclang_rpl_flash_create_library(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                     uint32_t bytes, uint32_t *attempted_slot);

/* Validate all six banks before deleting one uniquely named backup. Only its
 * status byte changes; payload, references and allocation extents stay put.
 * No erase/reclamation. Missing names return NOT_FOUND; ambiguous names DIRTY.
 * attempted_slot is changed only immediately before clearing the status. Own
 * the mapping throughout, including finish_remove after an uncertain write. */
int hpclang_rpl_flash_remove_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *name,
                                    uint32_t name_length, uint32_t *attempted_slot);

/* Complete a caller-owned, previously validated deletion. Accept status 53,
 * a partially cleared subset, or already deleted 00. No other byte changes.
 * DIRTY requires retaining ownership and retrying before native return. */
int hpclang_rpl_flash_finish_remove(const struct hpclang_rpl_flash_io *io, uint32_t address);

struct hpclang_rpl_flash_replacement
{
    uint32_t old_slot, new_slot;
    int committed;
};

struct hpclang_rpl_flash_replace_plan
{
    uint32_t intent_slot;
    struct hpclang_rpl_flash_intent intent;
};

/* Read-only preparation for journaled replacement. Validate the whole port,
 * require exactly one existing backup, and reserve disjoint erased tails for
 * the intent and new record (possibly in different banks). Record both exact
 * allocation lengths and status-independent CRC32 values. No Flash writes or
 * RAM allocation; output changes only on success. Caller must keep the port
 * and source record stable until the eventual journal publication. This does
 * not itself publish an intent. */
int hpclang_rpl_flash_plan_replace(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                   uint32_t bytes, struct hpclang_rpl_flash_replace_plan *plan);

/* Plan and publish the durable intent before touching the replacement value.
 * Writes/reads back the complete body before publishing hidden status 7E.
 * plan is unchanged on preflight failure and receives the attempted transaction
 * immediately before writing. WRITTEN means intent publication only; old/new
 * objects are untouched. DIRTY after a programming/readback failure requires
 * retaining ownership and recovery; do not begin writing the new value unless
 * publication succeeded. */
int hpclang_rpl_flash_publish_intent(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                     uint32_t bytes, struct hpclang_rpl_flash_replace_plan *plan);

/* Publish a sector recovery image into a caller-reserved erased allocation.
 * Validate the encoded record and require its complete span plus end marker
 * to be erased before writing. Verify the body before publishing hidden 7E.
 * Caller proves the allocation boundary, transaction freshness, native image
 * semantics and exclusive ownership; record remains stable in RAM. No erase
 * or target-sector mutation occurs here. WRITTEN permits subsequent replay,
 * not native return or reclamation. FAILED/OCCUPIED preflight changes nothing;
 * DIRTY requires retaining ownership and resolving the attempted journal.
 * Retrying a partially written allocation is not supported by this entry.
 * Whole-port discovery/startup recovery do not yet recognize this format. */
int hpclang_rpl_flash_publish_sector(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                     const uint8_t *record, uint32_t bytes);

/* Retire a caller-owned sector journal only after reference repair and all
 * transaction obligations have completed. record is the original committed
 * image retained in stable RAM. Verify its complete Flash body and the complete
 * target image before clearing status to 00. Accept 7E, a partially cleared
 * subset, or 00 for retry; no publication state or different body is accepted.
 * Only the status byte changes. DIRTY means retain ownership and retry/resolve;
 * even a preflight mismatch cannot authorize Stock return. WRITTEN proves the
 * journal retired with matching target, not reference safety or startup recovery.
 * Caller establishes allocation identity, freshness and exclusive ownership. */
int hpclang_rpl_flash_retire_sector(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                    const uint8_t *record, uint32_t bytes);

/* Read-only discovery in one intact native bank. Validate its identity, walk
 * allocation boundaries (never search payload bytes), validate live objects,
 * require an erased tail and at most one committed sector journal. Unknown
 * active transactions and partial publication fail closed. Returns 1 found,
 * 0 absent or a negative write result; slot/target change only on 1. scratch
 * holds at least SECTOR_RECORD_BYTES. Caller keeps mapping and Flash stable.
 * This locates evidence, not erase authorization: all other banks, freshness,
 * restored-image validity and reference ownership still require validation.
 * Recovery from a damaged target bank needs an intact journal bank; this does
 * not discover records beyond a broken allocation chain or implement startup. */
int hpclang_rpl_flash_find_sector(const struct hpclang_rpl_flash_io *io, uint32_t bank,
                                  uint8_t *scratch, uint32_t capacity, uint32_t *slot,
                                  uint32_t *target);

/* Replace exactly one existing backup without relocating its payload. Validate
 * all banks, publish a durable intent, write the complete replacement, then
 * recover the intent to tombstone the old record and retire the journal.
 * Hold exclusive ownership across the transient duplicate-name interval.
 * Missing/ambiguous names and insufficient erased space do not change Flash.
 * transaction is required and unchanged on preflight rejection. On DIRTY after
 * mutation, finish_replace must resolve the outcome; committed is only set
 * after recovery verifies a published new value and deleted old value. Do not
 * use legacy discard_record cleanup. Initialize new_slot to UINT32_MAX to
 * distinguish preflight DIRTY. No reclamation or automatic startup hook. */
int hpclang_rpl_flash_replace_backup(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                     uint32_t bytes,
                                     struct hpclang_rpl_flash_replacement *transaction);

/* Resolve a caller-owned replacement using durable whole-port recovery.
 * WRITTEN means the new value committed; FAILED means the old value remains.
 * DIRTY means recovery/readback is unresolved and ownership must be retained.
 * The transaction must be from replace_backup and the mapping still owned;
 * replay after restart uses recover_tails without this optional RAM outcome. */
int hpclang_rpl_flash_finish_replace(const struct hpclang_rpl_flash_io *io,
                                     struct hpclang_rpl_flash_replacement *transaction);

/* Discard a caller-owned failed append using its retained expected record.
 * Verify its exact reserved extent before clearing status. If publication has
 * not started (FF status), also verify zero discard metadata first; retries
 * preserve that marker. No payload or other record
 * is touched. Returns WRITTEN only after the deleted slot is verified.
 * A DIRTY result requires retaining ownership/recovery before native return.
 * create_backup writes attempted_slot immediately before append (and leaves
 * it unchanged on preflight rejection), distinguishing an existing malformed
 * bank from the caller's failed transaction. The pointer is optional. */
int hpclang_rpl_flash_discard_record(const struct hpclang_rpl_flash_io *io, uint32_t address,
                                     const uint8_t *record, uint32_t bytes);

#endif
