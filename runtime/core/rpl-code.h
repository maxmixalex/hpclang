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

enum hpclang_rpl_code_relation
{
    HPCLANG_RPL_CODE_EQUAL,
    HPCLANG_RPL_CODE_UNEQUAL,
    HPCLANG_RPL_CODE_GREATER,
    HPCLANG_RPL_CODE_LESS,
    HPCLANG_RPL_CODE_GREATER_EQUAL,
    HPCLANG_RPL_CODE_LESS_EQUAL
};
struct hpclang_rpl_code_predicate
{
    uint64_t left_value, right_value;
    uint32_t instruction, first, nibbles, left, right, relation, taken, displacement;
};
/* Inspect five-nibble opcode-9 and fixed A-field 8A/8B compare/GOYES/RTNYES.
 * Values are unsigned selected fields; decimal mode does not change comparison.
 * right == CONSTANT means literal zero, independent of the pseudo-register.
 * displacement is the encoded byte; zero means a conditional stack return.
 * Do not interpret that as a jump back into this instruction. No state or stack
 * changes occur. Output changes only on success; inputs and mappings stay stable.
 * Operand origins, guard retention and preservation of the predicate after edits
 * remain caller obligations. A numeric result does not certify relocation. */
int hpclang_rpl_code_predicate_inspect(const struct hpclang_rpl_reader *reader,
                                       uint32_t instruction, uint32_t limit,
                                       const struct hpclang_rpl_code_values *values,
                                       struct hpclang_rpl_code_predicate *predicate);

struct hpclang_rpl_code_control
{
    uint32_t pc, carry, cursor, stack[8];
    uint32_t stack_origins[8][5];
    uint32_t stack_generated[8]; /* Producing call PC + 1; zero means not recorded. */
};

struct hpclang_rpl_code_return_use
{
    uint32_t present, target, origins[5];
    uint32_t generated;
};

/* Advance a private suspended-state copy through RTN/RTNSC/RTNCC, direct
 * jumps/calls and GOC/GONC (including their zero-displacement returns).
 * Carry is canonical 0/1. Calls overwrite the next circular slot and clear its
 * old origins and retain the producing call PC+1: a computed return PC is not
 * an original stored field. A taken
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

struct hpclang_rpl_execution_value
{
    uint32_t value, origins[5], adjustment;
};

struct hpclang_rpl_execution_subtraction
{
    struct hpclang_rpl_execution_value left, right;
    uint32_t first, nibbles, decimal, value, carry;
};

/* Optional subtraction recorder. Save the immutable relation and return five
 * nonzero tokens naming its RESULT digits, in a namespace disjoint from all
 * original/native sources. These tokens are expression facts, never writable
 * physical cells. Their resolver must reject them until the expression is
 * expanded/qualified; do not pass them through a physical-address resolver.
 * Copies/exchanges propagate these ordinary nibble facts. Partial overwrites
 * replace affected digits. Nested operands can name older records; this does
 * not implement recursive planning. Caller owns bounded storage and guards
 * instruction/P/mode. Return zero or a negative error; positive is malformed.
 * On any step/collection failure discard speculative records and observations.
 * Emitted for EA and BfA fields wholly within the low five digits only. */
typedef int (*hpclang_rpl_code_derive_fn)(
    void *context, uint32_t instruction, const struct hpclang_rpl_execution_subtraction *expression,
    uint32_t tokens[5]);

/* Evaluate one supported data instruction from caller-proven incoming values,
 * propagating origins alongside values. Supports inspected literals, copies,
 * exchanges, memory LOADS, D0/D1 immediate adjustments, SETHEX/SETDEC, A..D.A +/- 1
 * and EA/BfA (A=A-C, address field / fields 0..7). These arithmetic instructions use the proven
 * decimal flag (0 hexadecimal, 1 decimal); selected decimal input digits above
 * nine are unsupported. Hex increment/decrement retain the original or expression
 * source plus a 20-bit modular offset; BCD increment/decrement clear the affected
 * origins because their result is not a fixed binary offset. Missing sources stay
 * unknown. Retained offsets alone do not prove carry or later-effect invariance
 * under a proposed repair. Subtraction clears origins unless recorded. Increment,
 * decrement and EA/BfA ignore incoming carry, set carry to overflow/borrow and
 * preserve unselected digits. This entry does not retain derived expressions;
 * execution_step_derived can retain supported subtraction expressions through a callback.
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
    uint32_t hst; /* Native hardware status word; 82x clears only selected low bits. */
    /* RSTK can save an adjusted address without materializing its base digits. */
    uint32_t stack_adjustment[8];
    uint32_t status; /* Native software status word; 84n/85n clear/set bit n. */
};

struct hpclang_rpl_code_native_push
{
    uint64_t a, d, d1;
    uint32_t instruction, length, p, stores, store_address, store_value, exception_target;
};
/* Decode the qualified HP49g+/HP50g 80B40 service prefix through its first
 * memory effect. It resets P, sets A.A=0x3ac0 and decrements D.low32. If the
 * ORIGINAL D.A was zero, it returns native-profile target 0x3a9b without a
 * store. Otherwise D1-=5 and A.A must be stored there BEFORE threaded-next's
 * two dependent reads. Fields a/d/d1/p describe this prefix, not final NEXT.
 * No data access, native writes or origin fabrication. Callers must qualify
 * the ROM service mapping, constant provenance, store aliases, subsequent
 * reads/dispatch and error target before execution or relocation. The normal
 * path requires canonical D1>=5; failure leaves output unchanged. */
int hpclang_rpl_code_native_push_inspect(const struct hpclang_rpl_reader *instructions,
                                         uint32_t limit,
                                         const struct hpclang_rpl_code_execution *state,
                                         struct hpclang_rpl_code_native_push *effect);

struct hpclang_rpl_code_native_scan
{
    uint64_t b;
    uint32_t instruction, d0, d1, saved_b, saved_d1, target, cursor, carry, returns, reads;
};
/* Inspect the HP49g+/HP50g 81BA scan with P=0. Follow ordinary, nested-marker,
 * dispatch and return paths, bounded by max_reads five-nibble data reads.
 * Result describes B/D0/D1, carry, target and RSTK cursor; returns means the
 * popped stack word is cleared. All other modeled CPU values stay unchanged.
 * Native execution also saves old D1/B.low32 at CPU-relative byte offsets
 * 0xcb0/0xcb4, outside the captured CPU slice. These mandatory RAM effects are
 * described by saved_d1/saved_b, NOT performed or authorized here.
 * Caller must qualify ROM/mappings, padded native word reads, every read's
 * provenance, both saves and eventual target. P changes/cache effects remain
 * unsupported. No execution integration, allocation or writes. Failure leaves
 * output unchanged; readers may have observed a partial dependency sequence. */
int hpclang_rpl_code_native_scan_inspect(const struct hpclang_rpl_reader *instructions,
                                         const struct hpclang_rpl_reader *data, uint32_t limit,
                                         const struct hpclang_rpl_code_execution *state,
                                         uint32_t max_reads,
                                         struct hpclang_rpl_code_native_scan *output);

enum hpclang_rpl_code_scan_read_kind
{
    HPCLANG_RPL_CODE_SCAN_POINTER = 1,
    HPCLANG_RPL_CODE_SCAN_TAG,
    HPCLANG_RPL_CODE_SCAN_TARGET
};
struct hpclang_rpl_code_scan_read
{
    uint32_t kind, address, value;
};
/* Ordered five-nibble reads, after each complete successful word. The caller
 * retains addresses, values and their provenance, including branch decisions.
 * No writes. Any error invalidates ALL prior observations; positive callback
 * errors become MALFORMED and the scan output remains unchanged. */
typedef int (*hpclang_rpl_code_scan_read_fn)(void *context,
                                             const struct hpclang_rpl_code_scan_read *read);
int hpclang_rpl_code_native_scan_observed(const struct hpclang_rpl_reader *instructions,
                                          const struct hpclang_rpl_reader *data, uint32_t limit,
                                          const struct hpclang_rpl_code_execution *state,
                                          uint32_t max_reads, hpclang_rpl_code_scan_read_fn observe,
                                          void *context,
                                          struct hpclang_rpl_code_native_scan *output);

struct hpclang_rpl_code_scan_origins
{
    uint32_t b[8], d1[5], target[5], d1_adjustment, target_adjustment;
};
/* Qualify the entire scan before accepting a private execution step. Retain
 * ordered reads/predicates (replay native_scan_observed with immutable readers),
 * ROM identity, native padded reads, P=0/cache invariants and BOTH CPU-relative
 * RAM saves at 0xcb0/0xcb4 with original D1/B provenance. Record saves only in
 * caller scratch and preserve overwritten dependencies. This grants no live
 * writes. On dispatch supply nonzero producer tokens for B.low32, D1 and target;
 * D1/target relations must preserve their adjustments. Return paths retain
 * original B/D1 provenance and use the existing return-stack relation instead.
 * Callbacks cannot change state/readers/output effects. Discard all scratch on
 * any later step/visitor/collection error. Zero accepts, negative propagates,
 * positive becomes MALFORMED. A missing callback rejects native execution. */
typedef int (*hpclang_rpl_code_scan_fn)(void *context,
                                        const struct hpclang_rpl_code_native_scan *effect,
                                        const struct hpclang_rpl_code_execution *before,
                                        struct hpclang_rpl_code_scan_origins *origins);

enum hpclang_rpl_code_use_kind
{
    HPCLANG_RPL_CODE_USE_LOAD_ADDRESS = 1,
    HPCLANG_RPL_CODE_USE_REGISTER_TARGET,
    HPCLANG_RPL_CODE_USE_MEMORY_TARGET,
    HPCLANG_RPL_CODE_USE_RETURN_TARGET,
    HPCLANG_RPL_CODE_USE_STORE_ADDRESS
};

struct hpclang_rpl_code_address_use
{
    uint32_t kind, value, origins[5];
    uint32_t adjustment; /* value = original source + adjustment modulo 20 bits. */
    uint32_t generated;  /* For returns: producing call PC+1, not a nibble token. */
};

struct hpclang_rpl_code_step_uses
{
    uint32_t count, has_branch;
    struct hpclang_rpl_code_address_use addresses[4];
    struct hpclang_rpl_code_branch branch;
};

struct hpclang_rpl_code_store
{
    uint32_t instruction, first, nibbles;
    struct hpclang_rpl_code_address_use address;
    uint64_t value;                   /* Entire source register; selected digits start at first. */
    uint32_t origins[16], adjustment; /* Preserve computed low-five-digit relation. */
};

/* Record a simulated store in caller-owned scratch only; never write the target.
 * Qualify destination mapping/aliases, retain overwritten dependencies and supply
 * subsequent code/data reads and tokens from the updated scratch view. Computed
 * values need relation tokens: adjustment must not be flattened into old cell
 * tokens. Unknown sources remain unknown. Return zero or a negative error;
 * positive becomes MALFORMED. Discard all speculative effects on any later
 * step/visitor/collection failure. This callback grants no mutation permission. */
typedef int (*hpclang_rpl_code_store_fn)(void *context, const struct hpclang_rpl_code_store *store);

struct hpclang_rpl_code_configuration
{
    uint32_t instruction, value, origins[5], adjustment;
};
/* CONFIG observes C.A and its relation. The callback owns a guarded scratch
 * native snapshot, applies the qualified configuration model and updates its
 * simulated readers/maps. It must not touch live native state. Preserve all
 * original dependencies; discard scratch effects on any subsequent failure.
 * Zero succeeds; negative errors propagate; positive errors are malformed. */
typedef int (*hpclang_rpl_code_config_fn)(
    void *context, const struct hpclang_rpl_code_configuration *configuration);

/* Validate/retain the predicate and operand provenance before committing this
 * private step. before is the unchanged entry state, disjoint from callback
 * storage. Guard instruction/P selection and reject unqualified changed outcomes.
 * Zero accepts; negative errors propagate, positive errors are malformed.
 * Discard all callback observations on any step/collection error. */
typedef int (*hpclang_rpl_code_predicate_fn)(void *context,
                                             const struct hpclang_rpl_code_predicate *predicate,
                                             const struct hpclang_rpl_code_execution *before);

struct hpclang_rpl_code_constant
{
    struct hpclang_rpl_code_assignment assignment;
    uint32_t p, p_origin, p_adjustment;
};
/* Retain the exact constant-producing instruction and P provenance. Return one
 * nonzero relation token per assigned digit (relative to destination_first).
 * Tokens identify producer facts, never pretend to be physical constant cells.
 * No callback preserves legacy unknown origins. Errors leave step outputs intact;
 * discard callback facts after any failure. Readers/state must remain immutable. */
typedef int (*hpclang_rpl_code_constant_fn)(void *context,
                                            const struct hpclang_rpl_code_constant *constant,
                                            uint32_t tokens[16]);
struct hpclang_rpl_code_sum
{
    uint64_t left_value, right_value, result;
    uint32_t instruction, length, left, right, destination, first, nibbles, decimal, carry, p;
};
/* Inspect C0..CB address-field and Af0..AfB selected-field sums (f=0..7).
 * Both operands are retained; only the selected destination digits change.
 * Canonical BCD is required in decimal mode. Carry is normalized logical truth.
 * This pure inspector neither executes nor supplies provenance; errors leave
 * output unchanged. Execution integration and source repair are separate. */
int hpclang_rpl_code_sum_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                 uint32_t limit, const struct hpclang_rpl_code_values *values,
                                 struct hpclang_rpl_code_sum *sum);

struct hpclang_rpl_code_unit
{
    uint64_t before, after;
    uint32_t instruction, length, reg, first, nibbles, decrement, decimal, carry;
    uint32_t p, p_origin, p_adjustment, origins[16], adjustment;
};
struct hpclang_rpl_code_logical
{
    uint64_t left_value, right_value, result;
    uint32_t instruction, length, left, right, first, nibbles, disjunction, p;
};
/* 0Efr: AND/OR selected fields (f=0..7 or address field F). The destination
 * is left. Carry, decimal mode and unselected digits are unchanged. Errors
 * leave output untouched. This inspector supplies no relocation authority. */
int hpclang_rpl_code_logical_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                     uint32_t limit, const struct hpclang_rpl_code_values *values,
                                     struct hpclang_rpl_code_logical *logical);
struct hpclang_rpl_code_logical_source
{
    struct hpclang_rpl_code_logical logical;
    uint32_t p_origin, p_adjustment, origins[2][16], adjustment[2];
};
struct hpclang_rpl_code_bitshift
{
    uint64_t before, after;
    uint32_t instruction, length, reg, first, nibbles, p, lost_bit;
};
/* Inspect 81C..81F whole-register and 819fr selected-field one-bit right
 * shifts (f=0..7/F, r=0..3). Zero-fill only the field's high bit; carry and
 * decimal mode are unchanged. Execution sets SB when lost_bit is one.
 * Errors leave output intact; provenance and movement approval are separate. */
int hpclang_rpl_code_bitshift_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                      uint32_t limit, const struct hpclang_rpl_code_values *values,
                                      struct hpclang_rpl_code_bitshift *shift);
struct hpclang_rpl_code_bitshift_source
{
    struct hpclang_rpl_code_bitshift shift;
    uint32_t p_origin, p_adjustment, origins[16], adjustment;
};
/* Each result digit may depend on bits from two input digits. Return nonzero
 * producer tokens indexed by absolute register digit for the selected field.
 * If a nonzero low-five adjustment is invalidated (first < 5), also supply
 * tokens for all five low digits, including preserved digits outside the field.
 * Those digits retain their numerical values but need the adjusted input's
 * complete carry dependencies. No observer leaves affected origins unknown.
 * As with other observers, discard scratch facts after any step error. */
typedef int (*hpclang_rpl_code_bitshift_fn)(void *context,
                                            const struct hpclang_rpl_code_bitshift_source *fact,
                                            uint32_t tokens[16]);
/* Retain both operands and field-selection provenance. Supply nonzero producer
 * tokens in selected-digit order; they are relations, not physical cells.
 * Missing observer leaves computed origins unknown. Errors preserve step
 * outputs; discard callback scratch facts on any later error. */
typedef int (*hpclang_rpl_code_logical_fn)(void *context,
                                           const struct hpclang_rpl_code_logical_source *fact,
                                           uint32_t tokens[16]);
/* Inspect 0C/0D binary P increment/decrement (wrap modulo 16, carry on
 * overflow/underflow), AfC..AfF decrements and Bf4..Bf7 increments, f=0..7, and binary
 * 818frk additions/subtractions of k+1 (r=0..3/8..11, f=0..7 or address field F).
 * The immediate form ignores decimal mode. Other field extensions are unsupported.
 * Selected BCD
 * digits must be canonical. carry is normalized truth (native ROM may encode
 * a true carry as a nonzero mask); the numerical result preserves other digits.
 * Origin fields are zero here; the execution observer fills captured provenance.
 * No execution or mutation; errors leave output unchanged. */
int hpclang_rpl_code_unit_inspect(const struct hpclang_rpl_reader *reader, uint32_t instruction,
                                  uint32_t limit, const struct hpclang_rpl_code_values *values,
                                  struct hpclang_rpl_code_unit *unit);
typedef int (*hpclang_rpl_code_unit_fn)(void *context, const struct hpclang_rpl_code_unit *unit,
                                        uint32_t tokens[16]);

struct hpclang_rpl_code_sum_source
{
    struct hpclang_rpl_code_sum sum;
    uint32_t p_origin, p_adjustment, origins[2][16], adjustment[2];
};
typedef int (*hpclang_rpl_code_sum_fn)(void *context,
                                       const struct hpclang_rpl_code_sum_source *fact,
                                       uint32_t tokens[16]);

struct hpclang_rpl_code_native_word
{
    uint32_t instruction, reg, before, after, origins[8], adjustment;
    int32_t delta;
};
/* 81BD/81BE use binary 32-bit arithmetic even in Saturn decimal mode. Retain
 * the complete input word, including its low-address adjustment, and return
 * producer tokens for the high three B digits or all eight D digits. B's low
 * five digits keep their existing affine relation. Missing callback preserves
 * unknown computed origins. Qualify carry/borrow under any proposed relocation;
 * a zero observed high digit is not proof that it is a constant. This callback
 * only records facts in scratch. Discard all facts on any later error. Zero
 * accepts; negative propagates; positive and missing required tokens reject. */
typedef int (*hpclang_rpl_code_native_word_fn)(void *context,
                                               const struct hpclang_rpl_code_native_word *word,
                                               uint32_t tokens[8]);

struct hpclang_rpl_code_push_origins
{
    uint32_t a[5], d[8], p, target[5];
};
/* Qualify the 80B40 native service/profile and retain the fixed A/P producers,
 * full D.low32 decrement (including incoming adjustment), and original D.A
 * zero/nonzero decision. Supply nonzero relation tokens for A's five low
 * digits, all eight D digits and P; on the no-space path also supply the five
 * fixed error-target tokens. These are producer facts, not Saturn ROM cells.
 * The callback must constrain relocation to preserve the branch and native
 * constants. It records scratch facts only; discard them after any later
 * error. A missing observer rejects execution. Zero accepts, negative errors
 * propagate, positive errors and missing required tokens are malformed. */
typedef int (*hpclang_rpl_code_native_push_fn)(void *context,
                                               const struct hpclang_rpl_code_native_push *effect,
                                               const struct hpclang_rpl_code_execution *before,
                                               struct hpclang_rpl_code_push_origins *origins);

struct hpclang_rpl_code_callbacks
{
    hpclang_rpl_code_derive_fn derive;
    hpclang_rpl_code_store_fn store;
    hpclang_rpl_code_config_fn config;
    hpclang_rpl_code_predicate_fn predicate;
    hpclang_rpl_code_constant_fn constant;
    hpclang_rpl_code_unit_fn unit;
    hpclang_rpl_code_sum_fn sum;
    /* Separate from CONFIG: caller qualifies cancellation and any map rebuild.
     * A missing callback rejects UNCONFIG without changing execution state. */
    hpclang_rpl_code_config_fn unconfig;
    hpclang_rpl_code_scan_fn native_scan;
    uint32_t native_scan_max_reads; /* Explicit bound; zero rejects the scan. */
    hpclang_rpl_code_native_word_fn native_word;
    hpclang_rpl_code_logical_fn logical;
    hpclang_rpl_code_bitshift_fn bitshift;
    hpclang_rpl_code_native_push_fn native_push;
};
/* 86n/87n test software status bit n against zero/one, update carry and execute
 * GOYES/RTNYES. They consume the supplied status, without a register-predicate
 * callback. Callers must guard its initial snapshot and qualify every preceding
 * status writer; this does not establish status provenance or movement authority.
 * Taken returns retain the consumed stack source/generated metadata. */
/* Qualified 80B40 applies its prefix, then either emits the fixed error target
 * or stores A.A at D1-5 before the same two dependent reads as 81B1. The store
 * and reads use caller-owned scratch, including physical aliases; no native
 * writes are authorized. D1 retains its source with adjustment -5. The normal
 * path requires a store observer and canonical D0 whose increment fits.
 * P's native cached fields remain the caller's responsibility. */
/* Unified optional observation callbacks. Missing callbacks retain the legacy
 * rejection/unknown-source behavior; all prior error/discard contracts apply. */
int hpclang_rpl_code_execution_step_observed(const struct hpclang_rpl_reader *instructions,
                                             const struct hpclang_rpl_reader *data, uint32_t limit,
                                             struct hpclang_rpl_code_execution *state,
                                             hpclang_rpl_code_token_fn token,
                                             const struct hpclang_rpl_code_callbacks *callbacks,
                                             void *context,
                                             struct hpclang_rpl_code_step_uses *uses);

/* 810..817 circular nibble shifts follow the qualified HP49g+/HP50g native ROM:
 * digit origins are permuted and HST is unchanged (hardware Saturn documents
 * a right-wrap SB update instead). Nonzero adjustments cannot survive rotation:
 * their five affected origins become unknown before permutation. Carry and
 * other status bits are preserved; this supplies no new source authority. */
/* One C step combining the data/control evaluators with register/indirect PC
 * transfers, PC exchanges, C/RSTK transfers and qualified HP49g+/HP50g 81B1/81BD/81BE.
 * 81BE with carry clear stores D0.A at B.A, increments B.low32 by five,
 * sets D0=(A.A+5) modulo 20 bits, then executes the 81B1 load sequence.
 * Its store callback must supply subsequent reads/tokens from scratch; a later
 * read failure requires discarding that speculative store. With carry set it
 * jumps to native-ROM target 0x2e15 without a store. That target is emitted
 * with unknown origins: planning requires separate ROM-profile qualification,
 * never a fabricated Saturn operand. Native ROM profile qualification remains
 * the caller's responsibility for all three instructions.
 * 82x clears the selected low-four HST bits, preserving all other state.
 * 81B1 emits D0 and its loaded pointer as load addresses, then the loaded PC
 * target. It replaces A's low five digits and sources, retaining its upper bits;
 * D0 retains its original sources with an additional offset of five. Both
 * instructions leave carry/decimal mode unchanged and require canonical D0.
 * 81BD emits three dependent load addresses followed by the memory PC target;
 * B retains its low-five-digit source plus a modular adjustment; D0 retains
 * the first loaded pointer's source plus five. Arithmetic on D clears affected
 * origins. Full low-address copies/exchanges retain
 * adjustments; partial overwrites discard the affected expression. Its B/D updates use
 * the low 32-bit ARM words, independent of Saturn decimal mode. A D0 result
 * beyond 20 bits is rejected. values.carry and control.carry
 * describe the same state and must agree. Emit the five source tokens of every
 * consumed load address, register jump target, indirect-memory target or return.
 * Copies alone emit no use. Direct encoded operands are described by branch,
 * not misrepresented as copied address digits. Generated returns retain a call
 * producer separately; other computed PC origins stay unknown. C/RSTK transfers
 * conservatively discard generated-call metadata instead of mislabeling it.
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

/* Same evaluator, with optional caller-owned arithmetic result records. */
int hpclang_rpl_code_execution_step_derived(const struct hpclang_rpl_reader *instructions,
                                            const struct hpclang_rpl_reader *data, uint32_t limit,
                                            struct hpclang_rpl_code_execution *state,
                                            hpclang_rpl_code_token_fn token,
                                            hpclang_rpl_code_derive_fn derive, void *context,
                                            struct hpclang_rpl_code_step_uses *uses);

/* As execution_step_derived, with optional simulated memory-store effects.
 * Without store, stores still reject. A store's entire span must fit the
 * 20-bit address space; callback rejection preserves state and uses. Readers
 * must expose the caller's current simulated view, including physical aliases
 * and instruction fetches affected by earlier stores. Other native/mapping/MMIO
 * effects remain unsupported and must never be treated as ordinary RAM writes. */
int hpclang_rpl_code_execution_step_effects(const struct hpclang_rpl_reader *instructions,
                                            const struct hpclang_rpl_reader *data, uint32_t limit,
                                            struct hpclang_rpl_code_execution *state,
                                            hpclang_rpl_code_token_fn token,
                                            hpclang_rpl_code_derive_fn derive,
                                            hpclang_rpl_code_store_fn store, void *context,
                                            struct hpclang_rpl_code_step_uses *uses);

/* Extended step with caller-owned CONFIG simulation. Without config, 805 remains
 * unsupported. On success only PC advances in execution state; native effects
 * reside in the callback-owned snapshot. Other entry points retain their ABI. */
int hpclang_rpl_code_execution_step_native(
    const struct hpclang_rpl_reader *instructions, const struct hpclang_rpl_reader *data,
    uint32_t limit, struct hpclang_rpl_code_execution *state, hpclang_rpl_code_token_fn token,
    hpclang_rpl_code_derive_fn derive, hpclang_rpl_code_store_fn store,
    hpclang_rpl_code_config_fn config, void *context, struct hpclang_rpl_code_step_uses *uses);

/* Native step with explicit opcode-9/8A/8B predicate qualification. The legacy entry
 * points still reject those register predicates. Accepted comparisons update carry and execute
 * GOYES/RTNYES, including taken returns with source/generated metadata retained. */
int hpclang_rpl_code_execution_step_predicates(
    const struct hpclang_rpl_reader *instructions, const struct hpclang_rpl_reader *data,
    uint32_t limit, struct hpclang_rpl_code_execution *state, hpclang_rpl_code_token_fn token,
    hpclang_rpl_code_derive_fn derive, hpclang_rpl_code_store_fn store,
    hpclang_rpl_code_config_fn config, hpclang_rpl_code_predicate_fn predicate, void *context,
    struct hpclang_rpl_code_step_uses *uses);

/* Validate a collected generated return against its guarded producing call,
 * and plan that call's branch operand for a caller-qualified move. The return
 * replacement must equal the relocated call's fallthrough, including at an
 * exclusive move endpoint or 20-bit wrap. No stored return field is repaired.
 * mappings.field names the producing instruction/return mapping; mappings.target
 * names the call destination, independently qualified. Retain original call
 * bytes as guards and establish that its complete instruction survives the move.
 * Return 1 with an operand patch, 0 without one, or a negative span error.
 * Zero is only this generated-return relation, not complete path coverage or
 * mutation permission. Output changes only on 1; all reads must remain stable.
 * Non-generated, mixed-token or adjusted records reject; unknown is not generated. */
int hpclang_rpl_code_generated_return_plan(const struct hpclang_rpl_reader *instructions,
                                           const struct hpclang_rpl_code_address_use *use,
                                           const struct hpclang_rpl_reference_mappings *mappings,
                                           uint32_t first, uint32_t end, int32_t delta,
                                           uint32_t replacement,
                                           struct hpclang_rpl_code_patch *patch);

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

/* D0/D1 immediate arithmetic retains the original low-five source tokens and
 * accumulates a modulo-20-bit adjustment. Other slots are preserved. Validate
 * a nonzero immediate in [-16,16] and canonical incoming adjustment before
 * any change. Carry is not represented here: callers separately qualify all
 * carry-dependent effects of a proposed source repair. This relation alone
 * does not qualify the resulting address or authorize source mutation. */
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
