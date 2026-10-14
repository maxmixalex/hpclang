#include "rpl-flash-write.h"
#include "rpl-port.h"
#include "rpl-flash-record.h"
#include <assert.h>
#include <string.h>

static const uint8_t record[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};

struct chip
{
    uint8_t data[160];
    uint32_t base;
    unsigned reads, writes, fail_read, fail_write;
    uint16_t partial;
    int persistent, silent;
};

static int read16(void *context, uint32_t at, uint16_t *value)
{
    struct chip *chip = context;
    assert(at >= chip->base);
    at -= chip->base;
    assert(!(at & 1u) && at + 1u < sizeof chip->data);
    if (chip->reads++ == chip->fail_read)
        return 0;
    *value = (uint16_t)(chip->data[at] | (uint16_t)chip->data[at + 1u] << 8u);
    return 1;
}

static int program16(void *context, uint32_t at, uint16_t value)
{
    struct chip *chip = context;
    assert(at >= chip->base);
    at -= chip->base;
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

static void recovery(void)
{
    for (uint32_t first = 16; first <= 17; ++first) {
        struct chip chip;
        const struct hpclang_rpl_flash_io io = {&chip, read16, program16, 0, sizeof chip.data};
        uint32_t slot = first + sizeof record;
        /* A live predecessor must survive recovery of every interrupted body
         * bit, including interruptions before the length field is complete. */
        for (unsigned bits = 0; bits <= (sizeof record - 1u) * 8u; ++bits) {
            init(&chip, first);
            memset(chip.data + first, 0xff, sizeof chip.data - first);
            memcpy(chip.data + first, record, sizeof record);
            for (unsigned bit = 0; bit < bits; ++bit)
                chip.data[slot + 1u + bit / 8u] &=
                    (uint8_t)(record[1u + bit / 8u] | ~(1u << (bit & 7u)));
            uint8_t before[sizeof chip.data];
            memcpy(before, chip.data, sizeof before);
            assert(hpclang_rpl_flash_recover_tail(&io, first, sizeof chip.data) == 0);
            assert(!memcmp(chip.data, before, slot));
            assert(
                !memcmp(chip.data + slot + 4u, before + slot + 4u, sizeof chip.data - slot - 4u));
            struct hpclang_rpl_reader reader = {nibble, &chip};
            struct hpclang_rpl_flash_space space;
            assert(!hpclang_rpl_flash_tail(&reader, first * 2u, sizeof chip.data * 2u, &space));
            unsigned writes = chip.writes;
            assert(!hpclang_rpl_flash_recover_tail(&io, first, sizeof chip.data));
            assert(chip.writes == writes);
        }
        /* A sparse interrupted body forces recovery to program an extent.
         * All read failures must fail closed; all program cuts must preserve
         * the preceding live object and bytes outside the recovery header. */
        init(&chip, first);
        memset(chip.data + first, 0xff, sizeof chip.data - first);
        memcpy(chip.data + first, record, sizeof record);
        chip.data[slot + 20u] = 0;
        struct chip original = chip;
        /* Physical NOR addresses exceed the native reader's coordinate space. */
        const uint32_t bases[] = {0x140000u, 0x1e0000u, 0xffffff00u};
        for (unsigned i = 0; i < sizeof bases / sizeof bases[0]; ++i) {
            chip = original;
            chip.base = bases[i];
            struct hpclang_rpl_flash_io mapped = io;
            mapped.first += chip.base;
            mapped.end += chip.base;
            assert(!hpclang_rpl_flash_recover_tail(&mapped, chip.base + first, mapped.end));
            assert(!memcmp(chip.data, original.data, slot));
            assert(chip.data[slot] == 0);
        }
        chip = original;
        assert(!hpclang_rpl_flash_recover_tail(&io, first, sizeof chip.data));
        unsigned reads = chip.reads, writes = chip.writes;
        for (unsigned cut = 0; cut < reads; ++cut) {
            chip = original;
            chip.fail_read = cut;
            assert(hpclang_rpl_flash_recover_tail(&io, first, sizeof chip.data) ==
                   HPCLANG_RPL_FLASH_DIRTY);
            assert(!memcmp(chip.data, original.data, slot));
        }
        for (unsigned cut = 0; cut < writes; ++cut) {
            const uint16_t masks[] = {0, 0x5555, 0xaaaa, 0xffff};
            for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
                chip = original;
                chip.fail_write = cut;
                chip.partial = masks[mask];
                assert(hpclang_rpl_flash_recover_tail(&io, first, sizeof chip.data) ==
                       HPCLANG_RPL_FLASH_DIRTY);
                assert(!memcmp(chip.data, original.data, slot));
                assert(!memcmp(chip.data + slot + 4u, original.data + slot + 4u,
                               sizeof chip.data - slot - 4u));
                /* Retry is safe after interrupted extent writes. A partially
                 * written status is intentionally outside this primitive. */
                chip.fail_write = UINT32_MAX;
                if (chip.data[slot] == 0xff || chip.data[slot] == 0)
                    assert(!hpclang_rpl_flash_recover_tail(&io, first, sizeof chip.data));
            }
        }
        chip = original;
        chip.data[slot] = 0x7f;
        assert(hpclang_rpl_flash_recover_tail(&io, first, sizeof chip.data) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!chip.writes);
        assert(hpclang_rpl_flash_recover_tail(&io, first, first) == HPCLANG_RPL_FLASH_INVALID);
        assert(hpclang_rpl_flash_recover_tail(&io, first, UINT32_MAX) == HPCLANG_RPL_FLASH_INVALID);
    }
}

static void finish_commit(void)
{
    struct chip chip;
    const struct hpclang_rpl_flash_io io = {&chip, read16, program16, 0, sizeof chip.data};
    for (uint32_t slot = 32; slot <= 33; ++slot) {
        for (unsigned status = 0; status <= 255; ++status) {
            init(&chip, slot);
            memcpy(chip.data + slot, record, sizeof record);
            chip.data[slot] = (uint8_t)status;
            uint8_t before[sizeof chip.data];
            memcpy(before, chip.data, sizeof before);
            int result = hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record);
            if ((status & 0x53u) == 0x53u) {
                assert(!result && chip.data[slot] == 0x53u);
                unsigned writes = chip.writes;
                assert(!hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record));
                assert(chip.writes == writes);
            } else {
                assert(result == HPCLANG_RPL_FLASH_DIRTY && !chip.writes);
                assert(chip.data[slot] == status);
            }
            assert(!memcmp(chip.data, before, slot));
            assert(
                !memcmp(chip.data + slot + 1u, before + slot + 1u, sizeof chip.data - slot - 1u));
        }
        init(&chip, slot);
        memcpy(chip.data + slot + 1u, record + 1u, sizeof record - 1u);
        struct chip pending = chip;
        assert(!hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record));
        unsigned reads = chip.reads;
        for (unsigned cut = 0; cut < reads; ++cut) {
            chip = pending;
            chip.fail_read = cut;
            int result = hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record);
            assert(result == HPCLANG_RPL_FLASH_FAILED || result == HPCLANG_RPL_FLASH_DIRTY);
            chip.fail_read = UINT32_MAX;
            assert(!hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record));
        }
        for (unsigned mask = 0; mask < 256; ++mask) {
            chip = pending;
            chip.fail_write = 0;
            chip.partial = (uint16_t)(mask << ((slot & 1u) * 8u));
            assert(hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record) ==
                   HPCLANG_RPL_FLASH_DIRTY);
            chip.fail_write = UINT32_MAX;
            assert(!hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record));
            assert(!memcmp(chip.data + slot, record, sizeof record));
        }
        for (unsigned at = 1; at < sizeof record; ++at) {
            chip = pending;
            chip.data[slot + at] ^= 1;
            assert(hpclang_rpl_flash_finish_commit(&io, slot, record, sizeof record) ==
                   HPCLANG_RPL_FLASH_DIRTY);
            assert(!chip.writes);
        }
        chip = pending;
        uint8_t bad[sizeof record];
        memcpy(bad, record, sizeof bad);
        bad[30] ^= 1;
        assert(hpclang_rpl_flash_finish_commit(&io, slot, bad, sizeof bad) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(!chip.reads && !chip.writes);
    }
}

static void remove_status(void)
{
    for (uint32_t slot = 16; slot <= 17; ++slot) {
        struct chip chip;
        const struct hpclang_rpl_flash_io io = {&chip, read16, program16, 0, sizeof chip.data};
        for (unsigned status = 0; status < 256; ++status) {
            init(&chip, slot);
            memcpy(chip.data + slot, record, sizeof record);
            chip.data[slot] = (uint8_t)status;
            uint8_t before[sizeof chip.data];
            memcpy(before, chip.data, sizeof before);
            int result = hpclang_rpl_flash_finish_remove(&io, slot);
            if (status & ~0x53u) {
                assert(result == HPCLANG_RPL_FLASH_DIRTY && !chip.writes);
            } else {
                assert(!result);
                before[slot] = 0;
                assert(!hpclang_rpl_flash_finish_remove(&io, slot));
            }
            assert(!memcmp(before, chip.data, sizeof before));
        }
        for (unsigned mask = 0; mask < 256; ++mask) {
            init(&chip, slot);
            memcpy(chip.data + slot, record, sizeof record);
            chip.fail_write = 0;
            chip.partial = (uint16_t)(mask << ((slot & 1u) * 8u));
            uint8_t before[sizeof chip.data];
            memcpy(before, chip.data, sizeof before);
            assert(hpclang_rpl_flash_finish_remove(&io, slot) == HPCLANG_RPL_FLASH_DIRTY);
            assert(!hpclang_rpl_flash_finish_remove(&io, slot));
            before[slot] = 0;
            assert(!memcmp(before, chip.data, sizeof before));
        }
        /* Read failures before programming and during verification must not
         * permit an uncertain status to escape the owner's recovery loop. */
        init(&chip, slot);
        memcpy(chip.data + slot, record, sizeof record);
        assert(!hpclang_rpl_flash_finish_remove(&io, slot));
        unsigned reads = chip.reads;
        for (unsigned cut = 0; cut < reads; ++cut) {
            init(&chip, slot);
            memcpy(chip.data + slot, record, sizeof record);
            chip.fail_read = cut;
            assert(hpclang_rpl_flash_finish_remove(&io, slot) == HPCLANG_RPL_FLASH_DIRTY);
            assert(!hpclang_rpl_flash_finish_remove(&io, slot));
            assert(chip.data[slot] == 0);
            assert(!memcmp(chip.data + slot + 1u, record + 1u, sizeof record - 1u));
        }
    }
}

int main(void)
{
    remove_status();
    finish_commit();
    recovery();
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
