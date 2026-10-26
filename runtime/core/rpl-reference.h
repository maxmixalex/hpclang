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

struct hpclang_rpl_command_program
{
    struct hpclang_rpl_reader reader;
    uint32_t address, limit, number, command;
};

struct hpclang_rpl_cached_program
{
    uint32_t number, command, address, span;
};

/* Match a complete caller-bounded TEMP allocation containing ROMP + DOCOL
 * against the independently resolved native command program. Source number,
 * command, mapping and address must come from validated catalog/bank/link
 * resolution, not from scanning for equal bytes. Require an exact allocation
 * extent and identical program nibbles; no trailing objects are accepted.
 * Source limit is a maximum readable span. Readers/mappings remain stable.
 * Output changes only on success (zero); errors are span results. No allocation
 * or writes. This proves the cache's identity/content, not the pointer type of
 * an arbitrary machine-stack value or completeness of execution roots. */
int hpclang_rpl_cached_program_match(const struct hpclang_rpl_reader *cache, uint32_t address,
                                     uint32_t extent,
                                     const struct hpclang_rpl_command_program *source,
                                     struct hpclang_rpl_cached_program *view);

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
 * A nonzero switch additionally emits EXECUTION for its own five-nibble field;
 * resolving the payload bank does not resolve the switch's executable reference.
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
 * Conversely, numerical address matches in an opaque payload are not evidence
 * of pointers: ownership/type evidence must distinguish raw data from roots.
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

struct hpclang_rpl_reference_node
{
    uint32_t address, limit, body_prologue;
};

/* Enumerate structural edges through all INLINE/COMPACT descendants in one
 * stable mapping. Caller supplies an exclusive, disjoint work array and a
 * positive node budget. The scheduler allocates no memory and uses no recursion;
 * underlying object parsers retain their own bounded recursion. The node budget
 * limits scheduled visits, not parser reads or callback count. Immediate edges are
 * delivered in native order; queued children are processed last-in first-out.
 * Aliased children are visited again and consume budget; this is not a visited
 * set. Capacity/budget exhaustion returns BOUNDS and invalidates partial output.
 * DIRECT/ACPTR/ROMP/FLASHP/ATTACHMENT targets are reported but not followed;
 * OPAQUE/EXECUTION/EMBEDDED_ENTRY remain explicit unresolved obligations.
 * Success only establishes structural enumeration, not graph completeness or
 * relocation safety. Callback failures propagate unchanged. Keep reader,
 * callback and workspace stable; discard all output on any failure. */
int hpclang_rpl_reference_visit_nested(const struct hpclang_rpl_reader *reader,
                                       const struct hpclang_rpl_reference_node *root,
                                       struct hpclang_rpl_reference_node *workspace,
                                       uint32_t capacity, uint32_t node_budget,
                                       hpclang_rpl_reference_visit_fn visit, void *context);

/* Observe each successful native nibble read during the same nested traversal,
 * including prologues, lengths and structural metadata. Repeated reads may
 * repeat observations; compact synthetic prologues are not native dependencies.
 * Feed these observations to a guard builder using the reader's independently
 * resolved mapping. Both callbacks are read-only; any callback error propagates
 * unchanged and invalidates all partial edges/dependencies. Failed reads and
 * invalid nibbles are not reported as observations. Keep all inputs/mappings
 * stable through traversal and any later validation/commit. This records only
 * actual parser reads: opaque contents and unfollowed edges remain unresolved,
 * and success does not establish complete references or authorize mutation. */
typedef int (*hpclang_rpl_reference_dependency_fn)(void *context, uint32_t address, uint8_t value);
int hpclang_rpl_reference_visit_guarded_nested(const struct hpclang_rpl_reader *reader,
                                               const struct hpclang_rpl_reference_node *root,
                                               struct hpclang_rpl_reference_node *workspace,
                                               uint32_t capacity, uint32_t node_budget,
                                               hpclang_rpl_reference_visit_fn visit,
                                               hpclang_rpl_reference_dependency_fn dependency,
                                               void *context);

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
 * and target must each be qualified separately; bank indices alone cannot
 * establish disjointness. Equal identities require common address coordinates
 * in plan_qualified_shift; translated aliases need plan_offset_shift below.
 * These are not raw bank-switch entry addresses.
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

/* As qualified_shift, but field+field_offset and target+target_offset locate
 * each address in the moving region's coordinates. Offsets affect membership
 * only: patches retain the old source field and native encoded addresses.
 * Relative encodings account for movement of either endpoint independently.
 * Caller must prove these affine translations and the same delta remain valid
 * across the entire relevant old/new ranges; aliases spanning discontinuities
 * require separate resolution. Distinct mapping identities mean proven physical
 * disjointness, not merely unequal bank numbers. Out-of-range native encodings
 * or partially moved five-nibble fields reject without changing output.
 * No map discovery, switch changes, code inspection or write authorization.
 * EXECUTION remains unsupported; its code obligations are not bypassed. */
int hpclang_rpl_reference_plan_offset_shift(const struct hpclang_rpl_reference *reference,
                                            const struct hpclang_rpl_reference_mappings *mappings,
                                            int32_t field_offset, int32_t target_offset,
                                            uint32_t first, uint32_t end, int32_t delta,
                                            struct hpclang_rpl_reference_patch *patch);

/* Explicitly plan the stored five-nibble address of an EXECUTION event after
 * independently resolving its field and target mappings. Accept only that event
 * kind, with zero extent/auxiliary and a nonzero target; reject a move to the
 * zero/no-function sentinel. The ordinary planners still reject EXECUTION.
 * This plans the address field only: retain the separate obligation to inspect
 * the target code, its embedded references and live execution state before any
 * mutation. It neither discovers an execution entry nor certifies code safety.
 * Same return/output/old-field contract as plan_qualified_shift; no writes. */
int hpclang_rpl_reference_plan_execution_shift(
    const struct hpclang_rpl_reference *reference,
    const struct hpclang_rpl_reference_mappings *mappings, uint32_t first, uint32_t end,
    int32_t delta, struct hpclang_rpl_reference_patch *patch);

struct hpclang_rpl_reference_edit
{
    uint32_t mapping, field, expected, value, width; /* Explicit 1..5 nibbles. */
};

struct hpclang_rpl_reference_edit_builder
{
    struct hpclang_rpl_reference_edit *edits;
    uint32_t capacity, count;
    int error;
};

/* Merge independently discovered 1..5-nibble edits into a sorted, nonoverlapping batch.
 * Exact duplicates (including unchanged fields) consume no extra capacity.
 * Conflicting duplicates and partial overlaps are malformed. Any error latches
 * and invalidates count; begin is required to restart. Caller owns storage and
 * normalizes physical aliases before adding entries. No IO or allocation.
 * Only complete successful discovery permits later validation/use of the batch;
 * this builder does not discover missing references or authorize relocation. */
int hpclang_rpl_reference_edits_begin(struct hpclang_rpl_reference_edit_builder *builder,
                                      struct hpclang_rpl_reference_edit *edits, uint32_t capacity);
int hpclang_rpl_reference_edits_add(struct hpclang_rpl_reference_edit_builder *builder,
                                    const struct hpclang_rpl_reference_edit *edit);

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
struct hpclang_rpl_catalog_mappings
{
    uint32_t payload, bank_switch;
};

/* Return zero with normalized physical mapping identities, or an error.
 * Resolve every payload, including bindings without a bank switch. A nonzero
 * bank_switch is executable code: resolve its own mapping independently of the
 * payload mapping it selects. For zero (no switch), bank_switch mapping is
 * ignored. Read-only; keep both resolutions stable through validation/commit. */
typedef int (*hpclang_rpl_catalog_mapping_fn)(void *context,
                                              const struct hpclang_rpl_library_binding *binding,
                                              struct hpclang_rpl_catalog_mappings *mappings);

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
 * data. For N records, supply 2*N edits (payload address and bank-switch address)
 * and 1+2*N guards (count, IDs and flags). Arrays must be disjoint from each
 * other and all input/IO state. On success counts describe the complete plan;
 * on error both counts are zero and all partially filled entries are invalid.
 * Zero bank-switch sentinels stay zero, even if the move includes address zero.
 * Reject moving a nonzero function target to zero, which would change its meaning.
 * Duplicate IDs are preserved as distinct records. Fields retain old addresses.
 * Keep mappings/catalog/buffers exclusively owned throughout later validation
 * and commit. This does not discover other roots, repair references or authorize
 * movement/Stock return. Zero is success; other results are span/resolver errors. */
int hpclang_rpl_catalog_plan_relocation(const struct hpclang_rpl_reader *reader, uint32_t address,
                                        uint32_t limit, const struct hpclang_rpl_catalog_move *move,
                                        hpclang_rpl_catalog_mapping_fn resolve, void *context,
                                        struct hpclang_rpl_catalog_reference_plan *plan);

struct hpclang_rpl_catalog_locations
{
    struct hpclang_rpl_catalog_mappings mappings;
    int32_t payload_offset, bank_switch_offset;
};
typedef int (*hpclang_rpl_catalog_location_fn)(void *context,
                                               const struct hpclang_rpl_library_binding *binding,
                                               struct hpclang_rpl_catalog_locations *locations);

/* Catalog plan with independently proven affine translations into the moving
 * region's coordinates. catalog_offset applies to catalog fields; the resolver
 * supplies payload and executable-switch offsets independently. Same proofs and
 * old/native-address output contract as plan_offset_shift. Guards and edits
 * retain catalog_mapping and original catalog addresses for validation/commit.
 * Offsets never alter zero switch sentinels or permit a function target to become
 * zero. Execution-code inspection and complete roots remain separate obligations.
 * Same capacities, zero counts on error and stable-input requirements as above. */
int hpclang_rpl_catalog_plan_relocation_offsets(const struct hpclang_rpl_reader *reader,
                                                uint32_t address, uint32_t limit,
                                                const struct hpclang_rpl_catalog_move *move,
                                                int32_t catalog_offset,
                                                hpclang_rpl_catalog_location_fn resolve,
                                                void *context,
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
