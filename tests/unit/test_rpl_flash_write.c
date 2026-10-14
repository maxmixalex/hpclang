#include "rpl-flash-write.h"
#include "rpl-port.h"
#include <assert.h>
#include <string.h>

static const uint8_t record[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};

struct chip
{
    uint8_t data[160];
    unsigned reads, writes, fail_read, fail_write;
    uint16_t partial;
    int persistent, silent;
};

static int read16(void *context, uint32_t at, uint16_t *value)
{
    struct chip *chip = context;
    assert(!(at & 1u) && at + 1u < sizeof chip->data);
    if (chip->reads++ == chip->fail_read)
        return 0;
    *value = (uint16_t)(chip->data[at] | (uint16_t)chip->data[at + 1u] << 8u);
    return 1;
}

static int program16(void *context, uint32_t at, uint16_t value)
{
    struct chip *chip = context;
    assert(!(at & 1u) && at + 1u < sizeof chip->data);
    uint16_t old = (uint16_t)(chip->data[at] | (uint16_t)chip->data[at + 1u] << 8u);
    assert((old & value) == value);
    unsigned call = chip->writes++;
    int fail = call == chip->fail_write || (chip->persistent && call > chip->fail_write);
    if (fail)
        value |= (uint16_t)~chip->partial;
    value &= old;
    chip->data[at] = (uint8_t)value;
    chip->data[at + 1u] = (uint8_t)(value >> 8u);
    return !fail || chip->silent;
}

static void init(struct chip *chip, uint32_t slot)
{
    *chip = (struct chip){.fail_read = UINT32_MAX, .fail_write = UINT32_MAX};
    memset(chip->data, 0xff, sizeof chip->data);
    chip->data[slot - 1u] = 0xa5;
    chip->data[slot + sizeof record + 1u] = 0x5a;
}

static int nibble(void *context, uint32_t at, uint8_t *value)
{
    const struct chip *chip = context;
    if (at / 2u >= sizeof chip->data)
        return 0;
    *value = (chip->data[at / 2u] >> ((at & 1u) * 4u)) & 15u;
    return 1;
}

static void verify_failed(struct chip *chip, uint32_t slot, const uint8_t *before)
{
    assert(!memcmp(chip->data, before, slot));
    assert(!memcmp(chip->data + slot + sizeof record, before + slot + sizeof record,
                   sizeof chip->data - slot - sizeof record));
    struct hpclang_rpl_reader reader = {nibble, chip};
    struct hpclang_rpl_port_cursor cursor = {slot * 2u, (slot + sizeof record + 1u) * 2u, 0};
    struct hpclang_rpl_port_entry entry;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 0);
    assert(chip->data[slot] == 0 || !memcmp(chip->data, before, sizeof chip->data));
}

int main(void)
{
    struct chip chip;
    const struct hpclang_rpl_flash_io io = {&chip, read16, program16, 0, sizeof chip.data};
    for (uint32_t slot = 32; slot <= 33; ++slot) {
        init(&chip, slot);
        uint8_t before[sizeof chip.data];
        memcpy(before, chip.data, sizeof before);
        assert(hpclang_rpl_flash_append(&io, slot, record, sizeof record) == 0);
        assert(!memcmp(chip.data + slot, record, sizeof record));
        assert(!memcmp(chip.data, before, slot));
        assert(!memcmp(chip.data + slot + sizeof record, before + slot + sizeof record,
                       sizeof chip.data - slot - sizeof record));
        unsigned reads = chip.reads, writes = chip.writes;
        assert(hpclang_rpl_flash_append(&io, slot, record, sizeof record) ==
               HPCLANG_RPL_FLASH_OCCUPIED);
        assert(chip.writes == writes);
        const uint16_t masks[] = {0, 0x5555, 0xaaaa, 0xffff};
        for (unsigned cut = 0; cut < writes; ++cut) {
            for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
                init(&chip, slot);
                chip.fail_write = cut;
                chip.partial = masks[mask];
                assert(hpclang_rpl_flash_append(&io, slot, record, sizeof record) ==
                       HPCLANG_RPL_FLASH_FAILED);
                verify_failed(&chip, slot, before);
            }
            init(&chip, slot);
            chip.fail_write = cut;
            chip.silent = 1;
            assert(hpclang_rpl_flash_append(&io, slot, record, sizeof record) ==
                   HPCLANG_RPL_FLASH_FAILED);
            verify_failed(&chip, slot, before);
        }
        for (unsigned cut = 0; cut < reads; ++cut) {
            init(&chip, slot);
            chip.fail_read = cut;
            assert(hpclang_rpl_flash_append(&io, slot, record, sizeof record) ==
                   HPCLANG_RPL_FLASH_FAILED);
            verify_failed(&chip, slot, before);
        }
        init(&chip, slot);
        chip.fail_write = 1;
        chip.partial = 0x5555;
        chip.persistent = 1;
        assert(hpclang_rpl_flash_append(&io, slot, record, sizeof record) ==
               HPCLANG_RPL_FLASH_DIRTY);
        init(&chip, slot);
        uint8_t bad[sizeof record];
        memcpy(bad, record, sizeof bad);
        bad[30] ^= 1;
        assert(hpclang_rpl_flash_append(&io, slot, bad, sizeof bad) == HPCLANG_RPL_FLASH_INVALID);
        assert(hpclang_rpl_flash_append(&io, 140, record, sizeof record) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(!chip.writes && !memcmp(chip.data, before, sizeof before));
    }
    return 0;
}
