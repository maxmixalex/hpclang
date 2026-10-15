#include "rpl-flash-replay.h"
#include <assert.h>
#include <string.h>

enum
{
    FIRST = 0x140000,
    END = 0x200000,
    SLOT = 0x160105,
    SECTOR = 4096
};
static uint8_t flash[END - FIRST], original[END - FIRST];
static uint8_t desired[SECTOR], scratch[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static unsigned reads, programs, erases, fail_read, fail_program, erase_prefix;
static int erase_error, silent_program;

static int read16(void *context, uint32_t address, uint16_t *value)
{
    assert(context == flash && !(address & 1u) && address >= FIRST && address + 1 < END);
    if (++reads == fail_read)
        return 0;
    *value = (uint16_t)(flash[address - FIRST] | (uint16_t)flash[address - FIRST + 1] << 8u);
    return 1;
}

static int program16(void *context, uint32_t address, uint16_t value)
{
    assert(context == flash && !(address & 1u) && address >= FIRST && address < FIRST + SECTOR);
    ++programs;
    if (silent_program)
        return 1;
    uint32_t offset = address - FIRST;
    /* A failed pulse may clear only some of the intended bits. */
    flash[offset] &= (uint8_t)value;
    if (programs == fail_program)
        return 0;
    flash[offset + 1] &= (uint8_t)(value >> 8u);
    return 1;
}

static int erase(void *context, uint32_t address)
{
    assert(context == flash && address == FIRST);
    ++erases;
    memset(flash, 255, erase_prefix);
    return !erase_error;
}

static const struct hpclang_rpl_flash_replay_io io = {{flash, read16, program16, FIRST, END},
                                                      erase};

static void reset_counters(void)
{
    reads = programs = erases = fail_read = fail_program = 0;
    erase_prefix = SECTOR;
    erase_error = silent_program = 0;
    /* Retry must reconstruct its entire image from durable Flash. */
    memset(scratch, 0xa5, sizeof scratch);
}

static void prepare(void)
{
    memcpy(flash, original, sizeof flash);
    reset_counters();
}

static int replay(void)
{
    int result = hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch);
    /* Journal and all unrelated sectors must survive every failure and retry. */
    assert(!memcmp(flash + SECTOR, original + SECTOR, sizeof flash - SECTOR));
    return result;
}

static void retry(void)
{
    reset_counters();
    assert(replay() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(!memcmp(flash, desired, SECTOR));
    reset_counters();
    assert(replay() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(!programs && !erases);
}

int main(void)
{
    for (unsigned i = 0; i < sizeof original; ++i)
        original[i] = (uint8_t)(i * 31u + 11u);
    for (unsigned i = 0; i < SECTOR; ++i)
        desired[i] = (uint8_t)(i * 73u);
    /* Include erased words: these must not need programming. */
    memset(desired + 128, 255, 128);
    assert(!hpclang_rpl_flash_sector_encode(FIRST, END, SLOT, FIRST, desired,
                                            original + SLOT - FIRST,
                                            HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES));
    prepare();
    assert(replay() == HPCLANG_RPL_FLASH_WRITTEN);
    unsigned total_reads = reads, total_programs = programs;
    assert(erases == 1 && total_programs == (SECTOR - 128) / 2);
    retry();

    /* Cut every read, including initial journal reads, post-erase blank checks
     * and final verification. Resume with no surviving scratch state. */
    for (unsigned cut = 1; cut <= total_reads; ++cut) {
        prepare();
        fail_read = cut;
        assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
        retry();
    }
    for (unsigned cut = 1; cut <= total_programs; ++cut) {
        prepare();
        fail_program = cut;
        assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
        retry();
    }
    for (unsigned cut = 0; cut <= SECTOR; ++cut) {
        prepare();
        erase_prefix = cut;
        erase_error = 1;
        assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs);
        retry();
    }
    prepare();
    erase_prefix = SECTOR - 1;
    assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs);
    retry();
    prepare();
    silent_program = 1;
    assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
    retry();

    /* No invalid or unpublished journal authorizes an erase. */
    for (unsigned i = 0; i < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES; ++i) {
        prepare();
        flash[SLOT - FIRST + i] ^= 1;
        assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(!erases && !programs && !memcmp(flash, original, SECTOR));
    }
    for (unsigned status = 0; status < 256; ++status) {
        if (status == 0x7e)
            continue;
        prepare();
        flash[SLOT - FIRST] = (uint8_t)status;
        assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(!erases && !programs);
    }
    prepare();
    assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch - 1) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(!reads && !programs && !erases);
    return 0;
}
