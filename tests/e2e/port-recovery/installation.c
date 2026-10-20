/* Installation witnesses for the supported hp4950 Stock 2.15 fixture.
 * ROM/catalog addresses are fixture inputs, not production profile discovery. */
#include "rpl-flash-replay.h"
#include "expected-library.h"
#include <stddef.h>

volatile unsigned installation_result[8];
#ifdef HPCLANG_INSTALLATION_STARTUP
void hpclang_startup_inventory(const uint32_t *map, const struct hpclang_rpl_reader *reader);
#endif
#ifdef HPCLANG_INSTALLATION_IDENTITY
#include "rpl-recovery-installation.h"
#define HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT EXPECTED_JOURNAL_SLOT
#include "binding.h"
const volatile struct hpclang_recovery_binding hpclang_recovery_binding =
    HPCLANG_RECOVERY_BINDING_INITIALIZER;
const volatile unsigned char hpclang_recovery_identity[48] = "HPCROOT1";
volatile unsigned transaction_pair[3];
#endif
#ifdef HPCLANG_INSTALLATION_TRANSACTION
static uint8_t transaction_scratch[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
volatile unsigned transaction_pair[3];
/* A runtime size keeps the compiler from emitting an absolute-pointer constant
 * aggregate for the position-independent witness. */
static volatile unsigned expected_bytes = sizeof expected_library;
#endif
void hpclang_flash_mapping_swap(unsigned descriptors[2]);
#ifdef HPCLANG_INSTALLATION_PUBLICATION
int hpclang_installation_publish(const uint32_t *map, const struct hpclang_rpl_flash_io *io,
                                 const struct hpclang_rpl_flash_catalog *catalog, uint32_t *slot);
#endif
#ifdef HPCLANG_INSTALLATION_CANCELLATION
int hpclang_installation_cancel(const uint32_t *map, const struct hpclang_rpl_flash_io *io,
                                const struct hpclang_rpl_flash_catalog *catalog, uint32_t *slot);
#endif

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
#ifdef HPCLANG_INSTALLATION_STARTUP
    hpclang_startup_inventory(map, &reader);
#endif
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 0x8611d, 3 + 128 * 16, 0x26bbb};
    const struct hpclang_rpl_flash_io io = {descriptors, read_flash, NULL, 0x140000, 0x200000};
    uint32_t slot = UINT32_MAX;
#if defined(HPCLANG_INSTALLATION_IDENTITY)
    uint8_t identity[48];
    for (unsigned i = 0; i < sizeof identity; ++i)
        identity[i] = hpclang_recovery_identity[i];
    struct hpclang_rpl_flash_installed_recovery installed;
    int result = hpclang_rpl_recovery_installation_stream(
        &io, EXPECTED_IDENTITY_LIBRARY, &catalog, identity, hpclang_recovery_binding.authority_slot,
        hpclang_recovery_binding.journal_slot, &installed);
    if (result >= 0) {
        slot = installed.library_slot;
        transaction_pair[0] = installed.journal_slot;
        transaction_pair[1] = installed.authority_slot;
        transaction_pair[2] = installed.target;
    }
#elif defined(HPCLANG_INSTALLATION_TRANSACTION)
    const struct hpclang_rpl_flash_recovery_layout layout = {
        expected_library, expected_bytes, EXPECTED_RECOVERY_BINDING, EXPECTED_ADMISSION_BINDING};
    struct hpclang_rpl_flash_installed_recovery installed;
    int result = hpclang_rpl_flash_preflight_installed(&io, &layout, &catalog, transaction_scratch,
                                                       sizeof transaction_scratch, &installed);
    if (result >= 0) {
        slot = installed.library_slot;
        transaction_pair[0] = installed.journal_slot;
        transaction_pair[1] = installed.authority_slot;
        transaction_pair[2] = installed.target;
    }
#elif defined(HPCLANG_INSTALLATION_PUBLICATION)
    int result = hpclang_installation_publish(map, &io, &catalog, &slot);
#elif defined(HPCLANG_INSTALLATION_CANCELLATION)
    int result = hpclang_installation_cancel(map, &io, &catalog, &slot);
#else
    int result = hpclang_rpl_flash_match_installation(&io, expected_library,
                                                      sizeof expected_library, &catalog, &slot);
#endif
    hpclang_flash_mapping_swap(descriptors);
    installation_result[0] = 0x48504944;
    installation_result[1] = (unsigned)result;
    installation_result[2] = slot;
    installation_result[5] = l1[0];
    installation_result[6] = l1[1];
    return result >= 0;
}
