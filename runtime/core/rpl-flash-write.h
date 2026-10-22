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
 * A CRC-valid library at the final allocation may likewise be abandoned only
 * when its hidden status is a strict NOR predecessor of live 53. First clear
 * metadata, then a required live bit, then the remaining status bits; retry
 * recognizes the zero-metadata bounded tail. Preserve its payload and never
 * publish it. Live libraries, partially removed libraries, malformed bodies
 * and hidden libraries followed by another allocation are not discarded.
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

/* Store a library at the caller-selected exact erased tail. Revalidate every
 * bank, ID absence and the selected tail/capacity before attempting a write.
 * A gap, interior address or insufficient selected tail is rejected even when
 * another bank has room. Same append failure/cleanup and exclusive-mapping
 * contract as create_library. No cross-port exclusion, registration, durable
 * ownership or startup permission is inferred from the supplied coordinates. */
int hpclang_rpl_flash_create_library_at(const struct hpclang_rpl_flash_io *io,
                                        const uint8_t *record, uint32_t bytes, uint32_t slot);

/* Read-only selection for a complete encoded library record. Validate every
 * Port 2 bank/allocation/tail and require its ID to be absent from that port,
 * including unregistered libraries. Return the same erased slot creation would
 * select under the same stable view; program16 may be NULL. Output changes only
 * on WRITTEN. Uses create_library's INVALID/FAILED/DIRTY/EXISTS/NOSPACE results.
 * This neither reserves storage nor proves cross-port/catalog uniqueness,
 * registration, startup priority or ownership of an existing matching library.
 * An installer must retain exclusive ownership and revalidate after changing
 * its encoded record; the selected address alone never authorizes a write. */
int hpclang_rpl_flash_plan_library(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                   uint32_t bytes, uint32_t *slot);

struct hpclang_rpl_flash_recovery_space
{
    uint32_t library_slot, journal_slot, authority_slot;
};

/* Read-only placement of a fresh native-aligned library and one recovery pair.
 * Validate all banks and ID absence, then allocate each extent from an erased
 * tail, without holes or overlap with target's 4 KiB sector. Search all bank
 * combinations; the pair need not share the library's bank. Output changes only
 * on success. This reserves nothing and does not seal descriptors, establish
 * cross-port exclusion, authorize publication or prove safe startup ordering. */
int hpclang_rpl_flash_plan_recovery_space(const struct hpclang_rpl_flash_io *io,
                                          const uint8_t *record, uint32_t bytes, uint32_t target,
                                          struct hpclang_rpl_flash_recovery_space *space);

/* Same complete-bank planning, including a separate 240-byte creation claim.
 * Search alternate placements when the first three extents leave no claim tail.
 * Both disjoint outputs change only on success. No storage is reserved and no
 * publication, cross-port exclusion or creation provenance is established. */
int hpclang_rpl_flash_plan_creation_space(const struct hpclang_rpl_flash_io *io,
                                          const uint8_t *record, uint32_t bytes, uint32_t target,
                                          struct hpclang_rpl_flash_recovery_space *space,
                                          uint32_t *claim_slot);

/* Resolve one installed library against the complete expected native record.
 * Validate all six bank identities, live records and erased tails, then require
 * a unique matching library ID, exact allocation length/header and complete
 * library object. Padding outside the object's extent is not identity: native
 * STO may zero the unused high nibble where the SDK encoder writes F. Deleted or
 * nested copies do not count. Returns WRITTEN (0) with slot only on success,
 * NOT_FOUND when absent, DIRTY for ambiguity or changed content, and the usual
 * INVALID/FAILED results. program16 may be NULL: this never writes or recovers.
 * Keep the expected record and mapped port stable throughout. The returned
 * physical address is an installation witness, not a persistent ownership
 * token, catalog/attachment check or authorization to erase/replay anything. */
int hpclang_rpl_flash_match_library(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                                    uint32_t bytes, uint32_t *slot);

struct hpclang_rpl_flash_catalog
{
    const struct hpclang_rpl_reader *reader;
    uint32_t address, nibbles, flash_table;
};

/* Also require one native catalog binding to the exact matched library header
 * and bank. The caller supplies a validated Stock catalog and ROM Flash-switch
 * table (16 seven-nibble entries, user banks 8..13); the reader maps both.
 * Compare actual table contents, requiring a unique switch match, rather than
 * assuming routine addresses form an arithmetic sequence. Keep all mappings,
 * expected bytes and catalog stable throughout. Output changes only on success.
 * No writes, registration, attachment, persistent ownership or recovery entry
 * are established; this verifies an installation before starting a transaction.
 * Same result codes as match_library; an absent catalog entry is NOT_FOUND. */
int hpclang_rpl_flash_match_installation(const struct hpclang_rpl_flash_io *io,
                                         const uint8_t *record, uint32_t bytes,
                                         const struct hpclang_rpl_flash_catalog *catalog,
                                         uint32_t *slot);

/* Locate one live library ID after validating all banks, allocations/tails and
 * its unique catalog/bank binding. This does not compare expected content:
 * identity consumers must additionally validate the complete library. Deleted
 * or nested copies are not live installations. Read-only; slot changes only on
 * success. Keep the same stable mapping/ownership for subsequent content checks.
 * Same result codes as match_installation; no transaction authority is granted. */
int hpclang_rpl_flash_locate_installation(const struct hpclang_rpl_flash_io *io,
                                          uint32_t library_number,
                                          const struct hpclang_rpl_flash_catalog *catalog,
                                          uint32_t *slot);

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
 * Discovery and whole-port preflight are separate operations; publication
 * does not establish automatic startup recovery. */
int hpclang_rpl_flash_publish_sector(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                     const uint8_t *record, uint32_t bytes);

/* Resume publication of the SAME caller-owned sector journal, using the exact
 * original READY capsule retained/reconstructed from trusted installation state.
 * FF permits NOR-compatible partial body bytes; 7F/FE/7E require the complete
 * body already equal. Validate all bytes and the still-erased next end marker
 * before writing anything, then verify the body before publishing READY.
 * READY is idempotent; retiring/retired records never become READY again.
 * Only the journal allocation can change; this does not erase or touch target
 * data, publish authority, or authorize replay. DIRTY retains ownership/retry.
 * Use before publishing a following adjacent allocation: an occupied end marker
 * rejects even an otherwise complete journal. After authority publication use
 * the bound lifecycle checks instead. Never infer ownership from compatible
 * bits, scan for a convenient capsule, or substitute a newly generated image.
 */
int hpclang_rpl_flash_finish_sector(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                    const uint8_t *record, uint32_t bytes);

/* Bind one caller-reserved erased authority allocation to an already durable
 * ready journal. Verify the complete journal, then publish/verify the binding
 * body before its ARMED status. Neither journal nor target is modified.
 * The expected ready journal remains stable in RAM. Caller owns the allocation
 * boundary, trusted root and transaction exclusion, including after reset.
 * FAILED/OCCUPIED/INVALID before writing change nothing. DIRTY retains ownership;
 * no target operation may start unless publication succeeded. Partial binding
 * publication uses finish_authority below; this entry requires erased storage.
 * This is not an installer, root replacement or startup discovery. */
int hpclang_rpl_flash_publish_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                        uint32_t journal_slot, const uint8_t *journal,
                                        uint32_t journal_bytes);

/* Store the same binding body but stop at durable status 7F (UNPUBLISHED).
 * Requires the same owned erased allocation and exact READY journal as publish.
 * Target and journal remain unchanged; replay/preflight must still reject this
 * phase. Use finish_staged_authority for a cut during staging, then explicitly
 * finish_authority only after the installer's resource/ownership prerequisites.
 * A complete 7F record is not itself durable installation provenance or RAM
 * ownership: an interrupted older direct publication can have the same bytes. */
int hpclang_rpl_flash_stage_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                      uint32_t journal_slot, const uint8_t *journal,
                                      uint32_t journal_bytes);

/* Retry this SAME owned staging attempt without enabling it. Only FF with a
 * compatible partial body or 7F with the complete expected body is accepted.
 * A repeated complete stage makes no writes. Reject FE/ARMED/CLOSING/CLOSED
 * without writes; never reinterpret activation as a fresh staging attempt.
 * Validates the whole body and erased end marker before its first write.
 * DIRTY retains ownership. This does not reconstruct a trusted claim after reset. */
int hpclang_rpl_flash_finish_staged_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                              uint32_t journal_slot, const uint8_t *journal,
                                              uint32_t journal_bytes);

/* Resume this SAME reserved transaction after a publication cut, including a
 * lost acknowledgement of ARMED. Requires the exact durable READY journal and
 * trusted root reservation used for the original attempt, not a newly selected
 * or previously closed allocation. FF may contain a NOR-compatible partial
 * body; 7F/FE/7E require the entire body to match already. Verify the whole
 * allocation and erased end marker before programming/verifying missing body
 * bits, then publish/verify ARMED. CLOSING/CLOSED never become writable again.
 * A DIRTY return retains ownership and prohibits starting the target mutation.
 * Reconstructing the trusted reservation after reset remains the installer's
 * responsibility; byte compatibility alone is not ownership or freshness.
 */
int hpclang_rpl_flash_finish_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                       uint32_t journal_slot, const uint8_t *journal,
                                       uint32_t journal_bytes);

/* Close a known transaction only after its journal is fully retired and the
 * target still equals its desired image. Compare the whole durable binding
 * and journal against the original READY journal in RAM before clearing the
 * authority status. Accept ARMED/CLOSING/CLOSED for an idempotent close retry;
 * never accept unpublished or incompatible state. Only that status may change.
 * DIRTY requires continued ownership even on read/preflight failure. The caller
 * must separately finish reference repair and native return obligations.
 * CLOSED prohibits replay; this does not authorize erasing/reusing the binding.
 */
int hpclang_rpl_flash_close_authority(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                                      uint32_t journal_slot, const uint8_t *journal,
                                      uint32_t journal_bytes);

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
 * require an erased tail and at most one ready/retiring sector journal. Unknown
 * active transactions and partial publication fail closed. Returns sector phase
 * READY (1), RETIRING (2), 0 absent or a negative write result; slot/target change
 * only on positive results. Retired records are skipped as native tombstones. scratch
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
