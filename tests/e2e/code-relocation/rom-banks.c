#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
#include "rpl-port.h"
#include "rpl-stock-memory.h"
#include "rpl-stock-rom.h"
#include "rpl-stock-view.h"
#include "rpl-bank-plan.h"
#include "irq.h"

volatile uint32_t hpclang_rom_bank_evidence[4];
volatile uint32_t hpclang_rom_profile_evidence[4];
volatile uint32_t hpclang_bank_plan_evidence[4];
static struct hpclang_rpl_bank_workspace plan_workspace;
static struct hpclang_rpl_bank_plan bank_plan;
static uint8_t native_before[0xc60];

static int plan_observe(void *context, uint32_t mapping, uint32_t address, uint8_t value)
{
    (void)address;
    if (mapping < 1 || mapping > 3 || value > 15)
        return -73;
    ++*(unsigned *)context;
    return 0;
}

static int plan_probe(const struct hpclang_rpl_rom_profile *profile)
{
    struct hpclang_rpl_stock_view view;
    if (hpclang_rpl_stock_view_reader(&view))
        return 0x7010;
    const struct hpclang_rpl_reader saturn = {hpclang_rpl_stock_view_saturn, &view};
    const struct hpclang_rpl_reader native = {hpclang_rpl_stock_view_native, &view};
    const struct hpclang_rpl_reader rom = {hpclang_rpl_stock_view_rom, &view};
    const struct hpclang_rpl_bank_sources sources = {&saturn, &native, &rom, profile, 1,
                                                     2,       3,       0,    0x40000, 0x860cc};
    for (unsigned i = 0; i < sizeof native_before; ++i)
        native_before[i] = view.sram[view.state_offset + i];
    unsigned accepted = 0, rejected = 0, observations = 0;
    for (unsigned index = 0; index < 18; ++index) {
        uint32_t entry = index == 16 ? 0x22e83u : 0x22e99u;
        if (index < 16) {
            entry = 0;
            for (unsigned j = 0; j < 5; ++j) {
                uint8_t value;
                if (!saturn.read_nibble(saturn.context, 0x26bbbu + index * 7u + j, &value))
                    return 0x7011;
                entry |= (uint32_t)value << (4u * j);
            }
        }
        int result = hpclang_rpl_bank_plan_prepare(&sources, entry, &plan_workspace, &bank_plan,
                                                   plan_observe, &observations);
        if (index == 14 || index == 15) {
            if (result >= 0)
                return 0x7012;
            ++rejected;
        } else {
            uint32_t expected = index < 16 ? index : index == 16 ? 2u : 4u;
            if (result || bank_plan.bank != expected || !bank_plan.geometry.bytes)
                return 0x7013;
            ++accepted;
        }
    }
    for (unsigned i = 0; i < sizeof native_before; ++i)
        if (native_before[i] != view.sram[view.state_offset + i])
            return 0x7014;
    hpclang_bank_plan_evidence[0] = accepted;
    hpclang_bank_plan_evidence[1] = rejected;
    hpclang_bank_plan_evidence[2] = observations;
    hpclang_bank_plan_evidence[3] = sizeof native_before;
    return accepted == 16 && rejected == 2 && observations ? 0 : 0x7015;
}

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
    return tested == 14 && samples == 3520 ? plan_probe(profile) : 0x700c;
}

int hpclang_rom_banks_probe(void)
{
    uint32_t irq = hpclang_irq_save_disable();
    int result = rom_banks_probe();
    hpclang_irq_restore(irq);
    return result;
}
