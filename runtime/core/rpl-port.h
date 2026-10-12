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

#endif
