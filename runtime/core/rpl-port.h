#ifndef HPCLANG_RUNTIME_RPL_PORT_H
#define HPCLANG_RUNTIME_RPL_PORT_H

#include "rpl-object-span.h"

struct hpclang_rpl_port_cursor
{
    uint32_t address;
    uint32_t end;
    int finished;
};

struct hpclang_rpl_port_entry
{
    uint32_t address;
    uint32_t span;
    uint32_t prologue;
    uint32_t value_address;
    uint32_t value_span;
    uint32_t library_number;
    uint32_t name_length;
    uint8_t name[256];
};

/* Walk a bounded native RAM port, ending at its zero prologue. Port-specific
 * mapping and ownership are caller responsibilities. Flash record allocation
 * is not a RAM-port chain. Returns 1 for an entry, 0 at end, or a negative span
 * result. Cursor and output are unchanged on error. No native references escape
 * the current invocation; callers copy values into owned snapshots separately. */
int hpclang_rpl_port_next(const struct hpclang_rpl_reader *reader,
                          struct hpclang_rpl_port_cursor *cursor,
                          struct hpclang_rpl_port_entry *entry);

/* Find the RAM-port record containing an interior target, including its header
 * and CRC. Validate the complete chain through its zero terminator before
 * publishing a match. The terminator and free tail are not objects. Mapping
 * and bank identity belong to the caller and must remain stable. Returns 1
 * found, 0 absent or a negative span result; output changes only on 1. */
int hpclang_rpl_port_find(const struct hpclang_rpl_reader *reader, uint32_t first, uint32_t end,
                          uint32_t target, struct hpclang_rpl_port_entry *entry);

enum hpclang_rpl_library_reference_kind
{
    HPCLANG_RPL_LIBRARY_HASH,
    HPCLANG_RPL_LIBRARY_MESSAGES,
    HPCLANG_RPL_LIBRARY_LINKS,
    HPCLANG_RPL_LIBRARY_CONFIG,
    HPCLANG_RPL_LIBRARY_FUNCTION
};

struct hpclang_rpl_library_binding
{
    uint32_t address;
    uint32_t bank_switch;
    uint32_t flags;
};

/* Semantic identity for a moved library's native catalog binding. Addresses
 * belong to the mapping selected by bank_switch, not saved RAM field addresses.
 * Caller validates library identity, mapping and transaction freshness. */
struct hpclang_rpl_catalog_rebind
{
    uint32_t number, bank_switch, previous, replacement;
};

/* Resolve the current RAM field for one authorized library relocation. Scan
 * the entire supplied catalog, require a unique matching ID, exact bank switch
 * and binding nibbles equal to their previous or replacement values, permitting
 * interrupted atomic-nibble RAM writes and already repaired bindings. Returns
 * 1 with field, 0 absent, or a negative span result; field changes only on 1.
 * The caller can construct a RAM edit with this fresh field and previous/value.
 * No writes occur. Absent bindings must be handled explicitly by the transaction.
 * Keep the current catalog stable through commit and re-resolve after rebuilding
 * it. This neither discovers the catalog nor proves complete roots, surviving
 * RAM ownership or a safe startup point for Flash recovery. */
int hpclang_rpl_catalog_rebind_field(const struct hpclang_rpl_reader *reader, uint32_t address,
                                     uint32_t limit,
                                     const struct hpclang_rpl_catalog_rebind *rebind,
                                     uint32_t *field);

struct hpclang_rpl_catalog_insertion
{
    uint32_t slot; /* Insert before this record, or at end. */
    uint32_t end;  /* Exclusive end of the existing table. */
    uint32_t count;
};

/* Plan insertion into the native strictly ID-ordered catalog. Validate every
 * existing record before returning 1 with a plan, or 0 for an existing ID.
 * Negative span results indicate invalid/unreadable input. Output changes only
 * on 1. limit bounds the readable existing table, not free space after it.
 * The caller must separately reserve 16 nibbles, relocate following memory and
 * its references, and preserve the mapping through commit. This never writes
 * or authorizes relocation, registration, attachment or library execution. */
int hpclang_rpl_catalog_plan_insert(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t limit, uint32_t number,
                                    struct hpclang_rpl_catalog_insertion *plan);

struct hpclang_rpl_catalog_removal
{
    uint32_t slot;
    uint32_t end;
    uint32_t count;
    struct hpclang_rpl_library_binding binding;
};

/* Inspect the native HOME attachment prefix (DORRP, count, 13-nibble records).
 * Caller supplies the proven HOME address and readable extent. Validate every
 * encoded record, including records after a match. Return 1 attached, 0 absent
 * or a negative span result. Absence is not proof of execution-reference
 * liveness or validation of the directory's stored objects. No writes. */
int hpclang_rpl_home_library_attached(const struct hpclang_rpl_reader *reader, uint32_t home,
                                      uint32_t limit, uint32_t number);

/* Plan removal from the strictly ID-ordered catalog. Validate the entire table,
 * including entries after the match, before publishing its slot and binding.
 * Returns 1 found, 0 absent, or a negative span result; output changes only on
 * 1. This read-only plan does not prove that the library is detached or unused.
 * The caller must prove those conditions, revalidate the plan under stable
 * ownership, repair references and compact memory before publishing a removal.
 * The binding retains its native bank coordinates; no payload is dereferenced. */
int hpclang_rpl_catalog_plan_remove(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t limit, uint32_t number,
                                    struct hpclang_rpl_catalog_removal *plan);

/* Enumerate every catalog entry, including banked and duplicate-ID records.
 * The record address identifies the catalog cell, not the library payload.
 * The binding is callback-local; copy it if needed. No target is dereferenced.
 * Keep the mapping stable and the callback read-only; discard partial results
 * on any error. A nonzero callback result stops traversal unchanged. */
typedef int (*hpclang_rpl_catalog_visit_fn)(void *context, uint32_t record, uint32_t number,
                                            const struct hpclang_rpl_library_binding *binding);
int hpclang_rpl_catalog_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t limit, hpclang_rpl_catalog_visit_fn visit, void *context);

/* Resolve a library ID in a bounded classic Stock catalog (3-nibble count,
 * 16-nibble records). Preserve the bank-switch identity: address alone is not
 * a dereferenceable object pointer. Mapping the catalog and resolving the bank
 * are caller responsibilities. Returns 1 found, 0 absent or a negative span
 * result. Duplicate matching IDs are malformed; output is unchanged on failure
 * or absence. The reader must remain stable throughout the call. */
int hpclang_rpl_library_lookup(const struct hpclang_rpl_reader *reader, uint32_t address,
                               uint32_t limit, uint32_t number,
                               struct hpclang_rpl_library_binding *binding);

/* Resolve one command through a native library ID header's relative link table.
 * The caller supplies the already selected, stable bank mapping and its half-open
 * bounds. This does not switch banks, execute code or prove target safety.
 * Returns 1 for a target, 0 for a missing table/entry/index, negative on malformed
 * or unreadable metadata. Output remains unchanged except on success. */
int hpclang_rpl_library_command(const struct hpclang_rpl_reader *reader, uint32_t first,
                                uint32_t end, uint32_t header, uint32_t number, uint32_t command,
                                uint32_t *target);

struct hpclang_rpl_library_attachment_targets
{
    uint32_t present; /* Bits 0/1 distinguish present hash/messages, even at address zero. */
    uint32_t hash;
    uint32_t messages;
};

/* Resolve hash/message table coordinates from an identity-checked library ID
 * header in a caller-selected stable mapping. first/end bound that mapping;
 * targets are not dereferenced or validated as executable/table bodies.
 * Return 1 after reading both offsets, negative on malformed/unreadable input;
 * output changes only on 1. Zero relative offsets mean absent fields.
 * These are bank-local coordinates, NOT ready-to-publish HOME fields: banked
 * libraries require separately owned DOACPTR wrappers carrying the bank switch.
 * A direct-mapped table may itself be a DOACPTR; do not unwrap it implicitly.
 * This neither switches banks nor authorizes registration/attachment. */
int hpclang_rpl_library_attachment_resolve(const struct hpclang_rpl_reader *reader, uint32_t first,
                                           uint32_t end, uint32_t header, uint32_t number,
                                           struct hpclang_rpl_library_attachment_targets *targets);

/* Resolve a library's configuration entry through its ID header. As with
 * command lookup, the caller supplies a stable selected mapping and bounds.
 * Zero offset means no callback. The result is a bank-local address, not proof
 * of callback safety, execution order or permission to run it. Do not infer
 * absence from an unreadable header. Returns 1 found, 0 absent, or a negative
 * span result; target changes only on 1. The callback is not dereferenced. */
int hpclang_rpl_library_configuration(const struct hpclang_rpl_reader *reader, uint32_t first,
                                      uint32_t end, uint32_t header, uint32_t number,
                                      uint32_t *target);

/* Match a switch address against a caller-validated ROM table containing
 * 16 seven-nibble Flash entries and a separate ERAM entry. Addresses are read,
 * never inferred from an arithmetic sequence of implementation routines.
 * Returns 1 found, 0 unknown or a negative span result; output is unchanged on
 * absence/error. A returned index still requires physical mapping validation. */
enum hpclang_rpl_bank_kind
{
    HPCLANG_RPL_BANK_FLASH,
    HPCLANG_RPL_BANK_ERAM
};
struct hpclang_rpl_bank_binding
{
    enum hpclang_rpl_bank_kind kind;
    uint32_t index;
};

struct hpclang_rpl_rom_bank
{
    uint32_t physical, bytes;
};

/* Validate a captured ROM size directory against all 16 captured native bank
 * bases. The count KiB-size bytes immediately precede the first bank; their
 * address and the Flash size are supplied by a separately verified firmware
 * layout. Do not discover that layout by searching for plausible data.
 * Every bank must fit a Saturn window, the complete bank sequence must end exactly
 * at flash_bytes, and unused native base cells must be zero. Inputs stay stable.
 * Returns 1 for a populated index, 0 for an unused index, negative span results
 * for inconsistent metadata. Output changes only on 1. This is read-only
 * geometry validation, not firmware identity or mutation authorization. */
int hpclang_rpl_rom_bank_layout(const uint8_t *kib_sizes, uint32_t count, uint32_t directory,
                                uint32_t flash_bytes, const uint32_t native_bases[16],
                                uint32_t index, struct hpclang_rpl_rom_bank *bank);

int hpclang_rpl_bank_lookup(const struct hpclang_rpl_reader *reader, uint32_t flash_table,
                            uint32_t eram_entry, uint32_t bank_switch,
                            struct hpclang_rpl_bank_binding *binding);

/* Record every valid nibble read from the switch table, including nonmatching
 * entries needed to rule out an ambiguous match. The caller binds addresses to
 * the reader's proven mapping and retains them as commit guards. The callback
 * returns zero or a negative error, propagated unchanged; positive returns are
 * MALFORMED, never a successful lookup. Discard partial dependencies on error. Output changes
 * only on a complete successful match. No writes or allocation. This observes
 * lookup dependencies, not switch-code identity, populated-bank geometry or all
 * live references. Keep reader, table and callback state stable throughout. */
typedef int (*hpclang_rpl_bank_dependency_fn)(void *context, uint32_t address, uint8_t value);
/* Configuration lookup with dependencies for the library ID and relative
 * configuration pointer, including a zero (absent) pointer. Same stable selected
 * mapping contract as library_configuration; retain mapping/catalog guards too.
 * Callback errors discard partial observations: negative propagates, positive
 * becomes MALFORMED. Target changes only on found (1), not absence or error.
 * This does not inspect the target or qualify startup order/execution safety. */
int hpclang_rpl_library_configuration_guarded(const struct hpclang_rpl_reader *reader,
                                              uint32_t first, uint32_t end, uint32_t header,
                                              uint32_t number, uint32_t *target,
                                              hpclang_rpl_bank_dependency_fn dependency,
                                              void *context);

/* Capture/validate all sixteen bank geometries from an independently qualified
 * layout. operands is the ROM BYTE offset of two little-endian ARM pointers:
 * count byte, then KiB-size directory. bases_address is the CPU-state BYTE offset
 * of sixteen 32-bit bases. Reader/dependency addresses are NIBBLE offsets in
 * their respective physical-ROM/native-state mappings; never conflate them.
 * Metadata must fit the first 512 KiB of its mapping (20-bit guard coordinates).
 * flash_bytes and operand/base provenance require independent qualification;
 * no scanning for plausible metadata. Record every pointer/count/size/base read.
 * Return zero with sixteen entries (unused banks zeroed), or a negative error
 * preserving all output. Discard partial dependencies on failure; positive
 * callback errors are MALFORMED. Caller holds mappings stable and validates
 * guards before use. Geometry validation does not identify firmware or authorize
 * reads, switches or mutation. banks points to sixteen output entries. */
int hpclang_rpl_rom_bank_capture_guarded(
    const struct hpclang_rpl_reader *rom, const struct hpclang_rpl_reader *state, uint32_t operands,
    uint32_t bases_address, uint32_t flash_bytes, struct hpclang_rpl_rom_bank *banks,
    hpclang_rpl_bank_dependency_fn rom_dependency, void *rom_context,
    hpclang_rpl_bank_dependency_fn state_dependency, void *state_context);

int hpclang_rpl_bank_lookup_guarded(const struct hpclang_rpl_reader *reader, uint32_t flash_table,
                                    uint32_t eram_entry, uint32_t bank_switch,
                                    struct hpclang_rpl_bank_binding *binding,
                                    hpclang_rpl_bank_dependency_fn dependency, void *context);

struct hpclang_rpl_banked_target
{
    struct hpclang_rpl_bank_binding bank;
    uint32_t offset;
};

struct hpclang_rpl_bank_identity_range
{
    uint32_t address, length;
    const uint8_t *nibbles;
};

/* Match a separately reviewed switch alias to a canonical table entry. Expected
 * ranges must be independent immutable evidence, not a copy of the current
 * reader. They must include both entry addresses and all code/data needed by
 * the caller's equivalence proof, including mutable selector data when the
 * handler reads it. Equal entry code alone does not prove a fixed bank index.
 * This routine checks bytes, not completeness
 * of that proof or machine-code semantics. Calling-context assumptions, page
 * owners, geometry, firmware identity and executable references remain caller
 * obligations. No fixed ROM addresses or alias arithmetic are inferred.
 * Returns 1 for a validated alias and canonical binding, 0 for a different
 * requested switch/absent canonical entry, negative on mismatch/error. Output
 * changes only on 1; discard partial dependencies on any other result. Positive
 * dependency errors become MALFORMED. All inputs/read mappings stay stable. */
int hpclang_rpl_bank_alias_lookup_guarded(const struct hpclang_rpl_reader *reader,
                                          uint32_t flash_table, uint32_t eram_entry,
                                          uint32_t requested_switch, uint32_t alias,
                                          uint32_t canonical,
                                          const struct hpclang_rpl_bank_identity_range *ranges,
                                          uint32_t range_count,
                                          struct hpclang_rpl_bank_binding *binding,
                                          hpclang_rpl_bank_dependency_fn dependency, void *context);

struct hpclang_rpl_flashptr
{
    uint32_t selector, index;
};

/* Decode DOFLASHP's two indices without assuming a mapping or table address.
 * Returns 1 on success; negative errors preserve output. */
int hpclang_rpl_flashptr_decode(const struct hpclang_rpl_reader *source, uint32_t address,
                                uint32_t limit, struct hpclang_rpl_flashptr *pointer);

/* Observe all 12 source nibbles without resolving selector/index or reading a
 * target. Same dependency/error contract as bank_lookup_guarded; preserve output
 * on failure. Keep the source mapping identity with these dependencies. */
int hpclang_rpl_flashptr_decode_guarded(const struct hpclang_rpl_reader *source, uint32_t address,
                                        uint32_t limit, struct hpclang_rpl_flashptr *pointer,
                                        hpclang_rpl_bank_dependency_fn dependency, void *context);

/* Match a logical FlashPtr selector's live configuration against a separate
 * physical-bank configuration table. Both tables contain 16 five-nibble cells
 * at caller-validated RAM addresses. Zero/unknown configuration returns 0;
 * duplicate matches and invalid data fail. Never infer bank from selector or
 * configuration bit patterns. Keep the mapping and both tables stable for
 * the entire lookup. Output changes only on a unique match (1). */
int hpclang_rpl_flashptr_bank(const struct hpclang_rpl_reader *reader, uint32_t selectors,
                              uint32_t banks, uint32_t selector, uint32_t *bank);

/* Observe the selected configuration and all physical-bank configurations,
 * including nonmatches needed to exclude ambiguity. Same dependency/error
 * contract as bank_lookup_guarded; output changes only on a unique match.
 * Does not qualify the source FlashPtr, table mapping or selected bank geometry. */
int hpclang_rpl_flashptr_bank_guarded(const struct hpclang_rpl_reader *reader, uint32_t selectors,
                                      uint32_t banks, uint32_t selector, uint32_t *bank,
                                      hpclang_rpl_bank_dependency_fn dependency, void *context);

/* Look up one index in a caller-mapped native ROM bank header. Validate the
 * 0F bank identity and declared table count within the 0x40000..0x80000 window.
 * bank is the resolved physical-bank index, not the FlashPtr selector. The
 * caller validates physical mapping and supplies the header location.
 * Returns 1 found, 0 absent, negative malformed/unavailable/unsupported; output
 * changes only on success. Targets below 0x40000 are fixed-ROM addresses;
 * others retain this bank's identity. Never treats a user F0 allocation chain
 * as a ROM pointer table, dereferences the result, or executes native code. */
int hpclang_rpl_flashptr_lookup(const struct hpclang_rpl_reader *mapped, uint32_t header,
                                uint32_t bank, uint32_t index, uint32_t *target);

/* Retain identity, count and selected target nibbles as lookup dependencies.
 * Same callback/error contract as bank_lookup_guarded. This does not observe
 * the source FlashPtr or configuration tables, authenticate mapping/geometry,
 * or dereference the target. Keep those proofs and dependencies separately. */
int hpclang_rpl_flashptr_lookup_guarded(const struct hpclang_rpl_reader *mapped, uint32_t header,
                                        uint32_t bank, uint32_t index, uint32_t *target,
                                        hpclang_rpl_bank_dependency_fn dependency, void *context);

/* Resolve a count-prefixed table after the caller has validated its mapping
 * and format. first/end bound the readable native bank window; table points
 * at the five-nibble count followed by absolute five-nibble targets. This
 * primitive also supports special tables without an ordinary 0F header; it
 * must not be used to infer a table from arbitrary user allocations. Same
 * result/output contract as flashptr_lookup. Targets are not dereferenced. */
int hpclang_rpl_flashptr_table_lookup(const struct hpclang_rpl_reader *mapped, uint32_t first,
                                      uint32_t end, uint32_t table, uint32_t index,
                                      uint32_t *target);

/* Observe count and selected target for a separately proven special table.
 * Does not infer table identity, format or mapping from plausible bytes. Retain
 * those proofs and their dependencies separately. Same callback/error contract
 * as bank_lookup_guarded; absent/error results preserve output. */
int hpclang_rpl_flashptr_table_lookup_guarded(const struct hpclang_rpl_reader *mapped,
                                              uint32_t first, uint32_t end, uint32_t table,
                                              uint32_t index, uint32_t *target,
                                              hpclang_rpl_bank_dependency_fn dependency,
                                              void *context);

/* Resolve a bounded DOACPTR through caller-validated ROM switch tables. The
 * source and ROM may have distinct readers. Return a bank-qualified nibble
 * offset within the native 0x40000..0x80000 window, never a physical address.
 * Return 1 resolved, 0 unknown switch, negative on invalid/unreadable input;
 * output stays unchanged except on success. Does not dereference the target,
 * require a live allocation, or prove that reclamation/relocation is safe. */
int hpclang_rpl_acptr_resolve(const struct hpclang_rpl_reader *source, uint32_t address,
                              uint32_t limit, const struct hpclang_rpl_reader *rom,
                              uint32_t flash_table, uint32_t eram_entry,
                              struct hpclang_rpl_banked_target *target);

/* Resolve with independent dependency streams for the source object's 15
 * nibbles and every switch-table nibble. Addresses are local to each reader;
 * the caller must qualify their mappings, even when numeric addresses coincide.
 * Callbacks follow bank_lookup_guarded's error contract. On any error discard
 * both streams; target changes only on success. Does not observe target contents
 * or establish switch-code identity, bank geometry or complete live references.
 * Both mappings, readers and callback state must remain stable throughout. */
int hpclang_rpl_acptr_resolve_guarded(
    const struct hpclang_rpl_reader *source, uint32_t address, uint32_t limit,
    const struct hpclang_rpl_reader *rom, uint32_t flash_table, uint32_t eram_entry,
    struct hpclang_rpl_banked_target *target, hpclang_rpl_bank_dependency_fn source_dependency,
    void *source_context, hpclang_rpl_bank_dependency_fn rom_dependency, void *rom_context);

/* A read-only visitor: each callback's limit excludes the library CRC; the
 * input object limit must include it. Nonzero return aborts
 * traversal and is returned to the caller. The caller must keep the reader/mapping stable and must
 * discard partial traversal on error. This enumerates structural references;
 * it does not prove that referenced code has no further live references. */
typedef int (*hpclang_rpl_library_visit_fn)(void *context,
                                            enum hpclang_rpl_library_reference_kind kind,
                                            uint32_t field, uint32_t target, uint32_t limit);
int hpclang_rpl_library_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t limit, hpclang_rpl_library_visit_fn visit, void *context);

/* Traverse allocated slots within one Flash bank, starting after its bank
 * header. Each slot has a status byte, a five-nibble relative extent and one
 * metadata nibble before the object. The caller supplies bank mapping/bounds.
 * Deleted slots retain their extent and are skipped without reading payloads.
 * Unknown slot states are rejected until their native lifecycle is supported.
 * Cursor/output remain unchanged on error, as for RAM traversal. */
int hpclang_rpl_flash_next(const struct hpclang_rpl_reader *reader,
                           struct hpclang_rpl_port_cursor *cursor,
                           struct hpclang_rpl_port_entry *entry);

#endif
