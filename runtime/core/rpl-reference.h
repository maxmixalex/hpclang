#ifndef HPCLANG_RPL_REFERENCE_H
#define HPCLANG_RPL_REFERENCE_H

#include "rpl-object-span.h"

enum hpclang_rpl_reference_kind
{
    HPCLANG_RPL_REFERENCE_INLINE,
    HPCLANG_RPL_REFERENCE_COMPACT,
    HPCLANG_RPL_REFERENCE_DIRECT,
    HPCLANG_RPL_REFERENCE_ACPTR,
    HPCLANG_RPL_REFERENCE_ROMP,
    HPCLANG_RPL_REFERENCE_FLASHP,
    HPCLANG_RPL_REFERENCE_OPAQUE,
    HPCLANG_RPL_REFERENCE_ATTACHMENT
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

struct hpclang_rpl_reference_patch
{
    uint32_t field; /* Old address: apply before moving the containing bytes. */
    uint32_t value; /* Replacement five-nibble word. */
};

/* Plan one edge previously validated by reference_visit, for relocation of
 * [first,end) by delta within the same Saturn mapping. Return 1 with a patch,
 * 0 when its encoding does not change, or a negative span result. Output only
 * changes on 1. Relative library fields account for movement of both ends.
 * INLINE/COMPACT need recursive traversal; ROMP/FLASHP retain their indices.
 * Banked ACPTR and opaque contents return UNSUPPORTED and require separate
 * resolution. This is neither a graph-completeness check nor a mutation API. */
int hpclang_rpl_reference_plan_shift(const struct hpclang_rpl_reference *reference, uint32_t first,
                                     uint32_t end, int32_t delta,
                                     struct hpclang_rpl_reference_patch *patch);

#endif
