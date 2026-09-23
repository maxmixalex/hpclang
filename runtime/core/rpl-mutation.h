#ifndef HPCLANG_RUNTIME_RPL_MUTATION_H
#define HPCLANG_RUNTIME_RPL_MUTATION_H

#include <stdint.h>

#include "object-snapshot.h"
#include "rpl-object-span.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*hpclang_rpl_write_nibble_fn)(
    void *context,
    uint32_t nibble_address,
    uint8_t value);

struct hpclang_rpl_mutation_io {
    hpclang_rpl_read_nibble_fn read_nibble;
    hpclang_rpl_write_nibble_fn write_nibble;
    void *context;

    /*
     * Static fences computed by the launcher from hpclang-owned native pages.
     * TEMPOB/RSKTOP may grow only below temp_limit. DSKTOP may grow downward
     * only above user_limit.
     */
    uint32_t temp_limit;
    uint32_t user_limit;
    uint32_t stack_reserve_nibbles;
};

enum hpclang_rpl_mutation_result {
    HPCLANG_RPL_MUTATION_OK = 0,
    HPCLANG_RPL_MUTATION_UNAVAILABLE = -1,
    HPCLANG_RPL_MUTATION_MALFORMED = -2,
    HPCLANG_RPL_MUTATION_NOSPACE = -3,
    HPCLANG_RPL_MUTATION_LEVEL = -4,
    HPCLANG_RPL_MUTATION_IO = -5,
    HPCLANG_RPL_MUTATION_BUSY = -6,
    HPCLANG_RPL_MUTATION_NOT_FOUND = -7,
    HPCLANG_RPL_MUTATION_DIRECTORY = -8,
    HPCLANG_RPL_MUTATION_UNSUPPORTED = -9
};

/*
 * Copy an owned ARM-side snapshot into a new Stock TEMPOB block and push its
 * pointer onto level 1. All expected logical failures are checked before the
 * first mutation. The snapshot is the source of truth across allocation; no
 * borrowed Saturn object address survives the allocation step.
 */
int hpclang_rpl_mutation_push_snapshot(
    const struct hpclang_rpl_mutation_io *io,
    const struct hpclang_object *object);

/* Drop count objects after validating the complete request. */
int hpclang_rpl_mutation_drop(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t count);

/*
 * Allocate/copy first, then replace one existing pointer cell. A failure before
 * pointer commit leaves the visible stack unchanged; an unreferenced TEMPOB
 * block may remain for Stock garbage collection after an unexpected I/O fault.
 */
int hpclang_rpl_mutation_replace(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t level,
    const struct hpclang_object *object);

/*
 * Replace an existing non-directory USEROB object from an owned snapshot.
 * Same-size writes happen in place. Size-changing writes use the bounded
 * USEROB shift primitive and currently require the old object to be
 * unreferenced; referenced-value preservation through TEMPOB is the next
 * M7.3f layer.
 */
int hpclang_rpl_mutation_store_replace(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t directory,
    const uint8_t *name,
    uint32_t name_length,
    const struct hpclang_object *object);

/*
 * Strict compatibility helper for the first live M7.3f witness. It rejects a
 * different native extent even though the general replacement path can now
 * resize an unreferenced object.
 */
int hpclang_rpl_mutation_store_replace_same_size(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t directory,
    const uint8_t *name,
    uint32_t name_length,
    const struct hpclang_object *object);

/*
 * Internal USEROB geometry primitive for M7.3f.
 *
 * delta > 0 opens delta nibbles immediately before position by moving the
 * lower USEROB prefix toward lower addresses. delta < 0 removes -delta
 * nibbles immediately before position and closes the gap upward. All known
 * Stock pointer tables and classic-RPL directory relative links are validated
 * before mutation and relocated coherently. A contraction is rejected with
 * BUSY if any Stock-visible pointer still targets the removed interval.
 *
 * This does not create, encode, replace, or remove a named variable by itself;
 * higher-level store code owns the bytes placed into/opened from the gap.
 */
int hpclang_rpl_mutation_userob_shift_before(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t position,
    int32_t delta);

#ifdef __cplusplus
}
#endif

#endif
