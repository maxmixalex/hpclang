#ifndef HPCLANG_RPL_FLASH_RECORD_H
#define HPCLANG_RPL_FLASH_RECORD_H

#include "rpl-backup.h"

#define HPCLANG_RPL_FLASH_INTENT_BYTES 44u

struct hpclang_rpl_flash_intent
{
    uint32_t old_slot, new_slot, old_bytes, new_bytes, old_crc, new_crc;
};

enum hpclang_rpl_flash_intent_phase
{
    HPCLANG_RPL_FLASH_INTENT_UNPUBLISHED,
    HPCLANG_RPL_FLASH_INTENT_READY,
    HPCLANG_RPL_FLASH_INTENT_RETIRING,
    HPCLANG_RPL_FLASH_INTENT_RETIRED
};

/* Internal replacement-intent wire format used by backup replacement.
 * A 44-byte native allocation uses metadata 2 and hidden committed status 7E.
 * The body contains HPCREPL1, its own physical slot, both allocation addresses/
 * lengths/checksums, and a CRC32 over every byte except status and that CRC.
 * All integers are little-endian. Bounds cover exactly six native user banks.
 * The three allocations must be disjoint, exclude headers and remain within
 * their respective banks with space for an end marker. No Flash is changed.
 * Decode classifies publication/retirement cuts but cannot authorize recovery:
 * callers must separately validate the referenced records and transaction state.
 * Outputs remain unchanged on error. */
int hpclang_rpl_flash_intent_encode(uint32_t first, uint32_t end, uint32_t slot,
                                    const struct hpclang_rpl_flash_intent *intent, uint8_t *data,
                                    uint32_t capacity);
int hpclang_rpl_flash_intent_decode(uint32_t first, uint32_t end, uint32_t slot,
                                    const uint8_t *data, uint32_t bytes,
                                    struct hpclang_rpl_flash_intent *intent,
                                    enum hpclang_rpl_flash_intent_phase *phase);

#define HPCLANG_RPL_FLASH_SECTOR_BYTES 4096u
#define HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES (HPCLANG_RPL_FLASH_SECTOR_BYTES + 28u)

#define HPCLANG_RPL_FLASH_AUTHORITY_BYTES 32u

enum hpclang_rpl_flash_authority_phase
{
    HPCLANG_RPL_FLASH_AUTHORITY_UNPUBLISHED,
    HPCLANG_RPL_FLASH_AUTHORITY_ARMED,
    HPCLANG_RPL_FLASH_AUTHORITY_CLOSING,
    HPCLANG_RPL_FLASH_AUTHORITY_CLOSED
};

/* Internal one-use transaction binding, metadata 4 / HPCAUTH1. Binds its own
 * physical allocation to one validated sector journal's address, target and
 * status-independent CRC. Encode accepts only a ready journal; inspect also
 * accepts journal retirement cuts so cleanup can finish without replay.
 * FF/7F/FE are unpublished, 7E armed, proper subsets of 7E closing, 00 closed.
 * Only ARMED can be considered for replay; every other phase forbids it.
 * ARMED never overrides a journal's own RETIRING/RETIRED replay prohibition.
 *
 * Memory-only codecs, not a durable authority installer or erase permission.
 * The caller must locate this allocation through a trusted installation root,
 * publish/verify it before target writes, exclude Stock writers until journal
 * retirement/reference repair and closure, and gate startup before dependent
 * callbacks. Finding arbitrary matching bytes does not establish freshness.
 * Never erase/reuse this allocation while any bound journal can survive:
 * interrupted erasure could resurrect an old ARMED status. Root replacement
 * and reclamation require a separate protocol, not a counter in this record.
 * Allocation and journal must not overlap each other or the target sector.
 * Outputs remain unchanged on failure. Coordinates are physical byte offsets.
 */
int hpclang_rpl_flash_authority_encode(uint32_t first, uint32_t end, uint32_t slot,
                                       uint32_t journal_slot, const uint8_t *journal,
                                       uint32_t journal_bytes, uint8_t *data, uint32_t capacity);
int hpclang_rpl_flash_authority_inspect(uint32_t first, uint32_t end, uint32_t slot,
                                        const uint8_t *data, uint32_t bytes, uint32_t journal_slot,
                                        const uint8_t *journal, uint32_t journal_bytes,
                                        enum hpclang_rpl_flash_authority_phase *phase);

enum hpclang_rpl_flash_sector_phase
{
    HPCLANG_RPL_FLASH_SECTOR_READY = 1,
    HPCLANG_RPL_FLASH_SECTOR_RETIRING = 2,
    HPCLANG_RPL_FLASH_SECTOR_RETIRED = 3
};

struct hpclang_rpl_flash_sector_descriptor
{
    uint32_t target, crc;
    enum hpclang_rpl_flash_sector_phase phase;
};

/* Validate a sector capsule without retaining its 4 KiB image. The reader uses
 * bounded local nibble coordinates; slot/target remain physical byte addresses.
 * Read and CRC-check every byte, retaining only the 24-byte header. Output
 * changes only on success. Keep this same Flash view stable for subsequent
 * image reads, membership checks and authority inspection. This descriptor
 * does not establish freshness, ownership or permission to replay anything. */
int hpclang_rpl_flash_sector_inspect_reader(uint32_t first, uint32_t end, uint32_t slot,
                                            const struct hpclang_rpl_reader *reader,
                                            uint32_t address,
                                            struct hpclang_rpl_flash_sector_descriptor *descriptor);

/* Use only a descriptor from a successful inspect_reader (or equivalently
 * validated capsule) from the SAME stable view. Validates authority bytes and
 * binding; does not validate allocation membership or grant replay permission. */
int hpclang_rpl_flash_authority_inspect_descriptor(
    uint32_t first, uint32_t end, uint32_t slot, const uint8_t *data, uint32_t bytes,
    uint32_t journal_slot, const struct hpclang_rpl_flash_sector_descriptor *journal,
    enum hpclang_rpl_flash_authority_phase *phase);

/* Validate a complete sector capsule and classify 7E-to-00 retirement cuts.
 * Unpublished/partially published statuses are rejected. Outputs change only
 * on success. RETIRING/RETIRED never authorize replay; recovery must verify the
 * existing target before completing retirement. This is not freshness proof. */
int hpclang_rpl_flash_sector_inspect(uint32_t first, uint32_t end, uint32_t slot,
                                     const uint8_t *data, uint32_t bytes, uint32_t *target,
                                     const uint8_t **image,
                                     enum hpclang_rpl_flash_sector_phase *phase);

/* Internal redo-image allocation (metadata 3, HPCSECT1), recognized by the
 * read-only sector preflight but not authorized by public write preparation.
 * It is not yet used by public mutations. It holds a
 * complete 4 KiB desired sector image, its target/self addresses and a status-independent
 * CRC32. The journal allocation must be outside the target sector and native
 * bank headers, with an erased end-marker position remaining in its bank.
 * Callers separately prove real allocation boundaries and erased capacity;
 * the codec checks geometry only.
 * Encode/decode are memory-only; neither authorizes erase, establishes that
 * the image is a valid native bank, or provides startup recovery. Successful
 * decode returns the target and an image view into the caller's stable buffer.
 * Outputs are unchanged on failure. Source/output overlap is supported. */
int hpclang_rpl_flash_sector_encode(uint32_t first, uint32_t end, uint32_t slot, uint32_t target,
                                    const uint8_t *image, uint8_t *data, uint32_t capacity);
int hpclang_rpl_flash_sector_decode(uint32_t first, uint32_t end, uint32_t slot,
                                    const uint8_t *data, uint32_t bytes, uint32_t *target,
                                    const uint8_t **image);

/* CRC32/ISO-HDLC of allocation bytes after the status byte. Status publication
 * or deletion does not change record identity. This supplements, rather than
 * replaces, object CRC/shape validation. Reader coordinates are nibbles. */
int hpclang_rpl_flash_record_crc(const struct hpclang_rpl_reader *reader, uint32_t address,
                                 uint32_t bytes, uint32_t *crc);

/* Encode the allocation envelope around an exact, CRC-validated backup.
 * Coordinates in the input reader are nibbles; the owned output is bytes.
 * Matches native HP49/50 STO records: status 53, relative extent, metadata 1,
 * packed object, then erased padding to a whole number of four-byte words.
 * The unused nibble of an odd-sized object is also erased padding; Stock may
 * copy a different value there, outside the object's CRC-protected extent.
 * This only builds bytes; it neither reserves space nor commits a record.
 * The source stays stable throughout validation/copy. Outputs are unchanged
 * on failure. Free *record after use. */
int hpclang_rpl_flash_backup_record(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t span, uint8_t **record, uint32_t *bytes);

/* Same allocation envelope for an exact CRC-valid library. Does not reserve
 * Flash space, register a library or make the public library write available. */
int hpclang_rpl_flash_library_record(const struct hpclang_rpl_reader *reader, uint32_t address,
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
