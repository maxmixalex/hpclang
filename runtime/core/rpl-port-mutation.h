#ifndef HPCLANG_RUNTIME_RPL_PORT_MUTATION_H
#define HPCLANG_RUNTIME_RPL_PORT_MUTATION_H

#include "rpl-mutation.h"

/* Append to the ERAM chain without moving any existing record. limit is the
 * launcher-proven exclusive nibble boundary before its physical page pool. */
int hpclang_rpl_port1_create_backup(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                    const uint8_t *packed, uint32_t span);
/* Same ownership/append contract for a library. Does not register or attach it;
 * duplicate library IDs are rejected independently of their title. */
int hpclang_rpl_port1_create_library(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                     const uint8_t *packed, uint32_t span);

/* Read-only reference preflight. The callback must prove no live banked
 * reference targets [start,end), expressed as ERAM-relative nibble offsets.
 * Return a mutation result (OK, BUSY or a read/validation error). The inspected
 * native state and mappings must remain stable through the subsequent commit. */
struct hpclang_rpl_port1_guard
{
    int (*check)(void *context, uint32_t start, uint32_t end);
    void *context;
};

/* Internal ERAM edits require the reference guard; absence is UNAVAILABLE.
 * These routines do not relocate banked references. Libraries after the target
 * cannot move. The launcher ownership fence and append commit contract apply.
 * Input/guard/preflight failures do not write. Once the guarded commit begins,
 * a backend read/write failure is INCOMPLETE, not a preflight IO failure. Retain
 * ownership and do not resume Stock or retry: earlier writes may have changed
 * the chain and the reference guard does not provide rollback. */
int hpclang_rpl_port1_replace_backup(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                     const uint8_t *packed, uint32_t span,
                                     const struct hpclang_rpl_port1_guard *guard);
int hpclang_rpl_port1_remove_backup(const struct hpclang_rpl_mutation_io *io, uint32_t limit,
                                    const uint8_t *name, uint32_t name_length,
                                    const struct hpclang_rpl_port1_guard *guard);

/* Store a library record without registering or attaching it to the OS.
 * Existing record addresses remain stable; duplicate library IDs are rejected.
 * The caller owns stable packed input, as for backup creation below. */
int hpclang_rpl_port0_create_library(const struct hpclang_rpl_mutation_io *io,
                                     const uint8_t *packed, uint32_t span);

/* Prepend one validated packed backup without moving existing port objects.
 * Duplicate names are rejected. The input is owned ARM memory and must remain
 * stable. Uses the same preflight/commit I/O contract as USEROB mutation. */
int hpclang_rpl_port0_create_backup(const struct hpclang_rpl_mutation_io *io, const uint8_t *packed,
                                    uint32_t span);

/* Replacement preserves references to the complete old backup in TEMP.
 * Size changes preceding any library use the backup-prefix movement path;
 * moving a preceding library still needs the separate library-aware path. */
int hpclang_rpl_port0_replace_backup(const struct hpclang_rpl_mutation_io *io,
                                     const uint8_t *packed, uint32_t span);

/* Internal backup-chain geometry. Validate the complete chain and whole-record
 * boundaries before moving a prefix. Libraries must remain at their original
 * addresses; a caller needing to move one requires the library-aware path.
 * Contraction returns BUSY for live references into removed records. */
int hpclang_rpl_port0_shift_backups(const struct hpclang_rpl_mutation_io *io, uint32_t position,
                                    int32_t delta);

/* Preserve the removed backup and enclosed values for live references.
 * Preceding libraries still require the separate library relocation path. */
int hpclang_rpl_port0_remove_backup(const struct hpclang_rpl_mutation_io *io, const uint8_t *name,
                                    uint32_t name_length);

struct hpclang_rpl_catalog_insertion;
struct hpclang_rpl_library_binding;
/* Internal catalog commit, not public registration authorization. Revalidate
 * the strictly ordered catalog and the expected insertion plan before writes.
 * binding uses FINAL post-move coordinates, independently proven by the caller;
 * its payload, bank selector, flags and execution safety are not inferred here.
 * Caller supplies complete reference repairs and retains mapping/IRQ ownership
 * through growth, record publication and count publication (last). Input objects
 * and backend remain stable throughout. No callbacks/configuration are executed.
 * Preflight errors do not write. Any failure after growth starts is INCOMPLETE:
 * retain ownership, do not resume Stock or retry this non-replayable operation. */
int hpclang_rpl_catalog_insert_repaired(const struct hpclang_rpl_mutation_io *io, uint32_t number,
                                        const struct hpclang_rpl_library_binding *binding,
                                        const struct hpclang_rpl_catalog_insertion *expected,
                                        const struct hpclang_rpl_catalog_repairs *repairs);

#endif
