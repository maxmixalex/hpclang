#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
#include "rpl-port.h"
#include "rpl-stock-memory.h"
#include "rpl-stock-rom.h"
#include "irq.h"

volatile uint32_t hpclang_rom_bank_evidence[4];
volatile uint32_t hpclang_rom_profile_evidence[4];

/* Identify the reviewed immutable image before using its layout locations. */
static int rom_banks_probe(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!boot || (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G))
        return 0x7001;
    struct hpclang_rpl_stock_rom rom;
    const struct hpclang_rpl_rom_profile *profile;
    if (hpclang_rpl_stock_rom_reader(&rom))
        return 0x700d;
    if (hpclang_rpl_rom_profile_identify(hpclang_rpl_stock_rom_read, &rom, boot->model, &profile))
        return 0x700e;
    hpclang_rom_profile_evidence[0] = profile->model;
    hpclang_rom_profile_evidence[1] = profile->immutable_bytes;
    hpclang_rom_profile_evidence[2] = profile->geometry_operands;
    hpclang_rom_profile_evidence[3] = profile->geometry_bases;
    uintptr_t state = boot->saturn_state;
    if (state < 0x08000000u || state > 0x08080000u - profile->geometry_bases - 64u || (state & 3u))
        return 0x7002;
    const volatile uint32_t *l1 = (void *)0x07f00000u;
    if ((l1[0] & 0xfff00803u) != 0x802u)
        return 0x7003;
    const volatile uint32_t *locations = (void *)(uintptr_t)profile->geometry_operands;
    uint32_t count_at = locations[0], directory = locations[1];
    if (!count_at || count_at >= profile->flash_bytes || directory != count_at + 1u)
        return 0x7004;
    uint32_t count = *(const volatile uint8_t *)(uintptr_t)count_at;
    if (count != 14 || count > profile->flash_bytes - directory)
        return 0x7005;
    uint8_t sizes[16];
    uint32_t bases[16], saved_map[64];
    const volatile uint32_t *native_bases = (void *)(state + profile->geometry_bases);
    const volatile uint32_t *read_map = (void *)state;
    for (unsigned i = 0; i < count; ++i)
        sizes[i] = *(const volatile uint8_t *)(uintptr_t)(directory + i);
    for (unsigned i = 0; i < 16; ++i)
        bases[i] = native_bases[i];
    for (unsigned i = 0; i < 64; ++i)
        saved_map[i] = read_map[0x40 + i];
    unsigned tested = 0, samples = 0;
    for (unsigned index = 0; index < 16; ++index) {
        struct hpclang_rpl_rom_bank bank = {0xdead, 0xbeef};
        int result = hpclang_rpl_rom_bank_layout(sizes, count, directory, profile->flash_bytes,
                                                 bases, index, &bank);
        if (index >= count) {
            if (result || bank.physical != 0xdead || bank.bytes != 0xbeef)
                return 0x7006;
            continue;
        }
        struct hpclang_rpl_stock_rom_bank memory;
        struct hpclang_rpl_reader reader;
        if (result != 1 || !hpclang_rpl_stock_rom_bank_reader(&memory, &reader, &bank))
            return 0x7007;
        if (bank.bytes != (index ? 0x20000u : 0x18000u))
            return 0x7008;
        const volatile uint8_t *physical = (void *)(uintptr_t)bank.physical;
        for (uint32_t page = 0; page < bank.bytes; page += 0x800u) {
            const uint32_t positions[] = {page * 2u, page * 2u + 1u, (page + 0x800u) * 2u - 2u,
                                          (page + 0x800u) * 2u - 1u};
            for (unsigned i = 0; i < 4; ++i) {
                uint32_t at = positions[i];
                uint8_t value;
                if (!reader.read_nibble(reader.context, 0x40000u + at, &value) ||
                    value != ((physical[at / 2u] >> (4u * (at & 1u))) & 15u))
                    return 0x7009;
                ++samples;
            }
        }
        const uint32_t outside[] = {0x3ffffu, 0x40000u + bank.bytes * 2u, 0x80000u};
        for (unsigned i = 0; i < 3; ++i) {
            uint8_t value = 99;
            if (reader.read_nibble(reader.context, outside[i], &value) || value != 99)
                return 0x700a;
        }
        ++tested;
    }
    for (unsigned i = 0; i < 64; ++i)
        if (read_map[0x40 + i] != saved_map[i])
            return 0x700b;
    hpclang_rom_bank_evidence[0] = tested;
    hpclang_rom_bank_evidence[1] = samples;
    hpclang_rom_bank_evidence[2] = l1[0];
    hpclang_rom_bank_evidence[3] = l1[1];
    return tested == 14 && samples == 3520 ? 0 : 0x700c;
}

int hpclang_rom_banks_probe(void)
{
    uint32_t irq = hpclang_irq_save_disable();
    int result = rom_banks_probe();
    hpclang_irq_restore(irq);
    return result;
}
