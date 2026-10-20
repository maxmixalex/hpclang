/* Read-only transaction-state predicate before the reservation is released.
 * Not an installation-root resolver or permission to replay/erase Flash. */
#include "binding.h"
#include "rpl-flash-replay.h"
#include "rpl-stock-memory.h"
#include <stddef.h>

const volatile struct hpclang_recovery_binding hpclang_recovery_binding =
    HPCLANG_RECOVERY_BINDING_INITIALIZER;
static uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];

void hpclang_flash_mapping_swap(unsigned descriptors[2]);
static int readable(uint32_t address, uint32_t bytes)
{
    uint32_t offset = address & 0x1ffffu;
    return address >= 0x140000u && address < 0x200000u && offset >= 0x105u && offset % 4u == 1u &&
           offset + bytes < 0x20000u;
}
static int read_flash(void *context, uint32_t address, uint16_t *value)
{
    (void)context;
    *value = *(volatile uint16_t *)address;
    return 1;
}
int hpclang_recovery_predicate(const uint32_t *map)
{
    (void)map;
    uint32_t j = hpclang_recovery_binding.journal_slot;
    uint32_t a = hpclang_recovery_binding.authority_slot;
    if (hpclang_recovery_binding.magic[0] != 0x42435048u ||
        hpclang_recovery_binding.magic[1] != 0x31444e49u || !readable(j, sizeof journal) ||
        !readable(a, sizeof authority))
        return 0;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if ((l1[0] & 0xfff00803u) != 0x802u || (l1[1] & 0xfff00803u) != 0x100802u)
        return 0;
    unsigned descriptors[2] = {(l1[0] & ~0xcu) | 0xc00u, (l1[1] & ~0xcu) | 0xc00u};
    hpclang_flash_mapping_swap(descriptors);
    for (unsigned i = 0; i < sizeof journal; ++i)
        journal[i] = *(volatile uint8_t *)(j + i);
    for (unsigned i = 0; i < sizeof authority; ++i)
        authority[i] = *(volatile uint8_t *)(a + i);
    enum hpclang_rpl_flash_authority_phase phase;
    int result =
        hpclang_rpl_flash_authority_inspect(0x140000, 0x200000, a, authority, sizeof authority, j,
                                            journal, sizeof journal, &phase) == 0;
    result = result && (phase == HPCLANG_RPL_FLASH_AUTHORITY_ARMED ||
                        (phase == HPCLANG_RPL_FLASH_AUTHORITY_CLOSING && journal[0] == 0));
    if (result) {
        const struct hpclang_rpl_flash_io io = {descriptors, read_flash, NULL, 0x140000, 0x200000};
        result = hpclang_rpl_flash_bound_membership(&io, a, authority, sizeof authority, j, journal,
                                                    sizeof journal) == 0;
    }
    hpclang_flash_mapping_swap(descriptors);
    return result;
}
