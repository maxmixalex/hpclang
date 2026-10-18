#ifndef HPCLANG_RPL_CODE_H
#define HPCLANG_RPL_CODE_H

#include "rpl-reference.h"

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
