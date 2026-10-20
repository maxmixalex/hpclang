/* Read-only inventory on the supported Stock 2.15 fixture. Profile addresses
 * are verified fixture inputs, not production ROM discovery or startup safety. */
#include "rpl-port.h"
#include "rpl-stock-memory.h"
#include <stddef.h>

volatile uint32_t startup_result[3];
volatile uint32_t startup_entries[128][4];

struct inventory
{
    const struct hpclang_rpl_reader *mapped;
    uint8_t sizes[16];
    uint32_t bases[16], count, directory, entries;
    struct hpclang_rpl_rom_bank active;
    struct hpclang_rpl_reader port1;
};

static int eram_nibble(void *context, uint32_t address, uint8_t *value)
{
    const struct inventory *inventory = context;
    if (address < 0x40000u || address >= 0x80000u)
        return 0;
    return inventory->port1.read_nibble(inventory->port1.context, address - 0x40000u, value);
}

static int bank_nibble(void *context, uint32_t address, uint8_t *value)
{
    const struct inventory *inventory = context;
    if (address < 0x40000u || address - 0x40000u >= inventory->active.bytes * 2u)
        return 0;
    const volatile uint8_t *p =
        (void *)(uintptr_t)(inventory->active.physical + (address - 0x40000u) / 2u);
    *value = (*p >> (4u * (address & 1u))) & 15u;
    return 1;
}

static int visit(void *context, uint32_t record, uint32_t number,
                 const struct hpclang_rpl_library_binding *binding)
{
    (void)record;
    struct inventory *inventory = context;
    if (inventory->entries >= 128u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t first = 0, end = 0x100000u, bank = UINT32_MAX;
    struct hpclang_rpl_reader selected = *inventory->mapped;
    if (binding->bank_switch) {
        struct hpclang_rpl_bank_binding mapping;
        int result = hpclang_rpl_bank_lookup(inventory->mapped, 0x26bbbu, 0x26c39u,
                                             binding->bank_switch, &mapping);
        if (result != 1)
            return result < 0 ? result : HPCLANG_RPL_SPAN_UNAVAILABLE;
        first = 0x40000u;
        if (mapping.kind == HPCLANG_RPL_BANK_ERAM) {
            /* Inventory bank 16 denotes the separate ERAM switch-table entry. */
            bank = 16;
            selected = (struct hpclang_rpl_reader){eram_nibble, inventory};
            end = first + HPCLANG_RPL_PORT1_NIBBLES;
        } else {
            bank = mapping.index;
            result = hpclang_rpl_rom_bank_layout(inventory->sizes, inventory->count,
                                                 inventory->directory, 0x200000u, inventory->bases,
                                                 bank, &inventory->active);
            if (result != 1)
                return result < 0 ? result : HPCLANG_RPL_SPAN_UNAVAILABLE;
            selected = (struct hpclang_rpl_reader){bank_nibble, inventory};
            end = first + inventory->active.bytes * 2u;
        }
    }
    uint32_t target = 0;
    int result =
        hpclang_rpl_library_configuration(&selected, first, end, binding->address, number, &target);
    if (result < 0)
        return result;
    volatile uint32_t *row = startup_entries[inventory->entries++];
    row[0] = number;
    row[1] = binding->address;
    row[2] = bank;
    row[3] = target; /* Zero denotes absent in this fixture, never a failed read. */
    return 0;
}

void hpclang_startup_inventory(const uint32_t *map, const struct hpclang_rpl_reader *mapped)
{
    startup_result[0] = (uint32_t)HPCLANG_RPL_SPAN_UNAVAILABLE;
    uintptr_t state = (uintptr_t)map;
    if (state < 0x08000000u || state > 0x08080000u - 0xbe0u - 64u || (state & 3u))
        return;
    /* The original ARM initializer obtains the count and size-array addresses
     * from these two literal-pool words, then fills state+0xBE0. */
    const volatile uint32_t *locations = (void *)0x248a0u;
    uint32_t count_address = locations[0], directory = locations[1];
    if (!count_address || count_address >= 0x200000u || directory != count_address + 1u)
        return;
    uint32_t count = *(const volatile uint8_t *)(uintptr_t)count_address;
    if (!count || count > 16u || count > 0x200000u - directory)
        return;
    struct inventory inventory = {.mapped = mapped, .count = count, .directory = directory};
    const volatile uint32_t *l1 = (void *)0x07f00000u;
    if (!hpclang_rpl_stock_port1_capture(&inventory.port1, (void *)0x08020000u, l1[0x80]))
        return;
    for (uint32_t i = 0; i < count; ++i)
        inventory.sizes[i] = *(const volatile uint8_t *)(uintptr_t)(directory + i);
    const volatile uint32_t *bases = (void *)(state + 0xbe0u);
    for (uint32_t i = 0; i < 16; ++i)
        inventory.bases[i] = bases[i];
    int result = hpclang_rpl_rom_bank_layout(inventory.sizes, count, directory, 0x200000u,
                                             inventory.bases, 0, &inventory.active);
    if (result == 1)
        result = hpclang_rpl_catalog_visit(mapped, 0x8611du, 3u + 128u * 16u, visit, &inventory);
    startup_result[0] = (uint32_t)result;
    if (!result) {
        startup_result[1] = inventory.entries;
        startup_result[2] = count;
    }
}
