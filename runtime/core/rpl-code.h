#ifndef HPCLANG_RPL_CODE_H
#define HPCLANG_RPL_CODE_H

#include "rpl-reference.h"

struct hpclang_rpl_code_bank_entry
{
    uint32_t selector, dispatch, body;
};

/* Recognize the reviewed P=0/P=1 switch-program shapes in a caller-qualified
 * code mapping: full D0 literal, GOTO, at most 16 ?P= tests, then (for P=1)
 * a replacement full D0 literal, followed by P=0 / LC FF000 / CONFIG /
 * C=DAT0.A / D0=C.A / CONFIG / DAT0=C.B / UNCONFIG / opcode 03 return.
 * first/end bound the code mapping, not the selector data. Observe every code
 * nibble read; output changes only on zero. Callback errors invalidate all
 * partial dependencies; positive errors become MALFORMED. Other P values and
 * code shapes are unsupported. P=1 reports the exit selector, not the entry's
 * selector or a claim to restore the prior bank. No fixed addresses are used.
 * This proves syntax/operands, NOT firmware interpreter identity or the device
 * semantics of CONFIG and DAT0. Caller still qualifies those, mutable selector
 * contents, owner/geometry state, pending interrupts and return-stack roots.
 * No execution, data access, mapping change or mutation authorization. */
int hpclang_rpl_code_bank_entry_inspect(const struct hpclang_rpl_reader *reader, uint32_t first,
                                        uint32_t end, uint32_t entry, unsigned p,
                                        struct hpclang_rpl_code_bank_entry *view,
                                        hpclang_rpl_reference_dependency_fn dependency,
                                        void *context);

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
    HPCLANG_RPL_CODE_CONSTANT,
    HPCLANG_RPL_CODE_D0,
    HPCLANG_RPL_CODE_D1,
    HPCLANG_RPL_CODE_NEXT_PC
};

struct hpclang_rpl_code_literal
{
    uint32_t instruction, length, field;
    enum hpclang_rpl_code_register destination;
    unsigned nibbles;
    uint64_t mask, value;
};

/* Decode LC/LA or D0/D1 immediate loads at a caller-proven instruction boundary.
 * LC/LA require the proven P value. D0/D1 loads replace their low 2, 4 or 5
 * nibbles independently of P, preserving any remaining address nibbles.
 * LC/LA loaded nibbles wrap modulo 16 from P; other register bits survive.
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

struct hpclang_rpl_code_pc_exchange
{
    struct hpclang_rpl_code_assignment capture;
    uint32_t target;
};

/* APCEX/CPCEX consume the OLD low 20 register bits as the next PC and replace
 * those bits with instruction+4. The upper register bits, flags and machine
 * return stack are unchanged. capture describes that NEXT_PC assignment and
 * can propagate caller-proven instruction-mapping origins. The target needs
 * independent register provenance and mapping qualification; the captured PC
 * is not an ordinary scalar literal. Both effects belong to one instruction.
 * Inputs describe the registers AT this proven instruction boundary. No writes
 * or relocation authorization. Reject next-PC wrap; errors preserve output. */
int hpclang_rpl_code_pc_exchange_inspect(const struct hpclang_rpl_reader *reader,
                                         uint32_t instruction, uint32_t limit, uint32_t a_low,
                                         uint32_t c_low,
                                         struct hpclang_rpl_code_pc_exchange *exchange);

/* Describe one plain register copy/zero or P literal/nibble transfer. Nibble
 * indices refer to 64-bit A/B/C/D/R0..R4, 20-bit D0/D1 or one-nibble P. Other destination
 * nibbles survive; source nibbles come from the pre-instruction state. Supports
 * classic field copies and 81A register copies with standard fields (0..7,F).
 * D0/D1=A/C copies five low nibbles; D0/D1=AS/CS copies four, preserving
 * the fifth address nibble. Source A/C remains unchanged.
 * A=PC/C=PC copy the following instruction address into five low nibbles.
 * Their source is NEXT_PC and constant holds instruction+4 in the instruction
 * reader's mapping, not an arbitrary literal or a physical address. Preserve
 * that mapping and the instruction dependency when propagating this fact.
 * Captures whose next PC leaves the 20-bit address space are rejected.
 * PC exchanges and control transfers are not plain assignments.
 * Exchanges, arithmetic, memory reads and custom fields remain unsupported.
 * p must be 0..15 and is the proven P value AT the instruction, needed for
 * P/WP fields. Interpreter-internal caches updated by P writes are not modeled.
 * A copy propagates caller-proven provenance; it does not create address
 * provenance from a numeric value. No mutation or relocation authorization.
 * Error leaves output untouched. */
int hpclang_rpl_code_assignment_inspect(const struct hpclang_rpl_reader *reader,
                                        uint32_t instruction, uint32_t limit, unsigned p,
                                        struct hpclang_rpl_code_assignment *assignment);

struct hpclang_rpl_code_exchange
{
    uint32_t instruction, length;
    enum hpclang_rpl_code_register left, right;
    unsigned first, nibbles;
};

struct hpclang_rpl_code_memory
{
    uint32_t instruction, length;
    enum hpclang_rpl_code_register data, address;
    unsigned first, nibbles, load;
};

struct hpclang_rpl_code_address_adjust
{
    uint32_t instruction, length;
    enum hpclang_rpl_code_register address;
    int32_t delta;
};

/* Describe D0/D1 plus or minus an immediate 1..16 at a proven instruction
 * boundary. This is arithmetic, not a nibble copy: carries/borrows invalidate
 * copied origin tokens. Execution wraps the address at 20 bits and replaces
 * carry: one for overflow/underflow, zero otherwise. Any tracked carry fact
 * must also be updated or invalidated. The caller must establish the incoming address and
 * retain the instruction/mapping dependencies, then independently qualify the
 * resulting address (including wrap and mapping boundaries). No data access,
 * pointer provenance or relocation authorization. Errors preserve output. */
int hpclang_rpl_code_address_adjust_inspect(const struct hpclang_rpl_reader *reader,
                                            uint32_t instruction, uint32_t limit,
                                            struct hpclang_rpl_code_address_adjust *adjust);

/* Describe DAT0/DAT1 transfers to/from A/C at a proven instruction boundary.
 * load=1 reads memory into the register, load=0 writes the register to memory.
 * Memory starts at the caller-proven D0/D1 address; first selects the register
 * field, not an offset added to memory. Neither address register is advanced.
 * Supports fixed A/B fields, standard P/WP/XS/X/S/M/B/W/A fields and explicit
 * low-register nibble counts. p must be the established value at this instruction.
 * No data-memory reads/writes, address-wrap policy, pointer classification or
 * relocation permission is implied. Guard code, P, address and mapping inputs
 * separately before using this descriptor. Errors leave the output untouched. */
int hpclang_rpl_code_memory_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                    uint32_t limit, unsigned p,
                                    struct hpclang_rpl_code_memory *memory);

/* Abstract origins, not register values or pointer classifications. Each token
 * names one caller-owned nibble fact; zero means unknown. Register numbers are
 * the enum above, through D1. CONSTANT's slot and unused high nibbles are unused.
 * Initialize adjustments to zero for ordinary nibble seeds; each is a canonical
 * 20-bit modular offset. Callers establish seeds, memory identity/aliasing, instruction identity
 * and the executed path. Unmodeled writes must invalidate their affected facts; never skip
 * unsupported instructions and continue with stale origins. */
struct hpclang_rpl_code_origins
{
    uint32_t registers[HPCLANG_RPL_CODE_D1 + 1][16];
    /* Low five digits represent (source + adjustment) modulo 20 bits.
     * Tokens name the unadjusted source, not the current computed digits.
     * Zero adjustment is an ordinary nibble copy; missing tokens stay unknown. */
    uint32_t adjustment[HPCLANG_RPL_CODE_D1 + 1];
};

struct hpclang_rpl_code_values
{
    uint64_t registers[HPCLANG_RPL_CODE_D1 + 1];
    unsigned carry, decimal;
};

struct hpclang_rpl_code_control
{
    uint32_t pc, carry, cursor, stack[8];
    uint32_t stack_origins[8][5];
};

struct hpclang_rpl_code_return_use
{
    uint32_t present, target, origins[5];
};

/* Advance a private suspended-state copy through RTN/RTNSC/RTNCC, direct
 * jumps/calls and GOC/GONC (including their zero-displacement returns).
 * Carry is canonical 0/1. Calls overwrite the next circular slot and clear its
 * old origins: a computed return PC is not an original stored field. A taken
 * return reports the consumed slot's origins before clearing/popping it.
 * Untaken conditions leave the stack unchanged. Direct branch operand repair
 * remains the caller's responsibility via branch_inspect/plan_shift.
 * RTNSXM and predicates using other flags are unsupported; do not skip them.
 * Caller establishes instruction/path/mapping identity and owns all input
 * storage. No native writes or relocation permission. Errors preserve state
 * and use; success always replaces use (present=0 except a taken return).
 * State and use are disjoint from each other and the instruction reader. */
int hpclang_rpl_code_control_step(const struct hpclang_rpl_reader *instructions, uint32_t limit,
                                  struct hpclang_rpl_code_control *state,
                                  struct hpclang_rpl_code_return_use *use);

/* Name an instruction/data nibble already read from a stable mapping. data is
 * zero for encoded literals and one for data loads. Return zero on success;
 * token zero explicitly means unknown. The caller owns token/mapping identity
 * and guards. This callback must not mutate the readers or suspended state. */
typedef int (*hpclang_rpl_code_token_fn)(void *context, unsigned data, uint32_t address,
                                         uint8_t value, uint32_t *token);

/* Evaluate one supported data instruction from caller-proven incoming values,
 * propagating origins alongside values. Supports inspected literals, copies,
 * exchanges, memory LOADS, D0/D1 immediate adjustments, SETHEX/SETDEC and D.A+1.
 * D.A+1 uses the proven decimal flag (0 hexadecimal, 1 decimal); decimal input
 * digits above nine are unsupported. Arithmetic clears the affected origins.
 * No stores or control
 * transfers: an unsupported instruction stops the caller's walk. P, D0/D1 and
 * carry/decimal must be canonical. NEXT_PC captures deliberately receive unknown
 * origins: a computed PC is not an encoded literal cell.
 * Readers, mappings and all input state remain stable; data reads must be pure
 * memory, not device registers. The caller separately records load-address and
 * instruction dependencies before advancing. This does not establish a path,
 * authorize repairs, or prove complete reference coverage. On error values,
 * origins and length remain unchanged; callback observations must be discarded.
 * State/output objects are disjoint, and callbacks may not change them. */
int hpclang_rpl_code_data_step(const struct hpclang_rpl_reader *instructions,
                               const struct hpclang_rpl_reader *data, uint32_t instruction,
                               uint32_t limit, struct hpclang_rpl_code_values *values,
                               struct hpclang_rpl_code_origins *origins,
                               hpclang_rpl_code_token_fn token, void *context, uint32_t *length);

struct hpclang_rpl_code_execution
{
    struct hpclang_rpl_code_values values;
    struct hpclang_rpl_code_origins origins;
    struct hpclang_rpl_code_control control;
};

enum hpclang_rpl_code_use_kind
{
    HPCLANG_RPL_CODE_USE_LOAD_ADDRESS = 1,
    HPCLANG_RPL_CODE_USE_REGISTER_TARGET,
    HPCLANG_RPL_CODE_USE_MEMORY_TARGET,
    HPCLANG_RPL_CODE_USE_RETURN_TARGET
};

struct hpclang_rpl_code_address_use
{
    uint32_t kind, value, origins[5];
    uint32_t adjustment; /* value = original source + adjustment modulo 20 bits. */
};

struct hpclang_rpl_code_step_uses
{
    uint32_t count, has_branch;
    struct hpclang_rpl_code_address_use addresses[4];
    struct hpclang_rpl_code_branch branch;
};

/* One C step combining the data/control evaluators with register/indirect PC
 * transfers, PC exchanges, C/RSTK transfers and qualified HP49g+/HP50g 81BD.
 * 81BD emits three dependent load addresses followed by the memory PC target;
 * B retains its low-five-digit source plus a modular adjustment; arithmetic
 * on D and D0 clears affected origins. Full low-address copies/exchanges retain
 * adjustments; partial overwrites discard the affected expression. Its B/D updates use
 * the low 32-bit ARM words, independent of Saturn decimal mode. A D0 result
 * beyond 20 bits is rejected. values.carry and control.carry
 * describe the same state and must agree. Emit the five source tokens of every
 * consumed load address, register jump target, indirect-memory target or return.
 * Copies alone emit no use. Direct encoded operands are described by branch,
 * not misrepresented as copied address digits. Computed PC origins stay unknown.
 * The caller qualifies entry, memory mappings, tokens and instruction boundaries,
 * guards all reads, limits the walk and rejects unsupported instructions. A zero
 * origin does not authorize an unguarded repair. This is a private simulation,
 * not native mutation, complete reference closure or an installation permission.
 * State and uses are disjoint; callbacks/readers are pure and may not alter them.
 * Errors leave both outputs unchanged; discard callback observations on failure. */
int hpclang_rpl_code_execution_step(const struct hpclang_rpl_reader *instructions,
                                    const struct hpclang_rpl_reader *data, uint32_t limit,
                                    struct hpclang_rpl_code_execution *state,
                                    hpclang_rpl_code_token_fn token, void *context,
                                    struct hpclang_rpl_code_step_uses *uses);

struct hpclang_rpl_code_origin_cell
{
    uint32_t mapping, field, expected;
};

/* Resolve a caller-owned token to its original, guarded nibble cell. Zero is
 * unknown and is never passed to resolve. Return zero on success, negative on
 * failure; positive returns are malformed. Normalize physical aliases here. */
typedef int (*hpclang_rpl_code_origin_resolve_fn)(void *context, uint32_t token,
                                                  struct hpclang_rpl_code_origin_cell *cell);

/* Project a proven 20-bit address use back to its five original nibble sources.
 * tokens are in consumed low-to-high digit order; they need not name contiguous
 * cells or one mapping. Resolve and check all expected digits against current,
 * then sort/coalesce the repairs by mapping/address. Duplicate source cells must
 * agree on both old and replacement digits. Keep unchanged digits in the plan,
 * allowing a full pointer field to deduplicate with independently found roots.
 * Caller proves address consumption, replacement/target mapping, source identity
 * and all instruction/data dependencies. This does not infer pointers from
 * values, discover missing origins, or authorize writes. Mappings and sources
 * remain guarded until commit. Callbacks are read-only; their observations on
 * failure must be discarded. No allocation or IO except resolve. On success
 * write count (1..5) edits; on error leave outputs untouched. Output storage is
 * five edits plus a disjoint count, also disjoint from callback-owned storage. */
int hpclang_rpl_code_origins_plan(const uint32_t *tokens, uint32_t current, uint32_t replacement,
                                  hpclang_rpl_code_origin_resolve_fn resolve, void *context,
                                  struct hpclang_rpl_reference_edit *edits, unsigned *count);

/* Propagate caller-established instruction-field tokens through LC/LA or a
 * D0/D1 immediate load. encoded contains count tokens in encoded nibble order,
 * not destination order; zero means unknown. A/C starts at the proven P and
 * wraps at sixteen digits, while D0/D1 starts at zero and preserves any high
 * digits of a partial load. P is required even for a full A/C load: its mask
 * alone cannot recover the rotation. Keep the instruction mapping and P guarded.
 * A literal's numeric value does not classify it as an address. This records
 * where bits came from, not permission to relocate the instruction operand.
 * Snapshot tokens before writes, allowing encoded to overlap state. Validate
 * destination, count, P and mask before mutation; errors preserve state.
 * literal must be disjoint from writable token storage. */
int hpclang_rpl_code_origins_literal(struct hpclang_rpl_code_origins *state,
                                     const struct hpclang_rpl_code_literal *literal, unsigned p,
                                     const uint32_t *encoded, unsigned count);

/* Apply descriptors obtained from the inspectors to abstract facts only.
 * Assignment snapshots its source, exchange snapshots both sides, and memory
 * transfers snapshot the selected source before writing. Overlapping tag arrays
 * are supported. Scalar constants clear destination facts. NEXT_PC requires
 * five caller-established tokens (all zero is allowed), separately guarded for
 * the instruction mapping. Memory tokens name exactly count consecutive nibbles
 * starting at the established D0/D1 address; load reads them, store replaces them.
 * Descriptors must be disjoint from writable token storage. Validate register
 * and field bounds before any change; errors preserve state and memory tags.
 * These operations do not execute code, access native memory, create pointer
 * provenance from numeric values, or establish complete relocation coverage. */
int hpclang_rpl_code_origins_assign(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_assignment *assignment,
                                    const uint32_t *next_pc);
int hpclang_rpl_code_origins_exchange(struct hpclang_rpl_code_origins *state,
                                      const struct hpclang_rpl_code_exchange *exchange);
int hpclang_rpl_code_origins_memory(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_memory *transfer,
                                    uint32_t *memory, unsigned count);

/* Address arithmetic destroys all five copied nibble origins of D0/D1,
 * including when some resulting digits happen to be unchanged. Other slots
 * are preserved. Validate a nonzero signed immediate in [-16,16] before any
 * change. Carry is not represented here; callers must separately replace or
 * invalidate their carry fact. This does not qualify the resulting address. */
int hpclang_rpl_code_origins_adjust(struct hpclang_rpl_code_origins *state,
                                    const struct hpclang_rpl_code_address_adjust *adjust);

/* Describe a simultaneous exchange of the same field in two registers. Both
 * sources are the pre-instruction values; preserve all nibbles outside the field.
 * Supports A/B/C/D pairs and A/C with R0..R4, standard fields (0..7,F).
 * Also supports A/C with 20-bit D0/D1: EX swaps low five nibbles, XS low four;
 * preserve the fifth D0/D1 nibble for XS and all higher A/C nibbles.
 * P exchanges and custom fields remain unsupported.
 * p is the proven P value at this instruction. This transfers caller-proven
 * provenance only; it does not establish addresses or authorize relocation.
 * No writes; errors leave output untouched. Guard the instruction and P before
 * using the result, as for assignment_inspect. */
int hpclang_rpl_code_exchange_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                      uint32_t limit, unsigned p,
                                      struct hpclang_rpl_code_exchange *exchange);

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
