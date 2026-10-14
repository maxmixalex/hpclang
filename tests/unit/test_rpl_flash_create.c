#include "rpl-flash-write.h"
#include "rpl-flash-record.h"
#include "rpl-backup.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t record[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};
static uint8_t flash[6u * 0x20000u];
static unsigned writes;
static uint32_t fail_address;
static unsigned write_failures;
static unsigned fail_write_at;
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
    if (write_failures || writes == fail_write_at) {
        if (write_failures)
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
    fail_write_at = UINT32_MAX;
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

static int string_nibble(void *context, uint32_t address, uint8_t *value)
{
    uint32_t length = *(uint32_t *)context;
    if (address >= 10u + length * 2u)
        return 0;
    *value = address < 5u    ? (0x02a2cu >> (4u * address)) & 15u
             : address < 10u ? ((5u + length * 2u) >> (4u * (address - 5u))) & 15u
                             : (0x78u >> (4u * (address & 1u))) & 15u;
    return 1;
}

static int packed_nibble(void *context, uint32_t address, uint8_t *value)
{
    const uint8_t *data = context;
    *value = (data[address / 2u] >> (4u * (address & 1u))) & 15u;
    return 1;
}

static uint8_t *string_record(uint32_t length, uint32_t *bytes)
{
    const struct hpclang_rpl_reader reader = {string_nibble, &length};
    uint8_t *backup, *result;
    uint32_t span;
    assert(!hpclang_rpl_backup_encode(&reader, 0, 10u + length * 2u, (const uint8_t *)"FLASHONE", 8,
                                      &backup, &span));
    const struct hpclang_rpl_reader packed = {packed_nibble, backup};
    assert(!hpclang_rpl_flash_backup_record(&packed, 0, span, &result, bytes));
    free(backup);
    return result;
}

static void replace_backups(const struct hpclang_rpl_flash_io *io)
{
    struct hpclang_rpl_flash_replacement transaction = {UINT32_MAX, UINT32_MAX, 0};
    init();
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_NOT_FOUND);
    assert(!writes && transaction.new_slot == UINT32_MAX);
    memcpy(flash + 0x105u, record, sizeof record);
    assert(!hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction));
    unsigned complete_writes = writes;
    assert(transaction.committed && transaction.old_slot == base + 0x105u &&
           transaction.new_slot == base + 0x105u + sizeof record);
    assert(flash[0x105u] == 0 && !memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
    assert(!memcmp(flash + 0x105u + sizeof record, record, sizeof record));
    for (unsigned cut = 0; cut < complete_writes; ++cut) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        fail_write_at = cut;
        int result = hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction);
        if (transaction.committed) {
            assert(result == HPCLANG_RPL_FLASH_DIRTY);
            assert(!hpclang_rpl_flash_finish_remove(io, transaction.old_slot));
            assert(flash[0x105u] == 0);
            assert(!memcmp(flash + 0x105u + sizeof record, record, sizeof record));
        } else {
            assert(result == HPCLANG_RPL_FLASH_FAILED);
            assert(!memcmp(flash + 0x105u, record, sizeof record));
            assert(flash[0x105u + sizeof record] == 0);
        }
        assert(!memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
    }
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    write_failures = 100;
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!transaction.committed && !memcmp(flash + 0x105u, record, sizeof record));
    write_failures = 0;
    assert(!hpclang_rpl_flash_discard_record(io, transaction.new_slot, record, sizeof record));
    /* Old and new records may reside in different native banks. */
    init();
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    assert(!hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction));
    assert(transaction.new_slot == base + 0x105u);
    assert(transaction.old_slot == base + 5u * 0x20000u + 0x105u);
    assert(flash[5u * 0x20000u + 0x105u] == 0);
    assert(!memcmp(flash + 0x105u, record, sizeof record));
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    transaction.new_slot = UINT32_MAX;
    uint32_t before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && transaction.new_slot == UINT32_MAX && fingerprint(sizeof flash) == before);
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    flash[5u * 0x20000u + 0x1ffffu] = 0;
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && transaction.new_slot == UINT32_MAX && fingerprint(sizeof flash) == before);
    /* Grow and shrink an actual value, retaining every byte of each old body. */
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    uint32_t old_slot = 0x105u, next_slot = old_slot + sizeof record;
    for (uint32_t length = 32u;; length = 0) {
        uint32_t bytes;
        uint8_t *replacement = string_record(length, &bytes);
        before = fingerprint(sizeof flash);
        memcpy(flash + next_slot, replacement, bytes);
        flash[old_slot] = 0;
        uint32_t expected = fingerprint(sizeof flash);
        memset(flash + next_slot, 255, bytes);
        flash[old_slot] = 0x53;
        assert(fingerprint(sizeof flash) == before);
        assert(!hpclang_rpl_flash_replace_backup(io, replacement, bytes, &transaction));
        assert(fingerprint(sizeof flash) == expected);
        old_slot = next_slot;
        next_slot += bytes;
        free(replacement);
        if (!length)
            break;
    }
    /* Filling the remaining tails must not delete the old value to make room. */
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    for (uint32_t index = 0; index < 6; ++index) {
        uint32_t at = index * 0x20000u + 0x105u + (index ? 0u : sizeof record);
        uint32_t extent = (((index + 1u) * 0x20000u - at - 1u) * 2u & ~3u) - 2u;
        flash[at] = 0;
        flash[at + 1u] = (uint8_t)extent;
        flash[at + 2u] = (uint8_t)(extent >> 8u);
        flash[at + 3u] = (uint8_t)(0xf0u | (extent >> 16u));
    }
    transaction.new_slot = UINT32_MAX;
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_NOSPACE);
    assert(!writes && transaction.new_slot == UINT32_MAX && fingerprint(sizeof flash) == before);
}

static void remove_backups(const struct hpclang_rpl_flash_io *io)
{
    const uint8_t *name = (const uint8_t *)"FLASHONE";
    uint32_t attempted = UINT32_MAX;
    init();
    assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_NOT_FOUND);
    assert(!writes && attempted == UINT32_MAX);
    assert(hpclang_rpl_flash_remove_backup(io, NULL, 1, &attempted) == HPCLANG_RPL_FLASH_INVALID);
    for (uint32_t bank = 0; bank < 6; ++bank) {
        init();
        uint32_t slot = bank * 0x20000u + 0x105u;
        memcpy(flash + slot, record, sizeof record);
        /* Compare the entire medium to the intended single-byte change. */
        flash[slot] = 0;
        uint32_t expected = fingerprint(sizeof flash);
        flash[slot] = 0x53;
        assert(!hpclang_rpl_flash_remove_backup(io, name, 8, &attempted));
        assert(attempted == base + slot && writes == 1);
        assert(fingerprint(sizeof flash) == expected);
        assert(hpclang_rpl_flash_remove_backup(io, name, 8, NULL) == HPCLANG_RPL_FLASH_NOT_FOUND);
        assert(writes == 1);
    }
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    uint32_t before = fingerprint(sizeof flash);
    attempted = UINT32_MAX;
    assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && attempted == UINT32_MAX && fingerprint(sizeof flash) == before);
    for (uint32_t bank = 0; bank < 6; ++bank) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        flash[bank * 0x20000u + 0x1ffffu] = 0;
        before = fingerprint(sizeof flash);
        assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!writes && attempted == UINT32_MAX && fingerprint(sizeof flash) == before);
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        fail_address = base + bank * 0x20000u + 0x1fffeu;
        assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) ==
               HPCLANG_RPL_FLASH_FAILED);
        assert(!writes && attempted == UINT32_MAX);
    }
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    write_failures = 1;
    assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_DIRTY);
    assert(attempted == base + 0x105u);
    assert(!hpclang_rpl_flash_finish_remove(io, attempted));
    assert(flash[0x105u] == 0 && !memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
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
    remove_backups(&io);
    replace_backups(&io);
    return 0;
}
