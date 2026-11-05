#ifndef HPCLANG_RPL_STOCK_ROM_H
#define HPCLANG_RPL_STOCK_ROM_H

#include <stdint.h>
#include "rpl-rom-profile.h"

struct hpclang_rpl_stock_rom
{
    const volatile uint32_t *sections;
    uint32_t expected[2];
};

/* Qualify the two user-readable identity sections spanning immutable Flash.
 * Keep IRQ/FIQ masked and prohibit Stock calls/mapping or Flash changes for the
 * capture/read/profile-use lifetime. No mapping changes or Flash commands.
 * Capture publishes only on zero; reader checks both descriptors before each
 * block. This detects stale descriptors, not concurrent changes within a block. */
int hpclang_rpl_stock_rom_capture(struct hpclang_rpl_stock_rom *rom,
                                  const volatile uint32_t sections[2]);
int hpclang_rpl_stock_rom_reader(struct hpclang_rpl_stock_rom *rom);
/* Matches hpclang_rpl_rom_read_fn. Reads only [0,0x140000), including physical
 * address zero via assembly halfword loads. Invalid ranges/descriptors do not
 * touch Flash or output. Caller supplies a live captured context. */
int hpclang_rpl_stock_rom_read(void *context, uint32_t offset, uint8_t *bytes, uint32_t length);
#endif
