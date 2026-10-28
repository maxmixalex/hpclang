#ifndef HPCLANG_RPL_BANK_EFFECTS_H
#define HPCLANG_RPL_BANK_EFFECTS_H
#include "rpl-stock-memory.h"
#include "rpl-port.h"

struct hpclang_rpl_bank_effects
{
    struct hpclang_rpl_stock_bank_map map;
    uint32_t sentinel;    /* Native byte offset 0x400. */
    uint32_t writes[256]; /* Native byte offsets 0x404..0x803. */
    uint32_t cache[4];    /* Saved read/read/write/write words at 0xc4c..0xc58. */
};

/* Predict the reviewed populated-bank handler's mapping/cache state without
 * executing it. Inputs are separately qualified, stable snapshots; retain all
 * source dependencies for later guards. The existing map validator qualifies
 * read pages, geometry and overlay bounds. Write/cache words are opaque values
 * copied or replaced, never dereferenced or certified as writable addresses.
 * Equal selectors preserve all state. Otherwise publish Read[256] before
 * conditional overlay repair, preserving caches when their read page matches.
 * Zero succeeds; negative span errors preserve output. Output may equal input.
 * No allocation, native memory access or writes. This is not a complete CPU,
 * routing/interrupt model, execution-edge proof or mutation authorization. */
int hpclang_rpl_bank_effects_select(const struct hpclang_rpl_bank_effects *state,
                                    const struct hpclang_rpl_rom_bank banks[16], uint32_t lower,
                                    uint32_t upper, struct hpclang_rpl_bank_effects *selected);
#endif
