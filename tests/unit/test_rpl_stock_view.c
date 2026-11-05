#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include "rpl-stock-view.h"

static uint8_t ram[0x80000];
static unsigned flash_reads;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return 0;
}
static uint8_t flash_byte(uint32_t at)
{
    return (uint8_t)(at * 13u + 7u);
}
uint16_t hpclang_flash_read16(void *context, uint32_t address)
{
    (void)context;
    assert(!(address & 1) && address <= 0x13fffe);
    ++flash_reads;
    return flash_byte(address) | ((uint16_t)flash_byte(address + 1) << 8);
}
static void put(uint32_t at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        ram[at + i] = (uint8_t)(value >> (8 * i));
}
int main(void)
{
    for (unsigned i = 0; i < sizeof ram; ++i)
        ram[i] = (uint8_t)(i * 7 + 3);
    uint32_t sections[2] = {0xc1a, 0x100c1a}, section = 0x08000c1a;
    struct hpclang_rpl_stock_view view, sentinel;
    memset(&sentinel, 0xa5, sizeof sentinel);
    view = sentinel;
    const uint32_t invalid[] = {0, 0x07ffffff, 0x08000001, 0x08080000 - 0xc5c, UINT32_MAX};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        assert(hpclang_rpl_stock_view_capture(&view, ram, invalid[i], &section, sections) < 0);
        assert(!memcmp(&view, &sentinel, sizeof view));
    }
    assert(hpclang_rpl_stock_view_reader(&view) < 0);
    assert(hpclang_rpl_stock_view_capture(&view, ram, 0x08080000 - 0xc60, &section, sections) == 0);
    uint8_t value = 99;
    assert(hpclang_rpl_stock_view_native(&view, 0xc60 * 2 - 1, &value));
    assert(value == (ram[sizeof ram - 1] >> 4));
    assert(!hpclang_rpl_stock_view_native(&view, 0xc60 * 2, &value));
    assert(hpclang_rpl_stock_view_capture(&view, ram, 0x08003344, &section, sections) == 0);
    for (unsigned i = 0; i < 0xc60 * 2; ++i) {
        assert(hpclang_rpl_stock_view_native(&view, i, &value));
        assert(value == ((ram[0x3344 + i / 2] >> (4 * (i & 1))) & 15));
    }
    uint32_t pages[256];
    struct hpclang_rpl_stock_mapped_view mapped = {&view, pages};
    for (unsigned page = 0; page < 256; ++page) {
        uint32_t physical = page & 1 ? 0x08040000 + (page % 64) * 0x800 : (page % 64) * 0x800;
        put(0x3344 + page * 4, physical);
        /* Deliberately differ from the live page map, including SRAM/ROM. */
        pages[page] = page & 1 ? (page % 64) * 0x800 : 0x08040000 + (page % 64) * 0x800;
        const unsigned offsets[] = {0, 1, 4094, 4095};
        for (unsigned j = 0; j < 4; ++j) {
            unsigned at = offsets[j];
            uint8_t byte =
                page & 1 ? ram[physical - 0x08000000 + at / 2] : flash_byte(physical + at / 2);
            assert(hpclang_rpl_stock_view_saturn(&view, page * 4096 + at, &value));
            assert(value == ((byte >> (4 * (at & 1))) & 15));
            byte = page & 1 ? flash_byte(pages[page] + at / 2)
                            : ram[pages[page] - 0x08000000 + at / 2];
            uint8_t before[4];
            memcpy(before, ram + 0x3344 + page * 4, sizeof before);
            assert(hpclang_rpl_stock_view_mapped(&mapped, page * 4096 + at, &value));
            assert(value == ((byte >> (4 * (at & 1))) & 15));
            assert(!memcmp(before, ram + 0x3344 + page * 4, sizeof before));
        }
    }
    const uint32_t bad_pages[] = {1, 0x140000, 0x07fff800, 0x08080000, 0xfffff800};
    for (unsigned i = 0; i < sizeof bad_pages / sizeof bad_pages[0]; ++i) {
        put(0x3344, bad_pages[i]);
        value = 99;
        unsigned before = flash_reads;
        assert(!hpclang_rpl_stock_view_saturn(&view, 0, &value));
        assert(value == 99 && flash_reads == before);
        pages[0] = bad_pages[i];
        assert(!hpclang_rpl_stock_view_mapped(&mapped, 0, &value));
        assert(value == 99 && flash_reads == before);
    }
    put(0x3344, 0x13f800);
    pages[0] = 0x13f800;
    assert(hpclang_rpl_stock_view_mapped(&mapped, 4095, &value));
    assert(value == (flash_byte(0x13ffff) >> 4));
    assert(hpclang_rpl_stock_view_saturn(&view, 4095, &value));
    assert(value == (flash_byte(0x13ffff) >> 4));
    assert(hpclang_rpl_stock_view_rom(&view, 0, &value) && value == (flash_byte(0) & 15));
    assert(hpclang_rpl_stock_view_rom(&view, 0x27ffff, &value) &&
           value == (flash_byte(0x13ffff) >> 4));
    value = 99;
    assert(!hpclang_rpl_stock_view_rom(&view, 0x280000, &value) && value == 99);
    assert(!hpclang_rpl_stock_view_saturn(&view, 0x100000, &value) && value == 99);
    assert(!hpclang_rpl_stock_view_mapped(&mapped, 0x100000, &value) && value == 99);
    for (unsigned bit = 0; bit < 32; ++bit) {
        section ^= 1u << bit;
        assert(!hpclang_rpl_stock_view_native(&view, 0, &value) && value == 99);
        assert(!hpclang_rpl_stock_view_saturn(&view, 0, &value) && value == 99);
        assert(!hpclang_rpl_stock_view_mapped(&mapped, 0, &value) && value == 99);
        section ^= 1u << bit;
        for (unsigned i = 0; i < 2; ++i) {
            sections[i] ^= 1u << bit;
            unsigned before = flash_reads;
            assert(!hpclang_rpl_stock_view_rom(&view, 0, &value) && value == 99);
            assert(!hpclang_rpl_stock_view_saturn(&view, 0, &value) && value == 99);
            assert(!hpclang_rpl_stock_view_mapped(&mapped, 0, &value) && value == 99);
            assert(flash_reads == before);
            sections[i] ^= 1u << bit;
        }
    }
    section = 0;
    view = sentinel;
    assert(hpclang_rpl_stock_view_capture(&view, ram, 0x08003344, &section, sections) < 0);
    assert(!memcmp(&view, &sentinel, sizeof view));
    assert(!hpclang_rpl_stock_view_native(0, 0, &value));
    assert(!hpclang_rpl_stock_view_rom(0, 0, &value));
    assert(!hpclang_rpl_stock_view_saturn(0, 0, &value));
    assert(!hpclang_rpl_stock_view_mapped(0, 0, &value));
    mapped.pages = 0;
    assert(!hpclang_rpl_stock_view_mapped(&mapped, 0, &value));
    return 0;
}
