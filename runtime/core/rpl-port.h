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
int hpclang_rpl_bank_lookup(const struct hpclang_rpl_reader *reader, uint32_t flash_table,
                            uint32_t eram_entry, uint32_t bank_switch,
                            struct hpclang_rpl_bank_binding *binding);

struct hpclang_rpl_banked_target
{
    struct hpclang_rpl_bank_binding bank;
    uint32_t offset;
};

struct hpclang_rpl_flashptr
{
    uint32_t bank, index;
};

/* Decode DOFLASHP's two indices without assuming a mapping or table address.
 * Returns 1 on success; negative errors preserve output. */
int hpclang_rpl_flashptr_decode(const struct hpclang_rpl_reader *source, uint32_t address,
                                uint32_t limit, struct hpclang_rpl_flashptr *pointer);

/* Look up one index in a caller-mapped native ROM bank header. Validate the
 * 0F bank identity and declared table count within the 0x40000..0x80000 window.
 * The caller validates physical mapping and supplies the header location.
 * Returns 1 found, 0 absent, negative malformed/unavailable/unsupported; output
 * changes only on success. Targets below 0x40000 are fixed-ROM addresses;
 * others retain this bank's identity. Never treats a user F0 allocation chain
 * as a ROM pointer table, dereferences the result, or executes native code. */
int hpclang_rpl_flashptr_lookup(const struct hpclang_rpl_reader *mapped, uint32_t header,
                                const struct hpclang_rpl_flashptr *pointer, uint32_t *target);

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

/* A read-only visitor: limit excludes the library CRC. Nonzero return aborts
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
