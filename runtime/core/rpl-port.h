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
