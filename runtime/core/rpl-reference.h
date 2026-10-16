#ifndef HPCLANG_RPL_REFERENCE_H
#define HPCLANG_RPL_REFERENCE_H

#include "rpl-object-span.h"

enum hpclang_rpl_entry_kind
{
    HPCLANG_RPL_ENTRY_OBJECT,
    HPCLANG_RPL_ENTRY_SELF_DISPATCH
};

struct hpclang_rpl_entry
{
    enum hpclang_rpl_entry_kind kind;
    uint32_t dispatch;
    uint32_t span; /* Exact object span, or zero for an unbounded execution body. */
};

/* Inspect a caller-proven runstream entry in a stable mapping. Known object
 * prologues take precedence and require their ordinary bounded validation.
 * Otherwise recognize only a dispatch word equal to address+5, with its first
 * body nibble readable inside limit. This does not decode instructions, infer
 * code length, follow other dispatch words or certify absence of references.
 * Unknown dispatches remain UNSUPPORTED. Never use this to scan arbitrary data
 * for executable objects or to bypass object_span's failure contract.
 * Returns a span result; output changes only on success. */
int hpclang_rpl_entry_inspect(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t limit, struct hpclang_rpl_entry *entry);

enum hpclang_rpl_reference_kind
{
    HPCLANG_RPL_REFERENCE_INLINE,
    HPCLANG_RPL_REFERENCE_COMPACT,
    HPCLANG_RPL_REFERENCE_DIRECT,
    HPCLANG_RPL_REFERENCE_ACPTR,
    HPCLANG_RPL_REFERENCE_ROMP,
    HPCLANG_RPL_REFERENCE_FLASHP,
    HPCLANG_RPL_REFERENCE_OPAQUE,
    HPCLANG_RPL_REFERENCE_ATTACHMENT,
    HPCLANG_RPL_REFERENCE_EXECUTION,
    HPCLANG_RPL_REFERENCE_EMBEDDED_ENTRY
};

struct hpclang_rpl_reference
{
    enum hpclang_rpl_reference_kind kind;
    uint32_t field;
    uint32_t target;
    uint32_t extent;
    uint32_t auxiliary;
};

/* Immediate structural edges, not a reachability or mutation-safety verdict.
 * INLINE: target/extent are a complete child object.
 * COMPACT: target/extent omit its prologue, supplied in auxiliary.
 * DIRECT: target is a native address; extent is zero for a runstream reference,
 * or the available bound from a library table. Library edges use auxiliary =
 * library_reference_kind + 1; runstream edges use zero. This is not a type claim.
 * ACPTR: target is the stored Saturn address; auxiliary is its bank switch.
 * ROMP: target/auxiliary are library/command indices.
 * FLASHP: target/auxiliary are logical selector/index (not physical banks).
 * ATTACHMENT: target is a HOME attached-library pointer, auxiliary its library
 * ID. extent is zero; bank qualification happens after reading the target.
 * EXECUTION: field stores the dispatch address in target. Its machine-code
 * extent and reference semantics remain unresolved; do not treat it as a
 * normal object or a reference-free leaf. extent and auxiliary are zero.
 * EMBEDDED_ENTRY: field == target is the start of an inline execution entry,
 * not a stored address. Its extent is unknown (zero); inspect it separately.
 * OPAQUE: target/extent bound an unexpanded object; auxiliary is its prologue.
 * Opaque code, binary data and unrecognized child-directory header metadata require
 * their own semantic traversal;
 * a caller must never treat an OPAQUE event as absence of references.
 * All addresses retain the input reader's mapping. No target is dereferenced.
 * The callback is read-only; keep mappings stable and discard partial results
 * on any nonzero return. Callback errors propagate unchanged. */
typedef int (*hpclang_rpl_reference_visit_fn)(void *context,
                                              const struct hpclang_rpl_reference *reference);

/* body_prologue == 0 reads a normal object's prologue from address. Otherwise
 * address/limit describe a compact array body whose declared prologue is given
 * separately. A temporary reader supplies that prefix without writing memory.
 * Returns a span result or callback error; does not allocate or recurse into
 * reported targets. Graph scheduling, visited sets and budgets belong to caller. */
int hpclang_rpl_reference_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                                uint32_t limit, uint32_t body_prologue,
                                hpclang_rpl_reference_visit_fn visit, void *context);

enum hpclang_rpl_dispatch_schema
{
    HPCLANG_RPL_DISPATCH_DUPLICATE_THEN_ENTRY,
    HPCLANG_RPL_DISPATCH_TYPE_PREDICATE
};

/* Caller-proven dispatch identity in the reader's stable mapping. Establish
 * the handler's semantics separately before supplying this descriptor; this
 * function neither discovers nor authenticates ROM code. No model addresses
 * are built in. Known object prologues cannot be overridden by a descriptor.
 * DUPLICATE_THEN_ENTRY exposes an execution entry at address+5, not a stored
 * pointer field. TYPE_PREDICATE consumes a five-nibble scalar type identifier,
 * which must never be emitted as an object reference. Both emit EXECUTION for
 * the header, retaining the unresolved machine-code obligation. This is an
 * immediate-edge decoder, not a complete extent or mutation-safety proof.
 * Callback/read errors propagate; discard partial output on any error. */
struct hpclang_rpl_dispatch_descriptor
{
    uint32_t dispatch;
    enum hpclang_rpl_dispatch_schema schema;
};

int hpclang_rpl_dispatch_reference_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                                         uint32_t limit,
                                         const struct hpclang_rpl_dispatch_descriptor *descriptor,
                                         hpclang_rpl_reference_visit_fn visit, void *context);

struct hpclang_rpl_reference_patch
{
    uint32_t field; /* Old address: apply before moving the containing bytes. */
    uint32_t value; /* Replacement five-nibble word. */
};

/* Plan one edge previously validated by reference_visit, for relocation of
 * [first,end) by delta within the same Saturn mapping. Return 1 with a patch,
 * 0 when its encoding does not change, or a negative span result. Output only
 * changes on 1. Relative library fields account for movement of both ends.
 * INLINE/COMPACT/EMBEDDED_ENTRY need recursive traversal; ROMP/FLASHP retain their indices.
 * Banked ACPTR, execution dispatches and opaque contents return UNSUPPORTED and require separate
 * resolution. This is neither a graph-completeness check nor a mutation API. */
int hpclang_rpl_reference_plan_shift(const struct hpclang_rpl_reference *reference, uint32_t first,
                                     uint32_t end, int32_t delta,
                                     struct hpclang_rpl_reference_patch *patch);

/* Identities of resolved physical mappings, assigned by the caller. The field
 * and target must each be qualified separately; identical Saturn addresses in
 * different banks do not alias. These are not raw bank-switch entry addresses.
 * Resolve ACPTR switches and catalog bindings before supplying these identities.
 * Keep mappings stable throughout planning and commit. */
struct hpclang_rpl_reference_mappings
{
    uint32_t field, target, moving;
};

/* As plan_shift, with independently qualified source/target locations. Allows
 * an ACPTR only because the caller has resolved its target mapping above.
 * patch.field remains the old address in mappings.field; retain that mapping
 * with the patch. No bank switch, native bytes or index tables are modified.
 * This does not discover roots or prove graph completeness/erase safety. */
int hpclang_rpl_reference_plan_qualified_shift(
    const struct hpclang_rpl_reference *reference,
    const struct hpclang_rpl_reference_mappings *mappings, uint32_t first, uint32_t end,
    int32_t delta, struct hpclang_rpl_reference_patch *patch);

struct hpclang_rpl_reference_edit
{
    uint32_t mapping, field, expected, value;
};

struct hpclang_rpl_reference_reader
{
    int (*read_nibble)(void *context, uint32_t mapping, uint32_t address, uint8_t *value);
    void *context;
};

/* Read-only dependencies of a relocation plan, including short native count,
 * ID and flag fields. Width is 1..5 nibbles. Overlap is allowed: guards do not
 * write. Validate alongside edits while retaining exclusive ownership through
 * commit; neither validation supplies synchronization or discovers omissions. */
struct hpclang_rpl_reference_guard
{
    uint32_t mapping, field, expected, width;
};

struct hpclang_rpl_reference_guard_builder
{
    struct hpclang_rpl_reference_guard *guards;
    uint32_t capacity, count;
    int error;
};

/* Bounded accumulation of observed nibbles into sorted, nonoverlapping guards.
 * Merge adjacent observations up to five nibbles; never include an unobserved
 * cell. Repeated identical observations use no extra space. Conflicting values,
 * invalid input or insufficient capacity latch an error and invalidate count.
 * Start a new stream with begin after any failure. Only successful complete
 * traversal permits using the array; this does not prove traversal completeness.
 * Caller owns builder/storage exclusively and normalizes physical mapping IDs.
 * No allocation, native reads or native writes are performed here. */
int hpclang_rpl_reference_guards_begin(struct hpclang_rpl_reference_guard_builder *builder,
                                       struct hpclang_rpl_reference_guard *guards,
                                       uint32_t capacity);
int hpclang_rpl_reference_guards_observe(struct hpclang_rpl_reference_guard_builder *builder,
                                         uint32_t mapping, uint32_t address, uint8_t value);

struct hpclang_rpl_library_binding;
/* Return zero with a normalized physical mapping identity, or an error.
 * Resolve every binding, including those without a bank switch. Read-only;
 * keep the resolution stable through validation/commit. */
typedef int (*hpclang_rpl_catalog_mapping_fn)(void *context,
                                              const struct hpclang_rpl_library_binding *binding,
                                              uint32_t *mapping);

struct hpclang_rpl_catalog_move
{
    uint32_t catalog_mapping, moving_mapping, first, end;
    int32_t delta;
};

struct hpclang_rpl_catalog_reference_plan
{
    struct hpclang_rpl_reference_edit *edits;
    uint32_t edit_capacity, edit_count;
    struct hpclang_rpl_reference_guard *guards;
    uint32_t guard_capacity, guard_count;
};

/* Assemble the catalog portion of a relocation plan without writing native
 * data. For N records, supply 2*N edits (address and unchanged bank switch)
 * and 1+2*N guards (count, IDs and flags). Arrays must be disjoint from each
 * other and all input/IO state. On success counts describe the complete plan;
 * on error both counts are zero and all partially filled entries are invalid.
 * Duplicate IDs are preserved as distinct records. Fields retain old addresses.
 * Keep mappings/catalog/buffers exclusively owned throughout later validation
 * and commit. This does not discover other roots, repair references or authorize
 * movement/Stock return. Zero is success; other results are span/resolver errors. */
int hpclang_rpl_catalog_plan_relocation(const struct hpclang_rpl_reader *reader, uint32_t address,
                                        uint32_t limit, const struct hpclang_rpl_catalog_move *move,
                                        hpclang_rpl_catalog_mapping_fn resolve, void *context,
                                        struct hpclang_rpl_catalog_reference_plan *plan);

int hpclang_rpl_reference_guards_validate(const struct hpclang_rpl_reference_reader *reader,
                                          const struct hpclang_rpl_reference_guard *guards,
                                          uint32_t count);

/* Read-only preflight of a complete caller-owned edit array. Mapping IDs must
 * normalize physical aliases to the same identity. All fields/words and
 * same-mapping overlaps (including duplicate fields) are checked before I/O;
 * then every five-nibble old value must equal expected. Returns a span result:
 * malformed plans or changed old values are MALFORMED, read errors UNAVAILABLE.
 * Empty arrays succeed without a reader. No write callback is accepted.
 * Caller must retain exclusive ownership and the unchanged array/mappings
 * through any later commit. This verifies neither missing roots nor write
 * failure recovery; success alone does not authorize native relocation. */
int hpclang_rpl_reference_edits_validate(const struct hpclang_rpl_reference_reader *reader,
                                         const struct hpclang_rpl_reference_edit *edits,
                                         uint32_t count);

struct hpclang_rpl_reference_writer
{
    struct hpclang_rpl_reference_reader reader;
    /* Atomic RAM nibble replacement, preserving neighboring data. On failure
     * the nibble is either unchanged or fully replaced. Never a NOR writer. */
    int (*write_nibble)(void *context, uint32_t mapping, uint32_t address, uint8_t value);
};

/* Finish a previously authorized, caller-owned RAM reference edit batch.
 * Validate all metadata/overlaps before IO, then require every nibble to equal
 * its expected or replacement value before any write. This admits interrupted
 * RAM updates, not unknown values. Write only differing nibbles and verify each.
 * Caller has already established complete roots, immutable plan/guards, resolved
 * physical aliases and exclusive ownership through commit/retry. Flash fields
 * require staged sector images instead. Every error retains the ownership and
 * recovery obligation: earlier writes or an earlier attempt may have happened.
 * This does not persist the plan, infer freshness, supply synchronization or
 * authorize relocation/Stock return. Zero-count batches succeed without IO. */
int hpclang_rpl_reference_edits_replay(const struct hpclang_rpl_reference_writer *writer,
                                       const struct hpclang_rpl_reference_edit *edits,
                                       uint32_t count);

#endif
