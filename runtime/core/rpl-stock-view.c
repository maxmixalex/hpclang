#include "rpl-stock-view.h"
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>

int hpclang_rpl_stock_view_capture(struct hpclang_rpl_stock_view *view,
                                   const volatile uint8_t *sram, uint32_t state_address,
                                   const volatile uint32_t *ram_section,
                                   const volatile uint32_t flash_sections[2])
{
    if (!view || !sram || !ram_section || !flash_sections)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if ((state_address & 3u) || state_address < 0x08000000u || state_address > 0x08080000u - 0xc60u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t descriptor = *ram_section;
    if ((descriptor & 0xfff00803u) != 0x08000802u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_stock_view next = {
        sram, ram_section, descriptor, state_address - 0x08000000u, {0}};
    int result = hpclang_rpl_stock_rom_capture(&next.rom, flash_sections);
    if (result)
        return result;
    *view = next;
    return 0;
}

int hpclang_rpl_stock_view_reader(struct hpclang_rpl_stock_view *view)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!boot || !boot->saturn_state ||
        (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const volatile uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    return hpclang_rpl_stock_view_capture(view, (const void *)(uintptr_t)0x08000000u,
                                          boot->saturn_state, l1 + 0x80, l1);
}

static int ram_live(const struct hpclang_rpl_stock_view *view)
{
    return view && view->sram && view->ram_section && *view->ram_section == view->expected_ram;
}

int hpclang_rpl_stock_view_native(void *context, uint32_t address, uint8_t *value)
{
    const struct hpclang_rpl_stock_view *view = context;
    if (!value || address >= 0xc60u * 2u || !ram_live(view))
        return 0;
    uint8_t byte = view->sram[view->state_offset + address / 2u];
    *value = (byte >> (4u * (address & 1u))) & 15u;
    return 1;
}

int hpclang_rpl_stock_view_rom(void *context, uint32_t address, uint8_t *value)
{
    struct hpclang_rpl_stock_view *view = context;
    uint8_t byte;
    if (!view || !value || address >= 0x140000u * 2u ||
        hpclang_rpl_stock_rom_read(&view->rom, address / 2u, &byte, 1))
        return 0;
    *value = (byte >> (4u * (address & 1u))) & 15u;
    return 1;
}

static int page_read(struct hpclang_rpl_stock_view *view, uint32_t page, uint32_t address,
                     uint8_t *value)
{
    if (page & 0x7ffu)
        return 0;
    uint32_t offset = (address & 0xfffu) / 2u;
    uint8_t byte;
    if (page <= 0x140000u - 0x800u) {
        if (hpclang_rpl_stock_rom_read(&view->rom, page + offset, &byte, 1))
            return 0;
    } else if (page >= 0x08000000u && page <= 0x08080000u - 0x800u) {
        byte = view->sram[page - 0x08000000u + offset];
    } else
        return 0;
    *value = (byte >> (4u * (address & 1u))) & 15u;
    return 1;
}

int hpclang_rpl_stock_view_saturn(void *context, uint32_t address, uint8_t *value)
{
    struct hpclang_rpl_stock_view *view = context;
    if (!value || address >= 0x100000u || !ram_live(view))
        return 0;
    uint32_t at = view->state_offset + (address >> 12) * 4u;
    uint32_t page = 0;
    for (unsigned i = 0; i < 4; ++i)
        page |= (uint32_t)view->sram[at + i] << (8u * i);
    return page_read(view, page, address, value);
}

int hpclang_rpl_stock_view_mapped(void *context, uint32_t address, uint8_t *value)
{
    const struct hpclang_rpl_stock_mapped_view *mapped = context;
    if (!mapped || !mapped->pages || !value || address >= 0x100000u || !ram_live(mapped->view))
        return 0;
    return page_read(mapped->view, mapped->pages[address >> 12], address, value);
}
