#include "rpl-stock-rom.h"
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>

/* The assembly load permits physical zero without a C null dereference. */
uint16_t hpclang_flash_read16(void *context, uint32_t address);

int hpclang_rpl_stock_rom_capture(struct hpclang_rpl_stock_rom *rom,
                                  const volatile uint32_t sections[2])
{
    if (!rom || !sections)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    uint32_t expected[2] = {sections[0], sections[1]};
    for (unsigned i = 0; i < 2; ++i)
        if ((expected[i] & 0xfff00803u) != ((i << 20) | 0x802u))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    *rom = (struct hpclang_rpl_stock_rom){sections, {expected[0], expected[1]}};
    return 0;
}

int hpclang_rpl_stock_rom_reader(struct hpclang_rpl_stock_rom *rom)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!boot || !boot->saturn_state ||
        (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    return hpclang_rpl_stock_rom_capture(rom, (const volatile uint32_t *)(uintptr_t)0x07f00000u);
}

int hpclang_rpl_stock_rom_read(void *context, uint32_t offset, uint8_t *bytes, uint32_t length)
{
    const struct hpclang_rpl_stock_rom *rom = context;
    if (!rom || !rom->sections || !bytes)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (offset > 0x140000u || length > 0x140000u - offset)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (rom->sections[0] != rom->expected[0] || rom->sections[1] != rom->expected[1])
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    while (length) {
        uint16_t value = hpclang_flash_read16(0, offset & ~1u);
        *bytes++ = (uint8_t)(value >> (8u * (offset & 1u)));
        --length;
        if (!(offset & 1u) && length) {
            *bytes++ = (uint8_t)(value >> 8);
            --length;
            ++offset;
        }
        ++offset;
    }
    return 0;
}
