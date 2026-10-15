#include "rpl-flash-write.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static const uint8_t record[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};
static uint8_t flash[6u * 0x20000u], before[sizeof flash];
static const uint32_t base = 0x140000u;
static uint32_t fail_address = UINT32_MAX;
static unsigned fail_crc_read;

static int read16(void *context, uint32_t address, uint16_t *value)
{
    assert(context == flash && !(address & 1u) && address >= base && address - base < sizeof flash);
    if (address == fail_address)
        return 0;
    if (fail_crc_read == 2 && address >= base + 0x104u && address < base + 0x125u)
        return 0;
    if (fail_crc_read && address == base + sizeof flash - 2u)
        fail_crc_read = 2;
    uint32_t at = address - base;
    *value = (uint16_t)(flash[at] | (uint16_t)flash[at + 1u] << 8u);
    return 1;
}

static int program16(void *context, uint32_t address, uint16_t value)
{
    (void)context;
    (void)address;
    (void)value;
    assert(0 && "planning must never program Flash");
    return 0;
}

static void init(void)
{
    memset(flash, 0xff, sizeof flash);
    for (uint32_t bank = 0; bank < 6; ++bank) {
        flash[bank * 0x20000u + 0x100u] = 0xf0;
        flash[bank * 0x20000u + 0x101u] = (uint8_t)(bank + 8u);
    }
    memcpy(flash + 0x105u, record, sizeof record);
    fail_address = UINT32_MAX;
    fail_crc_read = 0;
}

/* Fill with one tombstone after any live prefix, leaving exactly free_bytes
 * of halfword-allocatable tail and the final FF status byte. */
static uint32_t leave_tail(uint32_t bank, uint32_t prefix, uint32_t free_bytes)
{
    uint32_t at = bank * 0x20000u + 0x105u + prefix;
    uint32_t bytes = 0x20000u - 0x105u - prefix - free_bytes - 1u;
    assert(bytes >= 8u && !(bytes & 1u));
    uint32_t extent = bytes * 2u - 2u;
    flash[at] = 0;
    flash[at + 1u] = (uint8_t)extent;
    flash[at + 2u] = (uint8_t)(extent >> 8u);
    flash[at + 3u] = (uint8_t)(0xf0u | (extent >> 16u));
    return base + at + bytes;
}

static void rejected(const struct hpclang_rpl_flash_io *io, int expected)
{
    struct hpclang_rpl_flash_replace_plan plan, saved;
    memset(&plan, 0x5a, sizeof plan);
    memcpy(&saved, &plan, sizeof saved);
    memcpy(before, flash, sizeof flash);
    assert(hpclang_rpl_flash_plan_replace(io, record, sizeof record, &plan) == expected);
    assert(!memcmp(&plan, &saved, sizeof plan));
    assert(!memcmp(flash, before, sizeof flash));
}

int main(void)
{
    const struct hpclang_rpl_flash_io io = {flash, read16, program16, base, base + sizeof flash};
    struct hpclang_rpl_flash_replace_plan plan;
    init();
    memcpy(before, flash, sizeof flash);
    assert(!hpclang_rpl_flash_plan_replace(&io, record, sizeof record, &plan));
    assert(plan.intent.old_slot == base + 0x105u && plan.intent.old_bytes == sizeof record);
    assert(plan.intent_slot == base + 0x105u + sizeof record);
    assert(plan.intent.new_slot == plan.intent_slot + HPCLANG_RPL_FLASH_INTENT_BYTES);
    assert(plan.intent.new_bytes == sizeof record && plan.intent.old_crc == plan.intent.new_crc);
    assert(plan.intent.old_crc == UINT32_C(0xc11cf392)); /* Independent zlib vector. */
    assert(!memcmp(flash, before, sizeof flash));
    uint8_t encoded[HPCLANG_RPL_FLASH_INTENT_BYTES];
    assert(!hpclang_rpl_flash_intent_encode(io.first, io.end, plan.intent_slot, &plan.intent,
                                            encoded, sizeof encoded));
    /* Neither bank can fit both allocations; journal-first greed must still
     * find the pair when the only new-record space is in an earlier bank. */
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
        init();
        for (unsigned bank = 2; bank < 6; ++bank)
            leave_tail(bank, 0, 0);
        uint32_t zero = leave_tail(0, sizeof record, reverse ? sizeof record : sizeof encoded);
        uint32_t one = leave_tail(1, 0, reverse ? sizeof encoded : sizeof record);
        memcpy(before, flash, sizeof flash);
        assert(!hpclang_rpl_flash_plan_replace(&io, record, sizeof record, &plan));
        assert(plan.intent_slot == (reverse ? one : zero));
        assert(plan.intent.new_slot == (reverse ? zero : one));
        assert(!memcmp(flash, before, sizeof flash));
    }
    init();
    for (unsigned bank = 0; bank < 6; ++bank)
        leave_tail(bank, bank ? 0 : sizeof record, bank ? 0 : sizeof encoded + sizeof record - 2u);
    rejected(&io, HPCLANG_RPL_FLASH_NOSPACE);
    init();
    memcpy(flash + 0xa0105u, record, sizeof record);
    rejected(&io, HPCLANG_RPL_FLASH_DIRTY);
    init();
    flash[0x105u] = 0;
    rejected(&io, HPCLANG_RPL_FLASH_NOT_FOUND);
    for (unsigned bank = 0; bank < 6; ++bank) {
        init();
        flash[bank * 0x20000u + 0x101u] = 0;
        rejected(&io, HPCLANG_RPL_FLASH_DIRTY);
        init();
        fail_address = base + bank * 0x20000u + 0x1fffeu;
        rejected(&io, HPCLANG_RPL_FLASH_FAILED);
    }
    init();
    fail_crc_read = 1;
    rejected(&io, HPCLANG_RPL_FLASH_FAILED);
    return 0;
}
