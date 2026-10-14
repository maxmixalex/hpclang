#include "rpl-flash-write.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static const uint8_t record[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};
static uint8_t flash[6u * 0x20000u];
static unsigned writes;
static uint32_t fail_address;
static unsigned write_failures;
static const uint32_t base = 0x140000u;

static int read16(void *context, uint32_t address, uint16_t *value)
{
    (void)context;
    assert(!(address & 1u) && address >= base && address - base < sizeof flash);
    if (address == fail_address)
        return 0;
    uint32_t at = address - base;
    *value = (uint16_t)(flash[at] | (uint16_t)flash[at + 1u] << 8u);
    return 1;
}

static int program16(void *context, uint32_t address, uint16_t value)
{
    uint16_t old;
    assert(read16(context, address, &old));
    assert((old & value) == value);
    if (write_failures) {
        --write_failures;
        flash[address - base] &= (uint8_t)value;
        ++writes;
        return 0;
    }
    flash[address - base] = (uint8_t)value;
    flash[address - base + 1u] = (uint8_t)(value >> 8u);
    ++writes;
    return 1;
}

static void init(void)
{
    memset(flash, 0xff, sizeof flash);
    for (uint32_t i = 0; i < 6; ++i) {
        flash[i * 0x20000u + 0x100u] = 0xf0;
        flash[i * 0x20000u + 0x101u] = (uint8_t)(i + 8u);
    }
    writes = 0;
    write_failures = 0;
    fail_address = UINT32_MAX;
}

static uint32_t fingerprint(uint32_t end)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < end; ++i)
        hash = (hash ^ flash[i]) * 16777619u;
    return hash;
}

static void fill_bank(uint32_t index)
{
    uint32_t at = index * 0x20000u + 0x105u;
    uint32_t extent = ((0x40000u - 0x20au - 2u) & ~3u) - 2u;
    flash[at] = 0;
    flash[at + 1u] = (uint8_t)extent;
    flash[at + 2u] = (uint8_t)(extent >> 8u);
    flash[at + 3u] = (uint8_t)(0xf0u | (extent >> 16u));
}

int main(void)
{
    const struct hpclang_rpl_flash_io io = {0, read16, program16, base, base + sizeof flash};
    init();
    uint32_t attempted = UINT32_MAX;
    flash[5u * 0x20000u + 0x110u] = 0;
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, &attempted) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(attempted == UINT32_MAX && !writes);
    init();
    write_failures = 100;
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, &attempted) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(attempted == base + 0x105u && writes);
    assert(hpclang_rpl_flash_discard_record(&io, attempted, record, sizeof record) ==
           HPCLANG_RPL_FLASH_DIRTY);
    write_failures = 0;
    assert(!hpclang_rpl_flash_discard_record(&io, attempted, record, sizeof record));
    assert(!hpclang_rpl_flash_create_backup(&io, record, sizeof record, &attempted));
    assert(attempted == base + 0x105u + sizeof record);
    init();
    assert(!hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL));
    assert(!memcmp(flash + 0x105u, record, sizeof record));
    uint32_t before = fingerprint(sizeof flash);
    writes = 0;
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_EXISTS);
    assert(!writes && fingerprint(sizeof flash) == before);

    /* A matching name in the final bank must prevent writes to an empty first bank. */
    init();
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_EXISTS);
    assert(!writes && fingerprint(sizeof flash) == before);

    /* Validation continues through all banks before selecting an earlier free slot. */
    flash[5u * 0x20000u + 0x105u + 30u] ^= 1;
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && fingerprint(sizeof flash) == before);
    for (uint32_t index = 0; index < 6; ++index) {
        init();
        flash[index * 0x20000u + 0x110u] = 0;
        before = fingerprint(sizeof flash);
        assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!writes && fingerprint(sizeof flash) == before);
        init();
        flash[index * 0x20000u + 0x101u] ^= 1;
        assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!writes);
        const uint32_t offsets[] = {0x100u, 0x104u, 0x106u, 0x1fffeu};
        for (unsigned j = 0; j < sizeof offsets / sizeof offsets[0]; ++j) {
            init();
            fail_address = base + index * 0x20000u + offsets[j];
            assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
                   HPCLANG_RPL_FLASH_FAILED);
            assert(!writes);
        }
    }
    init();
    for (uint32_t i = 0; i < 5; ++i)
        fill_bank(i);
    before = fingerprint(5u * 0x20000u);
    assert(!hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL));
    assert(!memcmp(flash + 5u * 0x20000u + 0x105u, record, sizeof record));
    assert(fingerprint(5u * 0x20000u) == before);
    init();
    for (uint32_t i = 0; i < 6; ++i)
        fill_bank(i);
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_NOSPACE);
    assert(!writes && fingerprint(sizeof flash) == before);
    return 0;
}
