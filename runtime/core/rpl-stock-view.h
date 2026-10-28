#ifndef HPCLANG_RPL_STOCK_VIEW_H
#define HPCLANG_RPL_STOCK_VIEW_H
#include "rpl-stock-rom.h"

struct hpclang_rpl_stock_view
{
    const volatile uint8_t *sram;
    const volatile uint32_t *ram_section;
    uint32_t expected_ram, state_offset;
    struct hpclang_rpl_stock_rom rom;
};
/* Read-only planning view of qualified HP49g+/50g native state, SRAM and the
 * immutable ROM prefix. sram supplies 512 KiB representing physical 0x08000000;
 * state_address is a physical address with a complete 0xc60-byte state extent.
 * Capture checks identity sections and publishes only on zero. The Stock entry
 * takes these inputs from the launcher. Neither entry proves interpreter/ROM
 * identity: identify ROM separately and validate state provenance.
 * Keep IRQ/FIQ masked and prohibit Stock calls, mapping/Flash/state changes
 * throughout capture/read/use. Descriptor checks detect stale mappings, not
 * concurrent changes. No allocation, bank switching or writes to native state. */
int hpclang_rpl_stock_view_capture(struct hpclang_rpl_stock_view *view,
                                   const volatile uint8_t *sram, uint32_t state_address,
                                   const volatile uint32_t *ram_section,
                                   const volatile uint32_t flash_sections[2]);
int hpclang_rpl_stock_view_reader(struct hpclang_rpl_stock_view *view);
/* rpl_reader callbacks: native coordinates are state-relative nibbles; rom
 * coordinates are physical nibbles; saturn uses the current native Read[] map.
 * The Saturn view admits only aligned 2-KiB pages wholly inside SRAM or immutable
 * ROM, never user Flash/MMIO. It is a planning source, not a general bus reader.
 * Success is 1; any failure is 0 with output untouched. */
int hpclang_rpl_stock_view_native(void *context, uint32_t address, uint8_t *value);
int hpclang_rpl_stock_view_rom(void *context, uint32_t address, uint8_t *value);
int hpclang_rpl_stock_view_saturn(void *context, uint32_t address, uint8_t *value);
#endif
