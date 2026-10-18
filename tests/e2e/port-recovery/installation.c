/* Read-only installation witness for the supported hp4950 Stock 2.15 fixture.
 * ROM/catalog addresses are fixture inputs, not production profile discovery. */
#include "rpl-flash-write.h"
#include "expected-library.h"
#include <stddef.h>

volatile unsigned installation_result[8];
void hpclang_flash_mapping_swap(unsigned descriptors[2]);

static int read_flash(void *context, uint32_t address, uint16_t *value)
{
    (void)context;
    if (address < 0x140000u || address >= 0x200000u || (address & 1u))
        return 0;
    *value = *(volatile uint16_t *)address;
    return 1;
}

static int mapped_nibble(void *context, uint32_t address, uint8_t *value)
{
    const uint32_t *map = context;
    if (address >= 0x100000u)
        return 0;
    uint32_t page = map[address >> 12];
    if (!(page <= 0x140000u - 2048u || (page >= 0x08000000u && page <= 0x08080000u - 2048u)))
        return 0;
    *value =
        (*(volatile uint8_t *)(page + ((address & 4095u) >> 1)) >> (4u * (address & 1u))) & 15u;
    return 1;
}

int hpclang_recovery_predicate(const uint32_t *map)
{
    const volatile unsigned *l1 = (void *)0x07f00000;
    if ((l1[0] & 0xfff00803u) != 0x802u || (l1[1] & 0xfff00803u) != 0x100802u)
        return 0;
    installation_result[3] = l1[0];
    installation_result[4] = l1[1];
    unsigned descriptors[2] = {(l1[0] & ~0xcu) | 0xc00u, (l1[1] & ~0xcu) | 0xc00u};
    hpclang_flash_mapping_swap(descriptors);
    const struct hpclang_rpl_reader reader = {mapped_nibble, (void *)map};
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 0x8611d, 3 + 128 * 16, 0x26bbb};
    const struct hpclang_rpl_flash_io io = {descriptors, read_flash, NULL, 0x140000, 0x200000};
    uint32_t slot = UINT32_MAX;
    int result = hpclang_rpl_flash_match_installation(&io, expected_library,
                                                      sizeof expected_library, &catalog, &slot);
    hpclang_flash_mapping_swap(descriptors);
    installation_result[0] = 0x48504944;
    installation_result[1] = (unsigned)result;
    installation_result[2] = slot;
    installation_result[5] = l1[0];
    installation_result[6] = l1[1];
    return result == 0;
}
