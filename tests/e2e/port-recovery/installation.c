/* Installation witnesses for the supported hp4950 Stock 2.15 fixture.
 * ROM/catalog addresses are fixture inputs, not production profile discovery. */
#include "rpl-flash-replay.h"
#ifdef HPCLANG_INSTALLATION_TEMPLATE
#include "template.h"
#else
#include "expected-library.h"
#define EXPECTED_LIBRARY_BYTES sizeof expected_library
#endif
#include <stddef.h>

volatile unsigned installation_result[8];
#ifdef HPCLANG_INSTALLATION_CREATION_ADMISSION
#include "rpl-recovery-installation.h"
#include "rpl-recovery-reservation.h"
volatile uint32_t staged_creation_result[3];
#endif
#ifdef HPCLANG_INSTALLATION_CLEANUP
int hpclang_installation_cleanup(const struct hpclang_rpl_flash_io *io);
#endif
#ifdef HPCLANG_INSTALLATION_STARTUP
#include "rpl-recovery-installation.h"
#include "rpl-stock-memory.h"
volatile uint32_t placement_result[2];
volatile uint32_t preparation_result[4];
#ifdef HPCLANG_INSTALLATION_TEMPLATE_CREATION
volatile uint32_t preparation_creation_slot;
#endif
#ifndef HPCLANG_INSTALLATION_TEMPLATE
uint8_t prepared_library[EXPECTED_LIBRARY_BYTES];
/* Keep the layout pointer in the runtime PIC construction, not a relocated
 * constant aggregate embedded in the recovery image. */
static volatile uint32_t preparation_bytes = sizeof expected_library;
#endif
void hpclang_startup_inventory(const uint32_t *map, const struct hpclang_rpl_reader *reader);
#ifdef HPCLANG_INSTALLATION_STORE
static int stored_result;
int hpclang_installation_store(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                               uint32_t bytes, uint32_t slot);
#ifdef HPCLANG_INSTALLATION_TEMPLATE_CREATION
#include "rpl-object-store.h"
int hpclang_installation_execution_ready(void);
int hpclang_installation_stage(const struct hpclang_rpl_flash_io *io,
                               const struct hpclang_rpl_flash_recovery_space *space,
                               uint32_t target, uint32_t claim, const uint8_t identity[48],
                               const uint32_t *map);
#endif
#endif

static void plan_placement(const struct hpclang_rpl_flash_io *io,
                           const struct hpclang_rpl_flash_catalog *catalog)
{
    placement_result[0] = (uint32_t)HPCLANG_RPL_FLASH_INVALID;
    placement_result[1] = UINT32_MAX;
    uint32_t first = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!catalog->reader->read_nibble(catalog->reader->context, 0x80716u + i, &nibble))
            return;
        first |= (uint32_t)nibble << (4 * i);
    }
    const volatile uint32_t *l1 = (void *)0x07f00000u;
    struct hpclang_rpl_reader port1;
    if (first < 0x80000u || first > 0xffffbu ||
        !hpclang_rpl_stock_port1_capture(&port1, (void *)0x08020000u, l1[0x80]))
        return;
    const struct hpclang_rpl_recovery_port_view ports[2] = {{catalog->reader, first, 0x100000u},
                                                            {&port1, 0, HPCLANG_RPL_PORT1_NIBBLES}};
    uint32_t slot = UINT32_MAX;
    placement_result[0] = (uint32_t)hpclang_rpl_recovery_installation_plan(
        io, expected_library, EXPECTED_LIBRARY_BYTES, ports, catalog, &slot);
    placement_result[1] = slot;
#if !defined(HPCLANG_INSTALLATION_TEMPLATE_CREATION) || !defined(HPCLANG_INSTALLATION_STORE)
    for (unsigned i = 0; i < EXPECTED_LIBRARY_BYTES; ++i)
        prepared_library[i] = 0xa5;
#endif
    struct hpclang_rpl_flash_recovery_space space = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    const struct hpclang_rpl_flash_recovery_layout layout = {
        expected_library, preparation_bytes, EXPECTED_RECOVERY_BINDING, EXPECTED_ADMISSION_BINDING};
#ifdef HPCLANG_INSTALLATION_TEMPLATE_CREATION
    uint32_t claim = UINT32_MAX;
    preparation_result[0] = (uint32_t)
#ifdef HPCLANG_INSTALLATION_STORE
        hpclang_rpl_recovery_installation_prepare_creation_in_place(
#else
        hpclang_rpl_recovery_installation_prepare_creation(
#endif
            io, &layout, EXPECTED_IDENTITY_NIBBLE, template_creation, ports, catalog,
            EXPECTED_TARGET, prepared_library, EXPECTED_LIBRARY_BYTES, &space, &claim);
    preparation_creation_slot = claim;
#else
    preparation_result[0] = (uint32_t)hpclang_rpl_recovery_installation_prepare(
        io, &layout, EXPECTED_IDENTITY_NIBBLE, ports, catalog, EXPECTED_TARGET, prepared_library,
        EXPECTED_LIBRARY_BYTES, &space);
#endif
    preparation_result[1] = space.library_slot;
    preparation_result[2] = space.journal_slot;
    preparation_result[3] = space.authority_slot;
#ifdef HPCLANG_INSTALLATION_STORE
#ifdef HPCLANG_INSTALLATION_TEMPLATE_CREATION
    if (!preparation_result[0]) {
        uint32_t home;
        struct hpclang_rpl_directory_entry_view entry;
        stored_result = hpclang_rpl_store_home_object(catalog->reader, &home);
        if (!stored_result) {
            int found = hpclang_rpl_directory_find(catalog->reader, home,
                                                   (const uint8_t *)"HPCLEASE", 8, &entry);
            stored_result = found == HPCLANG_RPL_STORE_NOT_FOUND ? 0
                            : found                              ? found
                                                                 : HPCLANG_RPL_FLASH_EXISTS;
        }
        if (stored_result)
            return;
        if (!hpclang_installation_execution_ready()) {
            stored_result = HPCLANG_RPL_FLASH_UNSUPPORTED;
            return;
        }
    }
#endif
    if (!preparation_result[0])
        stored_result = hpclang_installation_store(io, prepared_library, EXPECTED_LIBRARY_BYTES,
                                                   space.library_slot);
#ifdef HPCLANG_INSTALLATION_TEMPLATE_CREATION
    if (!preparation_result[0] && !stored_result) {
        uint8_t identity[48];
        for (unsigned i = 0; i < sizeof identity; ++i) {
            uint32_t at = 8 + EXPECTED_IDENTITY_NIBBLE + 2 * i;
            identity[i] = (prepared_library[at / 2] >> (4 * (at & 1))) & 15;
            ++at;
            identity[i] |= ((prepared_library[at / 2] >> (4 * (at & 1))) & 15) << 4;
        }
        stored_result = hpclang_installation_stage(io, &space, EXPECTED_TARGET, claim, identity,
                                                   catalog->reader->context);
    }
#endif
#endif
}
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
#ifndef HPCLANG_INSTALLATION_CLEANUP
    const struct hpclang_rpl_reader reader = {mapped_nibble, (void *)map};
#ifdef HPCLANG_INSTALLATION_TEMPLATE
    if (!hpclang_installation_load_template(&reader)) {
        hpclang_flash_mapping_swap(descriptors);
        installation_result[0] = 0x48504944;
        installation_result[1] = (uint32_t)-100;
        installation_result[2] = UINT32_MAX;
        installation_result[5] = l1[0];
        installation_result[6] = l1[1];
        return 0;
    }
#endif
#ifdef HPCLANG_INSTALLATION_STARTUP
#if !defined(HPCLANG_INSTALLATION_TEMPLATE_CREATION) || !defined(HPCLANG_INSTALLATION_STORE)
    hpclang_startup_inventory(map, &reader);
#endif
#endif
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 0x8611d, 3 + 128 * 16, 0x26bbb};
#endif
    const struct hpclang_rpl_flash_io io = {descriptors, read_flash, NULL, 0x140000, 0x200000};
#ifdef HPCLANG_INSTALLATION_STARTUP
    plan_placement(&io, &catalog);
#endif
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
#elif defined(HPCLANG_INSTALLATION_CREATION_ADMISSION)
    struct hpclang_rpl_flash_installed_recovery installed;
    int result = hpclang_rpl_recovery_installation_staged(
        &io, EXPECTED_LIBRARY_ID, &catalog, expected_identity, EXPECTED_AUTHORITY_SLOT,
        EXPECTED_JOURNAL_SLOT, &installed);
    staged_creation_result[0] = result;
    if (!result) {
        uint32_t crc = 0;
        for (unsigned i = 0; i < 4; ++i) {
            uint32_t at = EXPECTED_JOURNAL_SLOT + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 4 + i;
            uint16_t word;
            if (!io.read16(io.context, at & ~1u, &word)) {
                result = HPCLANG_RPL_FLASH_FAILED;
                break;
            }
            crc |= (uint32_t)(uint8_t)(word >> (8 * (at & 1))) << (8 * i);
        }
        uint8_t header[80];
        if (!result && hpclang_rpl_recovery_reservation_header(expected_identity, &installed, crc,
                                                               32768, header)) {
            header[72] ^= 1;
            staged_creation_result[1] = hpclang_rpl_recovery_installation_creation_staged(
                &io, EXPECTED_LIBRARY_ID, &catalog, header, 0x1e0105, (const uint8_t *)"HPCLEASE",
                8);
            header[72] ^= 1;
            result = hpclang_rpl_recovery_installation_creation_staged(
                &io, EXPECTED_LIBRARY_ID, &catalog, header, 0x1e0105, (const uint8_t *)"HPCLEASE",
                8);
            staged_creation_result[2] = result;
            if (!result)
                slot = installed.library_slot;
        } else if (!result) {
            result = HPCLANG_RPL_FLASH_INVALID;
        }
    }
#elif defined(HPCLANG_INSTALLATION_PUBLICATION)
    int result = hpclang_installation_publish(map, &io, &catalog, &slot);
#elif defined(HPCLANG_INSTALLATION_CANCELLATION)
    int result = hpclang_installation_cancel(map, &io, &catalog, &slot);
#elif defined(HPCLANG_INSTALLATION_CLEANUP)
    int result = hpclang_installation_cleanup(&io);
#elif defined(HPCLANG_INSTALLATION_STORE)
    int result = (int)preparation_result[0];
    if (!result) {
        result = stored_result;
        slot = preparation_result[1];
    }
#else
    int result = hpclang_rpl_flash_match_installation(&io, expected_library, EXPECTED_LIBRARY_BYTES,
                                                      &catalog, &slot);
#endif
    hpclang_flash_mapping_swap(descriptors);
    installation_result[0] = 0x48504944;
    installation_result[1] = (unsigned)result;
    installation_result[2] = slot;
    installation_result[5] = l1[0];
    installation_result[6] = l1[1];
    return result >= 0;
}
