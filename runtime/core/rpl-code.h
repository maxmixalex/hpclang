#ifndef HPCLANG_RPL_CODE_H
#define HPCLANG_RPL_CODE_H

#include "rpl-reference.h"

/* Frame one Saturn/Saturn+ instruction at a caller-proven boundary. limit is
 * the available nibble count, not an end address. Validate every consumed nibble
 * and stop at unsupported encodings; never fall back to treating 80B as the
 * classic three-nibble BUSCC. Includes the GOYES/RTNYES operand in predicates.
 * This is framing only, not a proof of legal execution, register/address
 * semantics, reachability, ownership or absence of embedded references. In
 * particular, inline data must be excluded by independent code provenance.
 * No writes; length changes only on success. */
int hpclang_rpl_code_extent(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                            uint32_t limit, uint32_t *length);

typedef int (*hpclang_rpl_code_visit_fn)(void *context, uint32_t instruction, uint32_t length);

/* Walk an independently established instruction-only interval, without following
 * branches. Propagate the first decoder/callback error; callbacks preceding an
 * error have already run and must not mutate the input or commit relocation.
 * Input mapping must remain stable throughout. Zero means exact framing of the
 * interval, not complete reference coverage or permission to move its contents. */
int hpclang_rpl_code_visit(const struct hpclang_rpl_reader *reader, uint32_t first, uint32_t length,
                           hpclang_rpl_code_visit_fn visit, void *context);

struct hpclang_rpl_code_branch
{
    uint32_t instruction, length, field, width, base, target, encoded;
    unsigned conditional, call;
};

struct hpclang_rpl_code_patch
{
    uint32_t field, width, expected, value;
};

struct hpclang_rpl_code_indirect
{
    uint32_t instruction, register_value, target;
    unsigned register_c, via_memory;
};

enum hpclang_rpl_code_register
{
    HPCLANG_RPL_CODE_A,
    HPCLANG_RPL_CODE_B,
    HPCLANG_RPL_CODE_C,
    HPCLANG_RPL_CODE_D,
    HPCLANG_RPL_CODE_R0,
    HPCLANG_RPL_CODE_R1,
    HPCLANG_RPL_CODE_R2,
    HPCLANG_RPL_CODE_R3,
    HPCLANG_RPL_CODE_R4,
    HPCLANG_RPL_CODE_P,
    HPCLANG_RPL_CODE_CONSTANT
};

struct hpclang_rpl_code_literal
{
    uint32_t instruction, length, field;
    enum hpclang_rpl_code_register destination;
    unsigned nibbles;
    uint64_t mask, value;
};

/* Decode LC/LA at a caller-proven instruction boundary with the proven P value.
 * The loaded nibbles wrap modulo 16 from P; other register bits survive.
 * next_register = (old_register & ~mask) | value. field is the first encoded
 * literal nibble, not a pointer field. Constants do not establish address
 * provenance, mapping identity or permission to relocate. Preserve guards on
 * the full instruction and P dependency before using this in data-flow analysis.
 * No writes; invalid/unsupported input leaves output untouched. */
int hpclang_rpl_code_literal_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                     uint32_t limit, unsigned p,
                                     struct hpclang_rpl_code_literal *literal);

struct hpclang_rpl_code_assignment
{
    uint32_t instruction, length;
    enum hpclang_rpl_code_register source, destination;
    unsigned source_first, destination_first, nibbles;
    uint32_t constant;
};

/* Describe one plain register copy/zero or P literal/nibble transfer. Nibble
 * indices refer to 64-bit A/B/C/D/R0..R4 or one-nibble P. Other destination
 * nibbles survive; source nibbles come from the pre-instruction state. Supports
 * classic field copies and 81A register copies with standard fields (0..7,F).
 * Exchanges, arithmetic, memory reads and custom fields remain unsupported.
 * p must be 0..15 and is the proven P value AT the instruction, needed for
 * P/WP fields. Interpreter-internal caches updated by P writes are not modeled.
 * A copy propagates caller-proven provenance; it does not create address
 * provenance from a numeric value. No mutation or relocation authorization.
 * Error leaves output untouched. */
int hpclang_rpl_code_assignment_inspect(const struct hpclang_rpl_reader *reader,
                                        uint32_t instruction, uint32_t limit, unsigned p,
                                        struct hpclang_rpl_code_assignment *assignment);

/* Resolve PC=A, PC=C, PC=(A), PC=(C) at a proven instruction boundary.
 * a_low/c_low must describe the registers AT that instruction, not merely an
 * earlier suspended snapshot. Only the low 20 bits are consumed. The separate
 * pointer reader must resolve the selected register's mapping; it is unused
 * for direct register transfers. An indirect pointer must fit without wrapping.
 * Success reports a register use and (when via_memory) a five-nibble pointer
 * field at register_value. It does not establish register provenance, target
 * ownership/mapping, or authorize relocation. Guard both instruction and any
 * dereferenced cell before mutation; repair register sources separately from
 * memory pointer fields. No writes; output changes only on zero. */
int hpclang_rpl_code_indirect_inspect(const struct hpclang_rpl_reader *instructions,
                                      const struct hpclang_rpl_reader *pointers,
                                      uint32_t instruction, uint32_t limit, uint32_t a_low,
                                      uint32_t c_low, struct hpclang_rpl_code_indirect *transfer);

/* Plan an immediate D0=(5)/D1=(5) data-address update at a caller-proven
 * instruction boundary. Partial loads, A/C literals and computed addresses
 * require register/data-flow evidence and are not accepted here. The same
 * mapping, guard, return-value and OLD-field contract as branch_plan_shift
 * applies. Source movement alone never changes a full absolute address. */
int hpclang_rpl_code_pointer_plan_shift(const struct hpclang_rpl_reader *reader,
                                        uint32_t instruction, uint32_t limit,
                                        const struct hpclang_rpl_reference_mappings *mappings,
                                        uint32_t first, uint32_t end, int32_t delta,
                                        struct hpclang_rpl_code_patch *patch);

/* Decode only a caller-proven instruction boundary: GOC/GONC, GOTO/GOSUB,
 * GOLONG/GOSUBL, absolute GOVLNG/GOSBVL, and GOYES operands of register, HST,
 * ST, P and A/C bit tests. Absolute operands have width 5 and base zero.
 * Zero-displacement conditional returns are UNSUPPORTED, not
 * address operands. Addresses wrap at 20 bits; instruction bytes must fit in
 * the supplied stable mapping and limit. No scanning or target dereference.
 * Success does not prove target instruction boundaries, mapping identity,
 * reachability, or complete CODE/reference coverage. Output changes on zero. */
int hpclang_rpl_code_branch_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                    uint32_t limit, struct hpclang_rpl_code_branch *branch);

/* Inspect and plan a qualified move of [first,end) by delta. Reject a move
 * splitting the instruction, an unencodable displacement, or conversion of a
 * conditional branch into a return. The target may belong to another CODE
 * object; ownership and target mapping must be established by the caller.
 * Return 1 with a short operand patch at its OLD address, 0 if unchanged, or
 * a negative span result. Output changes only on 1. No reads beyond the
 * instruction, writes, allocation or permission to bypass opaque coverage.
 * Retain guards on the complete instruction and mappings until commit; this
 * short patch must retain its explicit width when converted to a reference
 * edit (with the independently resolved source mapping). */
int hpclang_rpl_code_branch_plan_shift(const struct hpclang_rpl_reader *reader,
                                       uint32_t instruction, uint32_t limit,
                                       const struct hpclang_rpl_reference_mappings *mappings,
                                       uint32_t first, uint32_t end, int32_t delta,
                                       struct hpclang_rpl_code_patch *patch);

#endif
