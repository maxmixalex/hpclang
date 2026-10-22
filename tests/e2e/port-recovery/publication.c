/* Native publication witness, not a production installer. The fresh fixture
 * owns both allocations and keeps its original no-op target image in RAM.
 * It exercises driver-level partial programming, not a power loss. */
#include "rpl-flash-replay.h"
#include "rpl-stock-memory.h"
#include "rpl-recovery-reservation.h"
#include "sst-flash.h"
#include "expected-library.h"
#include <hpclang/time.h>
#include <stddef.h>

__thread int errno;
static uint8_t planned_image[HPCLANG_RPL_FLASH_SECTOR_BYTES],
    planned_journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES],
    checked_transaction[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static volatile uint32_t planned_library_bytes = sizeof expected_library;

/* Freestanding witness support; all overlapping copies stay in owned RAM. */
void *memmove(void *destination, const void *source, size_t count)
{
    volatile uint8_t *out = destination;
    const volatile uint8_t *in = source;
    if ((uintptr_t)out < (uintptr_t)in) {
        for (size_t i = 0; i < count; ++i)
            out[i] = in[i];
    } else {
        while (count) {
            --count;
            out[count] = in[count];
        }
    }
    return destination;
}
#define CAPACITY 32768u
static struct
{
    uint32_t kind, nibble_count, native_prologue;
    uint8_t data[CAPACITY + 5];
} reservation;
static struct hpclang_rpl_recovery_reservation created_reservation;
volatile uint32_t reservation_result[2];
static unsigned reservation_writes;
__attribute__((noinline, noreturn)) static void reservation_failed(void)
{
    for (;;)
        __asm__ volatile("nop");
}
static int stock_read(void *context, uint32_t address, uint8_t *value)
{
    return hpclang_rpl_stock_memory_read(context, address, value);
}
static int stock_write(void *context, uint32_t address, uint8_t value)
{
    ++reservation_writes;
    reservation_result[1] = reservation_writes;
    int result = hpclang_rpl_stock_memory_write(context, address, value);
#ifdef HPCLANG_RESERVATION_FAIL_WRITE
    /* A lost acknowledgement may follow an actual native RAM write. */
    if (reservation_writes == HPCLANG_RESERVATION_FAIL_WRITE)
        return 0;
#endif
    return result;
}
static void set_word(uint32_t at, uint32_t value, unsigned count)
{
    for (unsigned i = 0; i < count; ++i, ++at) {
        unsigned shift = 4 * (at & 1);
        reservation.data[at / 2] =
            (reservation.data[at / 2] & ~(15u << shift)) | ((value & 15) << shift);
        value >>= 4;
    }
}
static int create_reservation(const uint32_t *map,
                              const struct hpclang_rpl_flash_installed_recovery *planned)
{
    struct hpclang_rpl_stock_memory memory;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if (!hpclang_rpl_stock_memory_capture(&memory, map, l1[0x80]))
        return -100;
    struct hpclang_rpl_mutation_io io = {.read_nibble = stock_read,
                                         .write_nibble = stock_write,
                                         .context = &memory,
                                         .temp_limit = 0x100000,
                                         .user_limit = 0x80000,
                                         .library_table = 0x8611d};
    reservation.kind = HPCLANG_OBJECT_STRING;
    reservation.native_prologue = 0x2a2c;
    reservation.nibble_count = 10 + 2 * CAPACITY;
    set_word(0, 0x2a2c, 5);
    set_word(5, 5 + 2 * CAPACITY, 5);
    for (unsigned i = 0; i < CAPACITY; ++i)
        reservation.data[5 + i] = (uint8_t)(i * 37 + 11);
    uint32_t crc = 0;
    for (unsigned i = 0; i < 4; ++i)
        crc |= (uint32_t)planned_journal[sizeof planned_journal - 4 + i] << (8u * i);
    if (!hpclang_rpl_recovery_reservation_header(expected_identity, planned, crc, CAPACITY,
                                                 reservation.data + 5))
        return HPCLANG_RPL_MUTATION_MALFORMED;
    return hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"HPCLEASE", 8,
                                                   (const struct hpclang_object *)&reservation,
                                                   &created_reservation);
}

volatile uint32_t publication_result[12];
volatile uint32_t activation_result[3];

__attribute__((noinline)) static int check_reservation(const uint32_t *map)
{
    struct hpclang_rpl_stock_memory memory;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if (!hpclang_rpl_stock_memory_capture(&memory, map, l1[0x80]))
        return HPCLANG_RPL_MUTATION_IO;
    const struct hpclang_rpl_reader reader = {stock_read, &memory};
    return hpclang_rpl_recovery_reservation_check_created(
        &reader, (const uint8_t *)"HPCLEASE", 8, (const struct hpclang_object *)&reservation,
        &created_reservation);
}

static unsigned publication_calls, cut_at;
unsigned short hpclang_flash_read16(void *, unsigned);
void hpclang_flash_write16(void *, unsigned, unsigned short);
static unsigned short bus_read(void *c, unsigned a)
{
    return hpclang_flash_read16(c, a);
}
static void bus_write(void *c, unsigned a, unsigned short v)
{
    hpclang_flash_write16(c, a, v);
}
static unsigned flash_time(void *c)
{
    (void)c;
    return (unsigned)hpclang_time_monotonic_us();
}
static int program_publication(void *c, uint32_t a, uint16_t value)
{
    if ((a < (EXPECTED_JOURNAL_SLOT & ~1u) ||
         a >= EXPECTED_JOURNAL_SLOT + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) &&
        (a < (EXPECTED_AUTHORITY_SLOT & ~1u) ||
         a >= EXPECTED_AUTHORITY_SLOT + HPCLANG_RPL_FLASH_AUTHORITY_BYTES))
        return 0;
    unsigned cut = ++publication_calls == cut_at;
    uint16_t actual = cut ? (uint16_t)(value | 0xaaaau) : value;
    const struct hpclang_sst_bus bus = {c, bus_read, bus_write, flash_time};
    const struct hpclang_sst_region region = {0x140000, 0x200000, 4096};
    int status = hpclang_sst_program_word(&bus, &region, a, actual, 1000);
    while (status == HPCLANG_SST_BUSY)
        status = hpclang_sst_wait_ready(&bus, a, 100000);
    uint16_t observed = hpclang_flash_read16(c, a);
    if (cut && status == HPCLANG_SST_OK && observed == actual && actual != value)
        ++publication_result[11];
    return !cut && status == HPCLANG_SST_OK && observed == value;
}
static void publish_owned(const uint32_t *map, const struct hpclang_rpl_flash_io *reader,
                          const struct hpclang_rpl_flash_recovery_layout *layout,
                          const struct hpclang_rpl_flash_catalog *catalog)
{
    uint64_t now;
    if (hpclang_time_read_us(&now))
        for (;;)
            __asm__ volatile("nop");
    struct hpclang_rpl_flash_io io = *reader;
    io.program16 = program_publication;
    struct hpclang_rpl_flash_installed_recovery installed;
    cut_at = 3;
    publication_result[0] = hpclang_rpl_flash_publish_sector(
        &io, EXPECTED_JOURNAL_SLOT, planned_journal, sizeof planned_journal);
    publication_result[1] = hpclang_rpl_flash_finish_sector(
        &io, EXPECTED_JOURNAL_SLOT, planned_journal, sizeof planned_journal);
    unsigned count = publication_calls;
    int repeat = hpclang_rpl_flash_finish_sector(&io, EXPECTED_JOURNAL_SLOT, planned_journal,
                                                 sizeof planned_journal);
    publication_result[2] = publication_calls - count;
    cut_at = publication_calls + 3;
    publication_result[3] =
        hpclang_rpl_flash_stage_authority(&io, EXPECTED_AUTHORITY_SLOT, EXPECTED_JOURNAL_SLOT,
                                          planned_journal, sizeof planned_journal);
    activation_result[0] = hpclang_rpl_flash_finish_staged_authority(
        &io, EXPECTED_AUTHORITY_SLOT, EXPECTED_JOURNAL_SLOT, planned_journal,
        sizeof planned_journal);
    activation_result[1] = hpclang_rpl_flash_preflight_staged_installed(
        &io, layout, catalog, checked_transaction, sizeof checked_transaction, &installed);
#ifdef HPCLANG_ACTIVATION_REJECT
    /* A valid staged Flash transaction must not hide a lost creation receipt. */
    ++created_reservation.capacity;
#endif
    activation_result[2] = check_reservation(map);
    /* Staging is still unarmed. Only this invocation's unchanged fresh receipt,
     * together with the exact installed staged transaction, permits activation.
     * This witness does not reconstruct that receipt after a restart. */
    if (activation_result[0] || activation_result[1] || activation_result[2])
        reservation_failed();
    publication_result[4] =
        hpclang_rpl_flash_finish_authority(&io, EXPECTED_AUTHORITY_SLOT, EXPECTED_JOURNAL_SLOT,
                                           planned_journal, sizeof planned_journal);
    publication_result[5] = hpclang_rpl_flash_preflight_installed(
        &io, layout, catalog, checked_transaction, sizeof checked_transaction, &installed);
    publication_result[6] = hpclang_rpl_flash_retire_sector(
        &io, EXPECTED_JOURNAL_SLOT, planned_journal, sizeof planned_journal);
    publication_result[7] =
        hpclang_rpl_flash_close_authority(&io, EXPECTED_AUTHORITY_SLOT, EXPECTED_JOURNAL_SLOT,
                                          planned_journal, sizeof planned_journal);
    publication_result[8] = hpclang_rpl_flash_preflight_installed(
        &io, layout, catalog, checked_transaction, sizeof checked_transaction, &installed);
    publication_result[9] = publication_calls;
    if (publication_result[0] != (uint32_t)-4 || publication_result[1] || repeat ||
        publication_result[2] || publication_result[3] != (uint32_t)-4 || publication_result[4] ||
        publication_result[5] != 1 || publication_result[6] || publication_result[7] ||
        publication_result[8] || publication_result[11] != 2)
        for (;;)
            __asm__ volatile("nop");
}

#ifdef HPCLANG_INSTALLATION_STAGING
static void stage_owned(const struct hpclang_rpl_flash_io *reader,
                        const struct hpclang_rpl_flash_recovery_layout *layout,
                        const struct hpclang_rpl_flash_catalog *catalog)
{
    uint64_t now;
    if (hpclang_time_read_us(&now))
        for (;;)
            __asm__ volatile("nop");
    struct hpclang_rpl_flash_io io = *reader;
    io.program16 = program_publication;
    publication_result[0] = hpclang_rpl_flash_publish_sector(
        &io, EXPECTED_JOURNAL_SLOT, planned_journal, sizeof planned_journal);
    publication_result[1] =
        hpclang_rpl_flash_stage_authority(&io, EXPECTED_AUTHORITY_SLOT, EXPECTED_JOURNAL_SLOT,
                                          planned_journal, sizeof planned_journal);
    struct hpclang_rpl_flash_installed_recovery installed;
    publication_result[2] = hpclang_rpl_flash_preflight_installed(
        &io, layout, catalog, checked_transaction, sizeof checked_transaction, &installed);
    installed = (struct hpclang_rpl_flash_installed_recovery){0};
    publication_result[6] = hpclang_rpl_flash_preflight_staged_installed(
        &io, layout, catalog, checked_transaction, sizeof checked_transaction, &installed);
    publication_result[7] = installed.library_slot;
    publication_result[8] = installed.journal_slot;
    publication_result[9] = installed.authority_slot;
    publication_result[10] = installed.target;
    unsigned count = publication_calls;
    int repeat = hpclang_rpl_flash_finish_staged_authority(&io, EXPECTED_AUTHORITY_SLOT,
                                                           EXPECTED_JOURNAL_SLOT, planned_journal,
                                                           sizeof planned_journal);
    publication_result[3] = publication_calls - count;
    publication_result[4] = publication_calls;
    uint16_t word;
    if (!io.read16(io.context, EXPECTED_AUTHORITY_SLOT & ~1u, &word))
        for (;;)
            __asm__ volatile("nop");
    publication_result[5] = (word >> (8u * (EXPECTED_AUTHORITY_SLOT & 1u))) & 255u;
    if (publication_result[0] || publication_result[1] ||
        publication_result[2] != (uint32_t)HPCLANG_RPL_FLASH_DIRTY || repeat ||
        publication_result[3] || publication_result[5] != 0x7fu || publication_result[6])
        for (;;)
            __asm__ volatile("nop");
}
#endif

#ifdef HPCLANG_INSTALLATION_ABORT
static int discard_reservation(const uint32_t *map)
{
    struct hpclang_rpl_stock_memory memory;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if (!hpclang_rpl_stock_memory_capture(&memory, map, l1[0x80]))
        return HPCLANG_RPL_MUTATION_IO;
    struct hpclang_rpl_mutation_io io = {.read_nibble = stock_read,
                                         .write_nibble = stock_write,
                                         .context = &memory,
                                         .temp_limit = 0x100000,
                                         .user_limit = 0x80000,
                                         .library_table = 0x8611d};
    return hpclang_rpl_recovery_reservation_discard_created(
        &io, (const uint8_t *)"HPCLEASE", 8, (const struct hpclang_object *)&reservation,
        &created_reservation);
}

static void abort_owned(const uint32_t *map, const struct hpclang_rpl_flash_io *reader,
                        const struct hpclang_rpl_flash_recovery_layout *layout,
                        const struct hpclang_rpl_flash_catalog *catalog)
{
    uint64_t now;
    if (hpclang_time_read_us(&now))
        reservation_failed();
    struct hpclang_rpl_flash_io io = *reader;
    io.program16 = program_publication;
    publication_result[0] = hpclang_rpl_flash_publish_sector(
        &io, EXPECTED_JOURNAL_SLOT, planned_journal, sizeof planned_journal);
    publication_result[1] =
        hpclang_rpl_flash_stage_authority(&io, EXPECTED_AUTHORITY_SLOT, EXPECTED_JOURNAL_SLOT,
                                          planned_journal, sizeof planned_journal);
    if (publication_result[0] || publication_result[1])
        reservation_failed();
    publication_result[2] = hpclang_rpl_flash_cancel_installed(&io, layout, catalog);
    if (publication_result[2])
        reservation_failed();
    publication_result[3] = discard_reservation(map);
    if (publication_result[3])
        reservation_failed();
    publication_result[4] = publication_calls;
    publication_result[5] = created_reservation.object;
    publication_result[6] = created_reservation.capacity;
}
#endif

int hpclang_installation_publish(const uint32_t *map, const struct hpclang_rpl_flash_io *io,
                                 const struct hpclang_rpl_flash_catalog *catalog, uint32_t *slot)
{
    const struct hpclang_rpl_flash_recovery_layout layout = {
        expected_library, planned_library_bytes, EXPECTED_RECOVERY_BINDING,
        EXPECTED_ADMISSION_BINDING};
    struct hpclang_rpl_flash_installed_recovery planned;
    if (hpclang_rpl_flash_preflight_installed(io, &layout, catalog, checked_transaction,
                                              sizeof checked_transaction, &planned) == 0) {
        *slot = planned.library_slot;
        publication_result[10] = 1;
        return 0;
    }
    /* The intended target is unchanged. Capture it once before any writes;
     * the host oracle independently compares every resulting Flash byte. */
    for (uint32_t i = 0; i < sizeof planned_image; i += 2) {
        uint16_t word;
        if (!io->read16(io->context, EXPECTED_TARGET + i, &word))
            return HPCLANG_RPL_FLASH_FAILED;
        planned_image[i] = (uint8_t)word;
        planned_image[i + 1] = (uint8_t)(word >> 8);
    }
    int result =
        hpclang_rpl_flash_sector_encode(io->first, io->end, EXPECTED_JOURNAL_SLOT, EXPECTED_TARGET,
                                        planned_image, planned_journal, sizeof planned_journal);
    if (result)
        return result;
    result = hpclang_rpl_flash_plan_installed(io, &layout, catalog, planned_journal,
                                              sizeof planned_journal, &planned);
    if (result)
        return result;
#ifdef HPCLANG_INSTALLATION_STAGING
    stage_owned(io, &layout, catalog);
    *slot = planned.library_slot;
    return 0;
#endif
    result = create_reservation(map, &planned);
    reservation_result[0] = (uint32_t)result;
    if (result == HPCLANG_RPL_MUTATION_INCOMPLETE) {
        /* The directory/arena may already have changed. Do not publish Flash
         * or resume Stock on an ordinary negative predicate result. This is
         * retention of the failed invocation, not rollback or restart recovery. */
        reservation_failed();
    }
    if (result)
        return result;
#ifdef HPCLANG_INSTALLATION_ABORT
    abort_owned(map, io, &layout, catalog);
#else
    publish_owned(map, io, &layout, catalog);
#endif
    *slot = planned.library_slot;
    return 0;
}
