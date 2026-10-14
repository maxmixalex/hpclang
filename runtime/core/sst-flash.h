#ifndef HPCLANG_SST_FLASH_H
#define HPCLANG_SST_FLASH_H

#include <stdint.h>

/* Internal SST x16 command engine. Addresses are byte offsets into the chip.
 * The caller identifies the compatible chip, owns an uncached mapping, runs
 * all code/callbacks from RAM and excludes interrupt/other Flash accesses.
 * Entry requires ordinary array-read mode with no operation in progress.
 * Bounds constrain data programming/erasure; unlock commands use chip offsets.
 * A BUSY result does NOT authorize restoring Stock execution or interrupts:
 * the owner must first establish that the chip has returned to array-read mode.
 * In particular, sending F0 is not a supported way to abort an SST erase. */
struct hpclang_sst_bus
{
    void *context;
    uint16_t (*read16)(void *context, uint32_t address);
    void (*write16)(void *context, uint32_t address, uint16_t value);
    uint32_t (*time_us)(void *context);
};

struct hpclang_sst_region
{
    uint32_t first, end, sector_bytes;
};

enum hpclang_sst_result
{
    HPCLANG_SST_OK,
    HPCLANG_SST_INVALID,
    HPCLANG_SST_NEEDS_ERASE,
    HPCLANG_SST_BUSY,
    HPCLANG_SST_VERIFY
};

int hpclang_sst_program_word(const struct hpclang_sst_bus *bus,
                             const struct hpclang_sst_region *region, uint32_t address,
                             uint16_t value, uint32_t timeout_us);
int hpclang_sst_erase_sector(const struct hpclang_sst_bus *bus,
                             const struct hpclang_sst_region *region, uint32_t address,
                             uint32_t timeout_us);

#endif
