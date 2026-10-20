#ifndef HPCLANG_RUNTIME_RPL_MUTATION_H
#define HPCLANG_RUNTIME_RPL_MUTATION_H

#include <stdint.h>

#include "object-snapshot.h"
#include "rpl-object-span.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef int (*hpclang_rpl_write_nibble_fn)(void *context, uint32_t nibble_address,
                                               uint8_t value);

    struct hpclang_rpl_mutation_io
    {
        /* Production uses an invocation-local writable main-RAM view while
         * interrupts
         * remain masked. Callback backends must keep mappings and
         * contents stable except
         * for these writes; preflight is not rollback
         * for a backend that starts failing
         * after the commit has begun. */
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
        /* Repeated ARM-side cell addresses are visited/relocated once.
         * Distinct cells holding equal values remain independent roots. */
        uint32_t **native_roots;
        uint32_t native_root_count;
        /* Optional classic ROM-part table: 3-nibble count, 16-nibble records.
         * Zero disables this model-specific source of native references. */
        uint32_t library_table;
        /* Model-validated nonoverlapping five-nibble display cells below SYSUPD.
         * Addresses, not pointer values; optional for synthetic backends. */
        const uint32_t *display_roots;
        uint32_t display_root_count;
    };

    enum hpclang_rpl_mutation_result
    {
        HPCLANG_RPL_MUTATION_OK = 0,
        HPCLANG_RPL_MUTATION_UNAVAILABLE = -1,
        HPCLANG_RPL_MUTATION_MALFORMED = -2,
        HPCLANG_RPL_MUTATION_NOSPACE = -3,
        HPCLANG_RPL_MUTATION_LEVEL = -4,
        HPCLANG_RPL_MUTATION_IO = -5,
        HPCLANG_RPL_MUTATION_BUSY = -6,
        HPCLANG_RPL_MUTATION_NOT_FOUND = -7,
        HPCLANG_RPL_MUTATION_DIRECTORY = -8,
        HPCLANG_RPL_MUTATION_UNSUPPORTED = -9,
        HPCLANG_RPL_MUTATION_EXISTS = -10,
        HPCLANG_RPL_MUTATION_NOT_DIRECTORY = -11,
        HPCLANG_RPL_MUTATION_NOT_EMPTY = -12,
        HPCLANG_RPL_MUTATION_NOMEM = -13,
        /* A guarded commit started and did not finish. Native/ARM state may be
         * partially changed; retain ownership, never resume Stock or retry. */
        HPCLANG_RPL_MUTATION_INCOMPLETE = -14
    };

    /* Read-only enumeration of the pointer cells already covered by native
     * relocation: launcher/CPU roots, stacks and supported Stock pointer tables.
     * slot is zero for an ARM-side launcher root. Values may be sentinels,
     * execution positions or metadata, not necessarily object beginnings.
     * HOME attachment fields and model-validated display cells are included.
     * Banked catalog entries and other object-body references need separate
     * traversal. Keep mappings stable and discard partial results on error. */
    typedef int (*hpclang_rpl_root_visit_fn)(void *context, uint32_t slot, uint32_t value);
    int hpclang_rpl_mutation_visit_roots(const struct hpclang_rpl_mutation_io *io,
                                         hpclang_rpl_root_visit_fn visit, void *context);

    enum hpclang_rpl_root_source
    {
        HPCLANG_RPL_ROOT_NATIVE,
        HPCLANG_RPL_ROOT_DATA_STACK,
        HPCLANG_RPL_ROOT_RETURN_STACK,
        HPCLANG_RPL_ROOT_TEMPENV,
        HPCLANG_RPL_ROOT_TOUCHTAB,
        HPCLANG_RPL_ROOT_VGER,
        HPCLANG_RPL_ROOT_SYSTEM,
        HPCLANG_RPL_ROOT_LIBRARY,
        HPCLANG_RPL_ROOT_DISPLAY,
        HPCLANG_RPL_ROOT_ATTACHMENT,
        HPCLANG_RPL_ROOT_LIBRARY_SWITCH
    };

    /* The same cells as visit_roots, with their source preserved. Source is
     * provenance, not a type assertion: system/support tables may mix metadata
     * and references, native roots may include saved execution positions, and
     * return-stack values need not point at object prologues. Library cells
     * tagged LIBRARY are main-RAM bindings only; enumerate banked payloads
     * separately. LIBRARY_SWITCH is the nonzero executable switch pointer in
     * the current mapping, before that function selects the payload bank.
     * No source is automatically safe to skip. Same read-only/error contract. */
    typedef int (*hpclang_rpl_root_source_visit_fn)(void *context,
                                                    enum hpclang_rpl_root_source source,
                                                    uint32_t slot, uint32_t value);
    int hpclang_rpl_mutation_visit_root_sources(const struct hpclang_rpl_mutation_io *io,
                                                hpclang_rpl_root_source_visit_fn visit,
                                                void *context);

    /* Report every valid native nibble read while enumerating the same roots,
     * including table heads, lengths and traversal metadata. Repeated reads may
     * repeat dependencies; the caller may coalesce them into bounded guards.
     * Both callbacks are read-only and return zero to continue. Any error
     * invalidates all partial roots/dependencies. No native write callback is
     * forwarded. Keep mappings, IO configuration and ARM-side native_roots
     * unchanged: those host values are not native nibble dependencies. Slot-zero
     * roots still appear through visit and need caller-owned lifetime/guards.
     * This supplies scan dependencies, not complete object-body references or
     * permission to relocate data. */
    typedef int (*hpclang_rpl_root_dependency_fn)(void *context, uint32_t address, uint8_t value);
    int hpclang_rpl_mutation_visit_guarded_roots(const struct hpclang_rpl_mutation_io *io,
                                                 hpclang_rpl_root_source_visit_fn visit,
                                                 hpclang_rpl_root_dependency_fn dependency,
                                                 void *context);

    /* Enumerate TEMP allocation payloads, newest first, after validating the
     * complete footer chain. Payloads can contain private execution data beyond
     * the first typed object's span; these are allocation bounds, not proof of
     * a single object. The callback is read-only and its nonzero result stops
     * traversal. Keep the native memory mapping stable throughout the call. */
    typedef int (*hpclang_rpl_temp_visit_fn)(void *context, uint32_t first, uint32_t nibbles);
    int hpclang_rpl_mutation_visit_temp(const struct hpclang_rpl_mutation_io *io,
                                        hpclang_rpl_temp_visit_fn visit, void *context);

    /* Bootstrap-only: turn the native page interval into a rooted opaque
     * TEMPOB block. Pin
     * the compact pre-existing TEMP prefix as well, so Stock
     * GC cannot move the block while
     * ARM executes from it. Extra roots are
     * pushed first, immediately below the incoming
     * data stack. */
    int hpclang_rpl_mutation_pin_arena(const struct hpclang_rpl_mutation_io *io, uint32_t first,
                                       uint32_t end, const uint32_t *extra_roots,
                                       uint32_t extra_count, uint32_t *pinned_cells);

    /*
     * Copy an owned ARM-side snapshot into a new Stock TEMPOB block and push its
     * pointer onto level 1. All expected logical failures are checked before the
     * first mutation. The snapshot is the source of truth across allocation; no
     * borrowed Saturn object address survives the allocation step.
     */
    int hpclang_rpl_mutation_push_snapshot(const struct hpclang_rpl_mutation_io *io,
                                           const struct hpclang_object *object);

    /* Drop count objects after validating the complete request. */
    int hpclang_rpl_mutation_drop(const struct hpclang_rpl_mutation_io *io, uint32_t count);

    /* Validate both levels and accounting before changing any pointer cell. */
    int hpclang_rpl_mutation_reorder(const struct hpclang_rpl_mutation_io *io, int nip);

    /* Push the immutable supported-entry Boolean reference without allocating. */
    int hpclang_rpl_mutation_push_boolean(const struct hpclang_rpl_mutation_io *io, int value);

    /* Return real 0/1 to a UserRPL condition without allocating a TEMPOB object.
     * Caller validates the HP49g+/HP50g Stock ROM real constants and owns the
     * captured RAM mapping throughout. Does not change the public Boolean ABI. */
    int hpclang_rpl_mutation_push_real_boolean(const struct hpclang_rpl_mutation_io *io, int value);

    /*
     * Allocate/copy first, then replace one existing pointer cell. A failure before
     * pointer commit leaves the visible stack unchanged; an unreferenced TEMPOB
     * block may remain for Stock garbage collection after an unexpected I/O fault.
     */
    int hpclang_rpl_mutation_replace(const struct hpclang_rpl_mutation_io *io, uint32_t level,
                                     const struct hpclang_object *object);

    /* Replace one exact-size native value, preserving old live references in
     * TEMPOB before overwrite. The caller validates namespace ownership and
     * maintains its enclosing metadata/checksum. Structural directory/library
     * objects must not be passed as ordinary values. */
    int hpclang_rpl_mutation_overwrite_value(const struct hpclang_rpl_mutation_io *io,
                                             uint32_t address, const struct hpclang_object *object);

    /*
     * Replace an existing non-directory USEROB object from an owned snapshot.
     * Same-size and size-changing writes both preserve live Stock references to
     * the old value through a bounded TEMPOB copy before the named object is
     * overwritten or resized.
     */
    int hpclang_rpl_mutation_store_replace(const struct hpclang_rpl_mutation_io *io,
                                           uint32_t directory, const uint8_t *name,
                                           uint32_t name_length,
                                           const struct hpclang_object *object);

    /*
     * Create one new classic-RPL directory entry. The complete new entry is
     * written into an unreachable USEROB gap first; the directory chain pointer is
     * the final visibility commit. Existing names return EXISTS.
     */
    int hpclang_rpl_mutation_store_create(const struct hpclang_rpl_mutation_io *io,
                                          uint32_t directory, const uint8_t *name,
                                          uint32_t name_length,
                                          const struct hpclang_object *object);

    /*
     * Remove one existing non-directory classic-RPL entry. Live references to the
     * object value are preserved through TEMPOB; references into the entry's
     * structural name/link prefix are rejected.
     */
    int hpclang_rpl_mutation_store_remove(const struct hpclang_rpl_mutation_io *io,
                                          uint32_t directory, const uint8_t *name,
                                          uint32_t name_length);

    /* Explicit directory removal, with cwd/ancestor rejection before writes. */
    int hpclang_rpl_mutation_store_rmdir(const struct hpclang_rpl_mutation_io *io,
                                         uint32_t directory, const uint8_t *name,
                                         uint32_t name_length, int recursive);

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
    /* Port-only geometry backend. Caller must validate the complete port chain,
     * record boundaries and library relocation safety before invoking this.
     * Live references into a contraction still return BUSY. */
    int hpclang_rpl_mutation_port0_shift_before(const struct hpclang_rpl_mutation_io *io,
                                                uint32_t position, int32_t delta);

    /* Same caller validation as above; preserve an entire removed record when
     * Stock retains references to it or any of its enclosed values. */
    int hpclang_rpl_mutation_port0_remove_record(const struct hpclang_rpl_mutation_io *io,
                                                 uint32_t address, uint32_t span);

    /* Caller validates both encodings and library movement. Packed bytes must
     * be owned ARM storage and remain stable throughout the transaction. */
    int hpclang_rpl_mutation_port0_replace_record(const struct hpclang_rpl_mutation_io *io,
                                                  uint32_t address, uint32_t old_span,
                                                  const uint8_t *packed, uint32_t new_span);

    int hpclang_rpl_mutation_userob_shift_before(const struct hpclang_rpl_mutation_io *io,
                                                 uint32_t position, int32_t delta);

    /* Internal catalog geometry: open one 16-nibble record at position, moving
     * the catalog suffix, metadata, TEMP and RPL return stack upward. Caller
     * must validate the catalog and prove relocation safety of all object
     * contents, including opaque code/binary data and banked references. This
     * only relocates the root sources supplied by io; it is not registration.
     * On success the caller must fill the gap and publish the new catalog count
     * before restoring Stock execution. Mapping/IRQ ownership spans both steps.
     * Preflight failures make no writes; stable backend reads/writes are required
     * during commit, as for the other mutation primitives. */
    int hpclang_rpl_mutation_catalog_grow(const struct hpclang_rpl_mutation_io *io,
                                          uint32_t position);

    struct hpclang_rpl_reference_edit;
    struct hpclang_rpl_reference_guard;
    struct hpclang_rpl_catalog_repairs
    {
        uint32_t main_mapping;
        const struct hpclang_rpl_reference_edit *edits;
        uint32_t edit_count;
        const struct hpclang_rpl_reference_guard *guards;
        uint32_t guard_count;
    };

    /* Same geometry/ownership contract as catalog_grow. Merge caller-proven
     * object/code repairs with an immutable snapshot of native root repairs.
     * Exact duplicates coalesce; conflicting/partial overlaps reject before
     * writes, including conflicts with unchanged roots. Fields are ORIGINAL
     * pre-move addresses, wholly in main RAM; normalized mappings must equal
     * main_mapping. Other physical banks require separate transaction work.
     * Guards describe unchanged preconditions, not permissions to relocate.
     * Keep input arrays, ARM roots, mappings and memory stable throughout.
     * Allocates temporary ARM heap storage; allocation/preflight failure makes
     * no writes. INCOMPLETE distinguishes a commit failure from those preflight
     * errors: retain ownership, do not return to Stock or retry this non-replayable
     * geometry operation. Complete
     * reference coverage remains the caller's obligation; not public registration. */
    int
    hpclang_rpl_mutation_catalog_grow_repaired(const struct hpclang_rpl_mutation_io *io,
                                               uint32_t position,
                                               const struct hpclang_rpl_catalog_repairs *repairs);

#ifdef __cplusplus
}
#endif

#endif
