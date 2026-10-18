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

/* Decode only a caller-proven instruction boundary: GOC/GONC, GOTO/GOSUB,
 * GOLONG/GOSUBL and GOYES operands of register, HST, ST, P and A/C bit tests.
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
