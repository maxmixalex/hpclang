/* Fresh library storage witness. The predicate owns RAM execution, mappings
 * and interrupts. This does not publish a recovery transaction or register it. */
#include "rpl-flash-write.h"
#include "sst-flash.h"
#include <hpclang/time.h>

__thread int errno;
volatile uint32_t storage_result[2];
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
