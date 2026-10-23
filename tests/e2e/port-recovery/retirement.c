/* Native claim closure witness; only its bound phase word may be programmed. */
#include "rpl-recovery-installation.h"
#include "sst-flash.h"
#include <hpclang/time.h>

__thread int errno;
volatile uint32_t retirement_close_result[4];
static uint32_t phase_word;
unsigned short hpclang_flash_read16(void *, unsigned);
void hpclang_flash_write16(void *, unsigned, unsigned short);

static uint16_t read_bus(void *context, uint32_t address)
{
    return hpclang_flash_read16(context, address);
}

static void write_bus(void *context, uint32_t address, uint16_t value)
{
    hpclang_flash_write16(context, address, value);
}

static uint32_t clock_us(void *context)
{
    (void)context;
    return (uint32_t)hpclang_time_monotonic_us();
}

static int program_phase(void *context, uint32_t address, uint16_t value)
{
    if (address != phase_word)
        return 0;
    const struct hpclang_sst_bus bus = {context, read_bus, write_bus, clock_us};
    const struct hpclang_sst_region region = {0x140000, 0x200000, 4096};
    int status = hpclang_sst_program_word(&bus, &region, address, value, 1000);
    ++retirement_close_result[2];
    while (status == HPCLANG_SST_BUSY)
        status = hpclang_sst_wait_ready(&bus, address, 100000);
    return status == HPCLANG_SST_OK && read_bus(context, address) == value;
}

int hpclang_installation_retire_close(const struct hpclang_rpl_flash_io *source,
                                      uint32_t library_id,
                                      const struct hpclang_rpl_flash_catalog *catalog,
                                      const uint8_t binding[80], uint32_t claim,
                                      const uint8_t *name, uint32_t length)
{
    uint64_t now;
    if (hpclang_time_read_us(&now))
        return HPCLANG_RPL_FLASH_UNSUPPORTED;
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = program_phase;
    phase_word = (claim + 16u) & ~1u;
    int result = hpclang_rpl_recovery_installation_close_retired_creation(
        &io, library_id, catalog, binding, claim, name, length, catalog->reader);
    retirement_close_result[0] = result;
    if (result)
        return result;
    uint32_t writes = retirement_close_result[2];
    result = hpclang_rpl_recovery_installation_close_retired_creation(
        &io, library_id, catalog, binding, claim, name, length, catalog->reader);
    retirement_close_result[1] = result;
    retirement_close_result[3] = retirement_close_result[2] - writes;
    return result ? result : retirement_close_result[3] ? HPCLANG_RPL_FLASH_DIRTY : 0;
}
