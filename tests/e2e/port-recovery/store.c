/* Fresh library storage witness. The predicate owns RAM execution, mappings
 * and interrupts. The creation variant also stages its planned transaction;
 * neither variant activates it or registers the root in the native catalog. */
#include "rpl-flash-write.h"
#include "sst-flash.h"
#include <hpclang/time.h>

__thread int errno;
volatile uint32_t storage_result[2];
#ifdef HPCLANG_INSTALLATION_TEMPLATE_CREATION
#include "rpl-recovery-reservation.h"
static uint8_t staged_image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
static uint8_t staged_journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static uint8_t staged_claim[HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES];
volatile uint32_t storage_stage_result[3];
#endif
static uint32_t owned_first, owned_end;
unsigned short hpclang_flash_read16(void *, unsigned);
void hpclang_flash_write16(void *, unsigned, unsigned short);

static uint16_t bus_read(void *context, uint32_t address)
{
    return hpclang_flash_read16(context, address);
}

static void bus_write(void *context, uint32_t address, uint16_t value)
{
    hpclang_flash_write16(context, address, value);
}

static uint32_t flash_time(void *context)
{
    (void)context;
    return (uint32_t)hpclang_time_monotonic_us();
}

static int program_owned(void *context, uint32_t address, uint16_t value)
{
    if (address < (owned_first & ~1u) || address >= owned_end)
        return 0;
    const struct hpclang_sst_bus bus = {context, bus_read, bus_write, flash_time};
    const struct hpclang_sst_region region = {0x140000, 0x200000, 4096};
    int status = hpclang_sst_program_word(&bus, &region, address, value, 1000);
    ++storage_result[1];
    while (status == HPCLANG_SST_BUSY)
        status = hpclang_sst_wait_ready(&bus, address, 100000);
    return status == HPCLANG_SST_OK && hpclang_flash_read16(context, address) == value;
}

int hpclang_installation_store(const struct hpclang_rpl_flash_io *source, const uint8_t *record,
                               uint32_t bytes, uint32_t slot)
{
    uint64_t now;
    if (hpclang_time_read_us(&now))
        for (;;)
            __asm__ volatile("nop");
    owned_first = slot;
    owned_end = slot + bytes;
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = program_owned;
    int result = hpclang_rpl_flash_create_library_at(&io, record, bytes, slot);
    storage_result[0] = (uint32_t)result;
    if (result == HPCLANG_RPL_FLASH_DIRTY)
        for (;;)
            __asm__ volatile("nop"); /* Retain ownership of an unverified tail. */
    return result;
}

#ifdef HPCLANG_INSTALLATION_TEMPLATE_CREATION
/* Same invocation and four-extent reservation as the successful root store.
 * Publish in planner order so a later allocation never hides behind an erased
 * marker. Name absence was checked before root storage. The durable intent
 * precedes any RAM creation; no RAM resource or activation is implied here. */
int hpclang_installation_stage(const struct hpclang_rpl_flash_io *source,
                               const struct hpclang_rpl_flash_recovery_space *space,
                               uint32_t target, uint32_t claim, const uint8_t identity[48])
{
    for (uint32_t i = 0; i < sizeof staged_image; i += 2) {
        uint16_t word;
        if (!source->read16(source->context, target + i, &word))
            return HPCLANG_RPL_FLASH_FAILED;
        staged_image[i] = word;
        staged_image[i + 1] = word >> 8;
    }
    int result =
        hpclang_rpl_flash_sector_encode(source->first, source->end, space->journal_slot, target,
                                        staged_image, staged_journal, sizeof staged_journal);
    if (result)
        return result;
    uint32_t crc = 0;
    for (unsigned i = 0; i < 4; ++i)
        crc |= (uint32_t)staged_journal[sizeof staged_journal - 4 + i] << (8 * i);
    const struct hpclang_rpl_flash_installed_recovery installed = {
        space->library_slot, space->journal_slot, space->authority_slot, target};
    uint8_t header[80];
    if (!hpclang_rpl_recovery_reservation_header(identity, &installed, crc, 32768, header) ||
        hpclang_rpl_flash_creation_encode(source->first, source->end, claim, header,
                                          (const uint8_t *)"HPCLEASE", 8, staged_claim,
                                          sizeof staged_claim))
        return HPCLANG_RPL_FLASH_INVALID;
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = program_owned;
    owned_first = space->journal_slot;
    owned_end = owned_first + sizeof staged_journal;
    result =
        hpclang_rpl_flash_publish_sector(&io, owned_first, staged_journal, sizeof staged_journal);
    storage_stage_result[0] = result;
    if (!result) {
        owned_first = space->authority_slot;
        owned_end = owned_first + HPCLANG_RPL_FLASH_AUTHORITY_BYTES;
        result = hpclang_rpl_flash_stage_authority(&io, owned_first, space->journal_slot,
                                                   staged_journal, sizeof staged_journal);
        storage_stage_result[1] = result;
    }
    if (!result) {
        owned_first = claim;
        owned_end = claim + sizeof staged_claim;
        result = hpclang_rpl_flash_publish_creation_intent(&io, claim, staged_claim,
                                                           sizeof staged_claim);
        storage_stage_result[2] = result;
    }
    if (result)
        for (;;)
            __asm__ volatile("nop"); /* Retain this partially published installation. */
    return result;
}
#endif

int hpclang_installation_cleanup(const struct hpclang_rpl_flash_io *source)
{
    uint64_t now;
    if (hpclang_time_read_us(&now))
        for (;;)
            __asm__ volatile("nop");
    owned_first = source->first;
    owned_end = source->end;
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = program_owned;
    int result = hpclang_rpl_flash_recover_tails(&io);
    storage_result[0] = (uint32_t)result;
    if (result)
        for (;;)
            __asm__ volatile("nop");
    return result;
}
