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

/* Internal redo-image allocation (metadata 3, HPCSECT1), not yet
 * used by public mutations or recognized by whole-port recovery. It holds a
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
