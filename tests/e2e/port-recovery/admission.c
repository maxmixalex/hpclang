/* Sealed installation/transaction predicate for the hp4950 Stock fixture.
 * Content identity does not establish reservation ownership or startup order. */
#include "binding.h"
#include "rpl-recovery-installation.h"
#include <stddef.h>

const volatile struct hpclang_recovery_binding hpclang_recovery_binding =
    HPCLANG_RECOVERY_BINDING_INITIALIZER;
const volatile unsigned char hpclang_recovery_identity[48] = "HPCROOT1";
const volatile uint32_t hpclang_recovery_library_id = UINT32_MAX;

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
    uint32_t j = hpclang_recovery_binding.journal_slot;
    uint32_t a = hpclang_recovery_binding.authority_slot;
    if (hpclang_recovery_binding.magic[0] != 0x42435048u ||
        hpclang_recovery_binding.magic[1] != 0x31444e49u ||
        hpclang_recovery_library_id == UINT32_MAX)
        return 0;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if ((l1[0] & 0xfff00803u) != 0x802u || (l1[1] & 0xfff00803u) != 0x100802u)
        return 0;
    unsigned descriptors[2] = {(l1[0] & ~0xcu) | 0xc00u, (l1[1] & ~0xcu) | 0xc00u};
    hpclang_flash_mapping_swap(descriptors);
    const struct hpclang_rpl_reader reader = {mapped_nibble, (void *)map};
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 0x8611d, 3 + 128 * 16, 0x26bbb};
    const struct hpclang_rpl_flash_io io = {descriptors, read_flash, NULL, 0x140000, 0x200000};
    uint8_t identity[48];
    for (unsigned i = 0; i < sizeof identity; ++i)
        identity[i] = hpclang_recovery_identity[i];
    struct hpclang_rpl_flash_installed_recovery installed;
    int phase = hpclang_rpl_recovery_installation_stream(&io, hpclang_recovery_library_id, &catalog,
                                                         identity, a, j, &installed);
    hpclang_flash_mapping_swap(descriptors);
    return phase > 0;
}
