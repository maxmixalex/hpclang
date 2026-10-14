#include "rpl-flash-record.h"
#include "rpl-port.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* HP50g 2.15: native `987 :2:FLASHONE STO`, captured after normal return.
 * Includes allocation header, DOBAK with native CRC, and erased padding. */
static const uint8_t native_record[] = {
    0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46, 0x4c, 0x41, 0x53, 0x48, 0x4f,
    0x4e, 0x45, 0x08, 0x14, 0x26, 0x90, 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};

struct image
{
    const uint8_t *data;
    uint32_t nibbles, base, reads, fail_after;
};

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct image *image = context;
    if (image->reads++ == image->fail_after || address < image->base ||
        address - image->base >= image->nibbles)
        return 0;
    uint32_t offset = address - image->base;
    *value = (image->data[offset / 2u] >> (4u * (offset & 1u))) & 15u;
    return 1;
}

static void put_word(uint8_t *data, uint32_t at, uint32_t value)
{
    for (unsigned i = 0; i < 5; ++i, ++at) {
        unsigned shift = (at & 1u) * 4u;
        data[at / 2u] =
            (uint8_t)((data[at / 2u] & ~(15u << shift)) | (((value >> (4u * i)) & 15u) << shift));
    }
}

static void test_tail(void)
{
    uint8_t bank[128];
    memset(bank, 0xff, sizeof bank);
    struct image source = {bank, sizeof bank * 2u, 0x20a, 0, UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &source};
    const uint32_t end = source.base + source.nibbles;
    struct hpclang_rpl_flash_space space;
    assert(hpclang_rpl_flash_tail(&reader, source.base, end, &space) == 0);
    assert(space.address == source.base && space.nibbles == 252);
    memcpy(bank, native_record, sizeof native_record);
    memcpy(bank + sizeof native_record, native_record, sizeof native_record);
    bank[sizeof native_record] = 0;
    /* Deleted payloads are not interpreted, but still occupy their slots. */
    bank[sizeof native_record + 8] ^= 0xff;
    source.reads = 0;
    assert(hpclang_rpl_flash_tail(&reader, source.base, end, &space) == 0);
    assert(space.address == source.base + 128u && space.nibbles == 124u);
    uint32_t reads = source.reads;
    for (uint32_t cut = 0; cut < reads; ++cut) {
        source.reads = 0;
        source.fail_after = cut;
        space = (struct hpclang_rpl_flash_space){123, 456};
        assert(hpclang_rpl_flash_tail(&reader, source.base, end, &space) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(space.address == 123 && space.nibbles == 456);
    }
    source.fail_after = UINT32_MAX;
    /* An interrupted append can leave any byte behind an erased status. */
    for (uint32_t at = 65; at < sizeof bank; ++at) {
        bank[at] = 0xfe;
        assert(hpclang_rpl_flash_tail(&reader, source.base, end, &space) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(space.address == 123 && space.nibbles == 456);
        bank[at] = 0xff;
    }
    bank[30] ^= 1;
    assert(hpclang_rpl_flash_tail(&reader, source.base, end, &space) == HPCLANG_RPL_SPAN_MALFORMED);
    bank[30] ^= 1;
    assert(hpclang_rpl_flash_tail(&reader, source.base, source.base + 128u, &space) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_flash_tail(&reader, end, source.base, &space) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_flash_tail(&reader, source.base, 0x100002u, &space) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_flash_tail(NULL, source.base, end, &space) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(space.address == 123 && space.nibbles == 456);
    assert(hpclang_rpl_flash_tail(&reader, source.base, source.base + 130u, &space) == 0);
    assert(space.address == source.base + 128u && space.nibbles == 0);
}

int main(void)
{
    test_tail();
    struct image source = {native_record + 4, 54, 0x100, 0, UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &source};
    uint8_t *record = NULL;
    uint32_t bytes = 0;
    assert(hpclang_rpl_flash_backup_record(&reader, source.base, source.nibbles, &record, &bytes) ==
           0);
    assert(bytes == sizeof native_record && !memcmp(record, native_record, bytes));
    free(record);

    uint32_t reads = source.reads;
    uint8_t sentinel;
    for (uint32_t cut = 0; cut < reads; ++cut) {
        source.reads = 0;
        source.fail_after = cut;
        record = &sentinel;
        bytes = 123;
        assert(hpclang_rpl_flash_backup_record(&reader, source.base, source.nibbles, &record,
                                               &bytes) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(record == &sentinel && bytes == 123);
    }
    source.fail_after = UINT32_MAX;
    assert(hpclang_rpl_flash_backup_record(&reader, source.base, 55, &record, &bytes) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_flash_backup_record(&reader, source.base, UINT32_MAX, &record, &bytes) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_flash_backup_record(NULL, 0, 54, &record, &bytes) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(record == &sentinel && bytes == 123);

    uint8_t corrupt[27];
    memcpy(corrupt, native_record + 4, sizeof corrupt);
    corrupt[26] ^= 0x10;
    source.data = corrupt;
    assert(hpclang_rpl_flash_backup_record(&reader, source.base, 54, &record, &bytes) ==
           HPCLANG_RPL_SPAN_MALFORMED);

    uint8_t bank[sizeof native_record + 2];
    memcpy(bank, native_record, sizeof native_record);
    memset(bank + sizeof native_record, 0xff, 2);
    source = (struct image){bank, sizeof bank * 2, 0x20a, 0, UINT32_MAX};
    struct hpclang_rpl_port_cursor cursor = {source.base, source.base + source.nibbles, 0};
    struct hpclang_rpl_port_entry entry;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 1);
    assert(entry.address == 0x212 && entry.span == 54 && entry.name_length == 8);
    assert(!memcmp(entry.name, "FLASHONE", 8) && cursor.address == 0x24a);
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 0 && cursor.finished);
    bank[0] = 0; /* Native PURGE leaves the allocation and payload in place. */
    cursor = (struct hpclang_rpl_port_cursor){source.base, source.base + source.nibbles, 0};
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 0 && cursor.finished);

    /* A 21-nibble real yields an odd backup span. Its unused high nibble
     * belongs to the erased padding, even when the input allocation holds 0. */
    static const uint8_t real[11] = {0x33, 0x29};
    source = (struct image){real, 21, 0, 0, UINT32_MAX};
    uint8_t *backup;
    uint32_t span;
    assert(hpclang_rpl_backup_encode(&reader, 0, 21, (const uint8_t *)"R", 1, &backup, &span) == 0);
    assert(span == 47);
    source = (struct image){backup, span, 0, 0, UINT32_MAX};
    assert(hpclang_rpl_flash_backup_record(&reader, 0, span, &record, &bytes) == 0);
    assert(bytes == 28 && (record[bytes - 1] & 0xf0u) == 0xf0u);
    free(record);
    free(backup);

    /* A full bank still needs its prefix and erased end marker. Exercise
     * the largest fitting record and the next halfword beyond that bound. */
    for (unsigned extra = 0; extra <= 2; extra += 2) {
        uint32_t object_span = 0x40000u - 0x20au - 2u - 8u - 24u + extra;
        uint8_t *string = calloc((object_span + 1u) / 2u, 1);
        assert(string);
        put_word(string, 0, 0x02a2cu);
        put_word(string, 5, object_span - 5u);
        source = (struct image){string, object_span, 0, 0, UINT32_MAX};
        assert(hpclang_rpl_backup_encode(&reader, 0, object_span, NULL, 0, &backup, &span) == 0);
        source = (struct image){backup, span, 0, 0, UINT32_MAX};
        int result = hpclang_rpl_flash_backup_record(&reader, 0, span, &record, &bytes);
        if (!extra) {
            assert(result == 0 && bytes == 0x1fefau);
            free(record);
        } else {
            assert(result == HPCLANG_RPL_SPAN_BOUNDS);
        }
        free(backup);
        free(string);
    }
    return 0;
}
