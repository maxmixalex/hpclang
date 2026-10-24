#ifndef HPCLANG_SST_FLASH_H
#define HPCLANG_SST_FLASH_H

#include <stdint.h>

/* Internal SST x16 command engine. Addresses are byte offsets into the chip.
 * The caller identifies the compatible chip, owns an uncached mapping, runs
 * all code/callbacks from RAM and excludes interrupt/other Flash accesses.
 * This includes the time callback: it must not enter a ROM SWI trampoline
 * while the chip is in query or busy mode, even if its own code lives in RAM.
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
    HPCLANG_SST_VERIFY,
    HPCLANG_SST_UNSUPPORTED
};

struct hpclang_sst_device
{
    uint32_t bytes, sector_bytes, block_bytes;
    uint16_t manufacturer, product;
};

/* Probe only on a board already known to use the SST command protocol.
 * Same RAM/mapping/interrupt ownership requirements as programming apply.
 * Always exits identification/query mode; output is cleared on failure.
 * Validates SST39LF/VF160 and SST39VF1601/1602/3201/3202 data sheet families. */
int hpclang_sst_identify(const struct hpclang_sst_bus *bus, struct hpclang_sst_device *device);

/* Continue polling a previously issued operation without sending commands.
 * Ownership must remain held while this returns BUSY. */
int hpclang_sst_wait_ready(const struct hpclang_sst_bus *bus, uint32_t address,
                           uint32_t timeout_us);

int hpclang_sst_program_word(const struct hpclang_sst_bus *bus,
                             const struct hpclang_sst_region *region, uint32_t address,
                             uint16_t value, uint32_t timeout_us);
int hpclang_sst_erase_sector(const struct hpclang_sst_bus *bus,
                             const struct hpclang_sst_region *region, uint32_t address,
                             uint32_t timeout_us);

#endif
