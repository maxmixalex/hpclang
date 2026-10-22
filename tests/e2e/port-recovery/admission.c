/* Sealed installation/transaction predicate for the hp4950 Stock fixture.
 * Content identity does not establish reservation ownership or startup order. */
#include "binding.h"
#include "rpl-recovery-installation.h"
#include "rpl-recovery-reservation.h"
#include "rpl-object-store.h"
#include <stddef.h>

const volatile struct hpclang_recovery_binding hpclang_recovery_binding =
    HPCLANG_RECOVERY_BINDING_INITIALIZER;
const volatile unsigned char hpclang_recovery_identity[48] = "HPCROOT1";
const volatile uint32_t hpclang_recovery_library_id = UINT32_MAX;
const volatile uint32_t hpclang_recovery_creation_slot = UINT32_MAX;
const volatile struct hpclang_recovery_reservation_binding hpclang_recovery_reservation =
    HPCLANG_RECOVERY_RESERVATION_INITIALIZER;

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
/* Keep directory buffers out of both streaming preflight call chains. */
__attribute__((noinline)) static int
reservation_object_matches(const struct hpclang_rpl_reader *reader, const uint8_t *name,
                           uint32_t length, const uint8_t header[80])
{
    uint32_t home;
    struct hpclang_rpl_directory_entry_view entry;
    if (hpclang_rpl_store_home_object(reader, &home))
        return 0;
    int found = hpclang_rpl_directory_find(reader, home, name, length, &entry);
    if (found == HPCLANG_RPL_STORE_NOT_FOUND)
        return 1; /* No RAM object will be released; retain the RAM-loss path. */
    return !found &&
           hpclang_rpl_recovery_reservation_match(reader, entry.object_address, header) == 1;
}

__attribute__((noinline)) static int
reservation_matches(const struct hpclang_rpl_reader *reader,
                    const struct hpclang_rpl_flash_installed_recovery *installed,
                    const uint8_t identity[48], const struct hpclang_rpl_flash_io *io,
                    const struct hpclang_rpl_flash_catalog *catalog)
{
    uint32_t length = hpclang_recovery_reservation.name_length;
    uint32_t capacity = hpclang_recovery_reservation.capacity;
    if (hpclang_recovery_reservation.magic[0] != 0x52435048u ||
        hpclang_recovery_reservation.magic[1] != 0x31565345u || !length || length > 127u ||
        capacity < HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES ||
        capacity > (0x80000u - 10u) / 2u)
        return 0;
    uint8_t name[128], header[HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES];
    for (unsigned i = 0; i < length; ++i) {
        name[i] = hpclang_recovery_reservation.name[i];
        if (name[i] < 33 || name[i] > 126)
            return 0;
    }
    uint32_t crc = 0;
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t at = installed->journal_slot + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 4u + i;
        uint16_t word;
        if (!read_flash(NULL, at & ~1u, &word))
            return 0;
        crc |= (uint32_t)(uint8_t)(word >> (8u * (at & 1u))) << (8u * i);
    }
    if (!hpclang_rpl_recovery_reservation_header(identity, installed, crc, capacity, header))
        return 0;
    uint32_t claim_slot = hpclang_recovery_creation_slot;
    if (claim_slot != UINT32_MAX) {
        enum hpclang_rpl_flash_creation_phase phase;
        if (hpclang_rpl_recovery_installation_creation(io, hpclang_recovery_library_id, catalog,
                                                       header, claim_slot, name, length, &phase) ||
            phase != HPCLANG_RPL_FLASH_CREATION_CREATED)
            return 0;
    }
    return reservation_object_matches(reader, name, length, header);
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
    int admitted = phase > 0 && reservation_matches(&reader, &installed, identity, &io, &catalog);
    hpclang_flash_mapping_swap(descriptors);
    return admitted;
}
