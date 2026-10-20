/* Fresh installation fixture only: the harness owns both unpublished slots.
 * This witness does not establish durable production ownership. */
#include "rpl-flash-replay.h"
#include "sst-flash.h"
#include "expected-library.h"
#include <hpclang/time.h>
__thread int errno;
volatile unsigned cancellation_result[3];
static volatile unsigned expected_bytes = sizeof expected_library;
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
static int program(void *c, unsigned a, unsigned short value)
{
    if ((a < (EXPECTED_JOURNAL_SLOT & ~1u) || a >= EXPECTED_JOURNAL_SLOT + 4) &&
        (a < (EXPECTED_AUTHORITY_SLOT & ~1u) || a >= EXPECTED_AUTHORITY_SLOT + 4))
        return 0;
    ++cancellation_result[1];
    const struct hpclang_sst_bus bus = {c, bus_read, bus_write, flash_time};
    const struct hpclang_sst_region region = {0x140000, 0x200000, 4096};
    int status = hpclang_sst_program_word(&bus, &region, a, value, 1000);
    while (status == HPCLANG_SST_BUSY)
        status = hpclang_sst_wait_ready(&bus, a, 100000);
    return status == HPCLANG_SST_OK && hpclang_flash_read16(c, a) == value;
}
int hpclang_installation_cancel(const unsigned *map, const struct hpclang_rpl_flash_io *reader,
                                const struct hpclang_rpl_flash_catalog *catalog, unsigned *slot)
{
    (void)map;
    uint64_t now;
    if (hpclang_time_read_us(&now))
        return -100;
    struct hpclang_rpl_flash_io io = *reader;
    io.program16 = program;
    const struct hpclang_rpl_flash_recovery_layout layout = {
        expected_library, expected_bytes, EXPECTED_RECOVERY_BINDING, EXPECTED_ADMISSION_BINDING};
    int result = hpclang_rpl_flash_cancel_installed(&io, &layout, catalog);
    cancellation_result[0] = (unsigned)result;
    if (result)
        for (;;)
            __asm__ volatile("nop");
    unsigned count = cancellation_result[1];
    result = hpclang_rpl_flash_cancel_installed(&io, &layout, catalog);
    cancellation_result[2] = cancellation_result[1] - count;
    if (result || cancellation_result[2])
        for (;;)
            __asm__ volatile("nop");
    return hpclang_rpl_flash_match_installation(&io, expected_library, expected_bytes, catalog,
                                                slot);
}
