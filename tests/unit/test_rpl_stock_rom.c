#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include "rpl-stock-rom.h"

static unsigned reads;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return 0;
}
uint16_t hpclang_flash_read16(void *context, uint32_t address)
{
    (void)context;
    assert(!(address & 1) && address <= 0x13fffe);
    ++reads;
    return (uint8_t)(address * 13u + 7u) | (uint16_t)(uint8_t)((address + 1u) * 13u + 7u) << 8;
}
int main(void)
{
    uint32_t sections[2] = {0x802, 0x100802};
    struct hpclang_rpl_stock_rom rom;
    assert(!hpclang_rpl_stock_rom_capture(&rom, sections));
    const uint32_t starts[] = {0, 1, 63, 64, 65, 0xfffff, 0x100000, 0x13fffe, 0x13ffff, 0x140000};
    uint8_t out[80], before[80];
    memset(before, 0xa5, sizeof before);
    for (unsigned j = 0; j < sizeof starts / sizeof starts[0]; ++j) {
        for (unsigned length = 0; length <= 65; ++length) {
            memcpy(out, before, sizeof out);
            reads = 0;
            int result = hpclang_rpl_stock_rom_read(&rom, starts[j], out, length);
            if (length > 0x140000u - starts[j]) {
                assert(result == HPCLANG_RPL_SPAN_BOUNDS && !reads &&
                       !memcmp(out, before, sizeof out));
            } else {
                assert(!result);
                assert(reads == (length ? (length + (starts[j] & 1u) + 1u) / 2u : 0));
                for (unsigned i = 0; i < length; ++i)
                    assert(out[i] == (uint8_t)((starts[j] + i) * 13u + 7u));
                assert(!memcmp(out + length, before + length, sizeof out - length));
            }
        }
    }
    for (unsigned section = 0; section < 2; ++section) {
        for (unsigned bit = 0; bit < 32; ++bit) {
            sections[section] ^= 1u << bit;
            reads = 0;
            memcpy(out, before, sizeof out);
            assert(hpclang_rpl_stock_rom_read(&rom, 0, out, 64) == HPCLANG_RPL_SPAN_UNSUPPORTED);
            assert(!reads && !memcmp(out, before, sizeof out));
            sections[section] ^= 1u << bit;
        }
        uint32_t old = sections[section];
        const uint32_t bad[] = {0, 0x402, 0x100802, 0x802};
        for (unsigned i = 0; i < 4; ++i) {
            if (bad[i] == old)
                continue;
            sections[section] = bad[i];
            struct hpclang_rpl_stock_rom previous = rom;
            assert(hpclang_rpl_stock_rom_capture(&rom, sections) < 0);
            assert(!memcmp(&rom, &previous, sizeof rom));
        }
        sections[section] = old;
    }
    assert(hpclang_rpl_stock_rom_read(&rom, UINT32_MAX, out, 1) < 0);
    assert(hpclang_rpl_stock_rom_read(&rom, 1, out, UINT32_MAX) < 0);
    assert(hpclang_rpl_stock_rom_reader(&rom) < 0);
    assert(hpclang_rpl_stock_rom_capture(0, sections) < 0);
    return 0;
}
