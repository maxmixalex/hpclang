#include "rpl-stock-flash-data.h"

uint16_t hpclang_flash_read16(void *context, uint32_t address);

int hpclang_rpl_stock_flash_data_read(void *context, uint32_t address, uint8_t *value)
{
    const struct hpclang_rpl_stock_view *view = context;
    if (!view || !value || address < 0x280000u || address >= 0x400000u || !view->ram_section ||
        *view->ram_section != view->expected_ram || !view->rom.sections)
        return 0;
    for (unsigned i = 0; i < 2; ++i)
        if (view->rom.sections[i] != view->rom.expected[i])
            return 0;
    uint16_t word = hpclang_flash_read16(0, (address / 2u) & ~1u);
    *value = (word >> (4u * (address & 3u))) & 15u;
    return 1;
}
