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
#include "rpl-stock-memory.h"
#include "template.h"
static uint8_t staged_image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
static uint8_t staged_journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static uint8_t staged_claim[HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES];
volatile uint32_t storage_stage_result[6];
struct resource_snapshot_layout
{
    uint32_t kind, nibble_count, native_prologue;
    uint8_t data[32768 + 5];
};
static struct hpclang_rpl_recovery_reservation resource_receipt;
static uint32_t execution_first, execution_end;

struct execution_check
{
    uint32_t first, end, matches;
};

static int execution_allocation(void *context, uint32_t first, uint32_t nibbles)
{
    struct execution_check *check = context;
    if (first <= check->first && check->end - first <= nibbles)
        ++check->matches;
    return 0;
}

/* This witness mutates USEROB only. Its entire live image, including stack and
 * snapshots, must belong to one stable TEMP allocation before Flash is touched.
 * The native interpreter owns that allocation until this invocation returns. */
void hpclang_installation_execution_capture(const struct hpclang_rpl_mutation_io *io,
                                            const struct hpclang_rpl_stock_memory *memory,
                                            uintptr_t first, uintptr_t end)
{
    execution_first = execution_end = 0;
    if (first >= end || first < 0x08000000u || end > 0x08080000u)
        return;
    unsigned matches = 0;
    struct execution_check check = {0};
    for (unsigned i = 0; i < HPCLANG_RPL_MAIN_PAGES; ++i) {
        uintptr_t page = memory->pages[i];
        if (first < page || first - page >= 2048u)
            continue;
        ++matches;
        check.first = 0x80000u + i * 4096u + 2u * (first - page);
        check.end = check.first + 2u * (end - first);
    }
    if (matches != 1 || check.end > 0x100000u)
        return;
    for (uint32_t at = check.first; at < check.end;) {
        uintptr_t physical = memory->pages[(at - 0x80000u) / 4096u] + (at & 4095u) / 2u;
        if (physical != first + (at - check.first) / 2u)
            return;
        at = (at & ~4095u) + 4096u;
    }
    if (hpclang_rpl_mutation_visit_temp(io, execution_allocation, &check) || check.matches != 1)
        return;
    execution_first = check.first;
    execution_end = check.end;
}

int hpclang_installation_execution_ready(void)
{
    return execution_first && execution_end;
}
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
#ifdef HPCLANG_INSTALLATION_RESOURCE_CANCEL
#include "rpl-flash-replay.h"
volatile uint32_t storage_cleanup_result[6];
static uint32_t cleanup_journal, cleanup_authority;

static int program_cleanup(void *context, uint32_t address, uint16_t value)
{
    if (address >= (cleanup_journal & ~1u) &&
        address < cleanup_journal + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) {
        owned_first = cleanup_journal;
        owned_end = owned_first + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES;
    } else if (address >= (cleanup_authority & ~1u) &&
               address < cleanup_authority + HPCLANG_RPL_FLASH_AUTHORITY_BYTES) {
        owned_first = cleanup_authority;
        owned_end = owned_first + HPCLANG_RPL_FLASH_AUTHORITY_BYTES;
    } else {
        return 0;
    }
    return program_owned(context, address, value);
}
#endif
static int resource_read(void *context, uint32_t at, uint8_t *value)
{
    return hpclang_rpl_stock_memory_read(context, at, value);
}

static int resource_write(void *context, uint32_t at, uint8_t value)
{
    return hpclang_rpl_stock_memory_write(context, at, value);
}

__attribute__((noinline)) static int
create_resource(const uint32_t *map, const struct hpclang_rpl_flash_io *flash, uint32_t claim,
                const struct hpclang_rpl_flash_recovery_space *space)
{
    /* The source copy is dead after sealing/storing prepared_library. Reuse its
     * owned RAM instead of adding another 32 KiB to the native CODE object. */
    struct resource_snapshot_layout *snapshot_storage = (void *)expected_library;
    if (preparation_bytes < sizeof *snapshot_storage)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    struct hpclang_rpl_stock_memory memory;
    const volatile uint32_t *l1 = (void *)0x07f00000u;
    if (!hpclang_rpl_stock_memory_capture(&memory, map, l1[0x80]))
        return HPCLANG_RPL_MUTATION_IO;
    struct hpclang_rpl_mutation_io io = {.read_nibble = resource_read,
                                         .write_nibble = resource_write,
                                         .context = &memory,
                                         .temp_limit = execution_first,
                                         .user_limit = execution_end,
                                         .library_table = 0x8611d};
    snapshot_storage->kind = HPCLANG_OBJECT_STRING;
    snapshot_storage->nibble_count = 10 + 2 * 32768;
    snapshot_storage->native_prologue = 0x2a2c;
    uint64_t header = 0x2a2cu | (uint64_t)(5 + 2 * 32768) << 20;
    for (unsigned i = 0; i < 5; ++i)
        snapshot_storage->data[i] = header >> (8 * i);
    for (unsigned i = 0; i < 32768; ++i)
        snapshot_storage->data[5 + i] = i < 80 ? staged_claim[20 + i] : (uint8_t)(i * 37 + 11);
    const struct hpclang_object *snapshot = (const struct hpclang_object *)snapshot_storage;
    int result = hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"HPCLEASE", 8,
                                                         snapshot, &resource_receipt);
    storage_stage_result[3] = result;
    if (result)
        return result;
    const struct hpclang_rpl_reader reader = {resource_read, &memory};
    result = hpclang_rpl_recovery_reservation_complete_creation(
        flash, claim, staged_claim, sizeof staged_claim, &reader, snapshot, &resource_receipt);
    storage_stage_result[4] = result;
    if (result)
        return result;
    unsigned operations = storage_result[1];
    result = hpclang_rpl_recovery_reservation_complete_creation(
        flash, claim, staged_claim, sizeof staged_claim, &reader, snapshot, &resource_receipt);
    storage_stage_result[5] = result;
    if (result || storage_result[1] != operations)
        return result ? result : HPCLANG_RPL_FLASH_DIRTY;
#ifdef HPCLANG_INSTALLATION_RESOURCE_CANCEL
    cleanup_journal = space->journal_slot;
    cleanup_authority = space->authority_slot;
    struct hpclang_rpl_flash_io cancellation = *flash;
    cancellation.program16 = program_cleanup;
    result = hpclang_rpl_flash_cancel_staged_pair(&cancellation, cleanup_journal, cleanup_authority,
                                                  staged_journal, sizeof staged_journal);
    storage_cleanup_result[0] = result;
    if (result)
        return result;
    operations = storage_result[1];
    result = hpclang_rpl_flash_cancel_staged_pair(&cancellation, cleanup_journal, cleanup_authority,
                                                  staged_journal, sizeof staged_journal);
    storage_cleanup_result[1] = storage_result[1] - operations;
    if (result || storage_cleanup_result[1])
        return result ? result : HPCLANG_RPL_FLASH_DIRTY;
    result = hpclang_rpl_recovery_reservation_discard_created(&io, (const uint8_t *)"HPCLEASE", 8,
                                                              snapshot, &resource_receipt);
    storage_cleanup_result[2] = result;
    if (result)
        return result;
    owned_first = claim;
    owned_end = claim + sizeof staged_claim;
    result = hpclang_rpl_flash_close_creation(flash, claim, staged_claim, sizeof staged_claim);
    storage_cleanup_result[3] = result;
    if (result)
        return result;
    operations = storage_result[1];
    result = hpclang_rpl_flash_close_creation(flash, claim, staged_claim, sizeof staged_claim);
    storage_cleanup_result[4] = result;
    storage_cleanup_result[5] = storage_result[1] - operations;
    return result ? result : storage_cleanup_result[5] ? HPCLANG_RPL_FLASH_DIRTY : 0;
#else
    (void)space;
    return 0;
#endif
}

/* Same invocation and four-extent reservation as the successful root store.
 * Publish in planner order so a later allocation never hides behind an erased
 * marker. Name absence was checked before root storage. The durable intent
 * precedes RAM creation and receipt-gated completion; authority stays unarmed. */
int hpclang_installation_stage(const struct hpclang_rpl_flash_io *source,
                               const struct hpclang_rpl_flash_recovery_space *space,
                               uint32_t target, uint32_t claim, const uint8_t identity[48],
                               const uint32_t *map)
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
    if (!result)
        result = create_resource(map, &io, claim, space);
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
