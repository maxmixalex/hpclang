#include "stock-flash.h"
#include "rpl-flash-record.h"
#include "rpl-stock-memory.h"
#include <stdint.h>

volatile uint32_t hpclang_e2e_result;
struct hpclang_sst_device hpclang_flash_probe_device;
static uint8_t block[512];

uint16_t hpclang_flash_read16(void *context, uint32_t address);

static uint32_t fingerprint(void)
{
    uint32_t value = 2166136261u;
    for (uint32_t address = 0; address < 0x200000u; address += 2u)
        value = (value ^ hpclang_flash_read16(0, address)) * 16777619u;
    return value;
}

static uint32_t unused_sector(void)
{
    /* Only the disposable Stock test image is eligible: require a completely
     * unused bank tail, not merely an all-FF sector inside a live object. */
    for (uint32_t bank = 0x140000u; bank < 0x200000u; bank += 0x20000u) {
        uint32_t index = (bank - 0x140000u) / 0x20000u;
        if (hpclang_flash_read16(0, bank + 0x100u) != (0xf0u | ((index + 8u) << 8u)))
            return 0;
        struct hpclang_rpl_reader reader;
        struct hpclang_rpl_flash_space space;
        if (!hpclang_rpl_stock_port2_reader(index, &reader) ||
            hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space))
            return 0;
        if (space.address != 0x20au)
            continue;
        if ((hpclang_flash_read16(0, bank + 0x104u) & 0xff00u) != 0xff00u)
            continue;
        uint32_t address;
        for (address = bank + 0x106u; address < bank + 0x20000u; address += 2u)
            if (hpclang_flash_read16(0, address) != UINT16_MAX)
                break;
        if (address == bank + 0x20000u)
            return bank + 0x1f000u;
    }
    return 0;
}

int main(void)
{
    const volatile uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    uint32_t original[2] = {l1[0], l1[1]};
    uint32_t before = fingerprint();
    unsigned failure = 1;
    for (unsigned pass = 0; pass < 3; ++pass) {
        if (hpclang_stock_flash_identify(&hpclang_flash_probe_device) != HPCLANG_SST_OK)
            goto done;
        failure = 2;
        if (hpclang_flash_probe_device.bytes != 0x200000u ||
            hpclang_flash_probe_device.sector_bytes != 4096u)
            goto done;
        failure = 3;
        if (l1[0] != original[0] || l1[1] != original[1])
            goto done;
        failure = 4;
        if (fingerprint() != before)
            goto done;
        failure = 1;
    }
    failure = 5;
    uint32_t scratch = unused_sector();
    if (!scratch)
        goto done;
    failure = 6;
    if (hpclang_stock_flash_program_word(0x13fffeu, 0) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_erase_sector(0x13f000u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program_word(0x200000u, 0) != HPCLANG_SST_INVALID)
        goto done;
    failure = 7;
    if (hpclang_stock_flash_program_word(scratch + 2u, 0x1234u) != HPCLANG_SST_OK ||
        hpclang_flash_read16(0, scratch + 2u) != 0x1234u ||
        hpclang_flash_read16(0, scratch) != UINT16_MAX)
        goto done;
    failure = 8;
    struct hpclang_rpl_reader reader;
    struct hpclang_rpl_flash_space space = {123, 456};
    if (!hpclang_rpl_stock_port2_reader((scratch - 0x140000u) / 0x20000u, &reader) ||
        hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space) != HPCLANG_RPL_SPAN_MALFORMED ||
        space.address != 123 || space.nibbles != 456)
        goto done;
    if (hpclang_stock_flash_program_word(scratch + 2u, UINT16_MAX) != HPCLANG_SST_NEEDS_ERASE ||
        hpclang_flash_read16(0, scratch + 2u) != 0x1234u ||
        hpclang_stock_flash_program_word(scratch + 4094u, 0x5678u) != HPCLANG_SST_OK ||
        hpclang_flash_read16(0, scratch + 4094u) != 0x5678u)
        goto done;
    failure = 9;
    const uint8_t conflict[4] = {0, 0, 0xff, 0xff};
    if (hpclang_stock_flash_program(scratch, conflict, sizeof(conflict)) !=
            HPCLANG_SST_NEEDS_ERASE ||
        hpclang_flash_read16(0, scratch) != UINT16_MAX ||
        hpclang_flash_read16(0, scratch + 2u) != 0x1234u)
        goto done;
    failure = 10;
    if (hpclang_stock_flash_program(0x1ffffeu, conflict, sizeof(conflict)) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, conflict, UINT32_MAX - 1u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch + 1u, conflict, 2u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, conflict, 3u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, conflict, 0u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, 0, 2u) != HPCLANG_SST_INVALID)
        goto done;
    failure = 11;
    for (uint32_t i = 0; i < sizeof(block); ++i)
        block[i] = (uint8_t)(i * 37u + 11u);
    for (unsigned pass = 0; pass < 2; ++pass) {
        if (hpclang_stock_flash_program(scratch + 16u, block, sizeof(block)) != HPCLANG_SST_OK)
            goto done;
        for (uint32_t i = 0; i < sizeof(block); i += 2u)
            if (hpclang_flash_read16(0, scratch + 16u + i) !=
                ((uint16_t)block[i] | (uint16_t)block[i + 1u] << 8u))
                goto done;
    }
    failure = 12;
    if (hpclang_flash_read16(0, scratch + 14u) != UINT16_MAX ||
        hpclang_flash_read16(0, scratch + 16u + sizeof(block)) != UINT16_MAX ||
        l1[0] != original[0] || l1[1] != original[1])
        goto done;
    failure = 13;
    if (hpclang_stock_flash_erase_sector(scratch) != HPCLANG_SST_OK || fingerprint() != before ||
        l1[0] != original[0] || l1[1] != original[1])
        goto done;
    failure = 14;
    if (hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space) || space.address != 0x20au ||
        space.nibbles != 0x3fdf4u)
        goto done;
    failure = 0;
done:
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
    return 0;
}
