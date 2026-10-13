#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "rpl-port.h"

struct image
{
    uint8_t data[160];
    uint32_t count;
    uint32_t fail_at;
};

static const char library[] = "04b2062000108510b7000000000000000000000d5c2";
static const char backup[] = "26b20130008005f42545a55425f48041620900003210119200f9e3";

static void append(struct image *image, const char *text)
{
    for (; *text; ++text) {
        assert(image->count < sizeof(image->data));
        image->data[image->count++] = *text <= '9' ? *text - '0' : *text - 'a' + 10;
    }
}

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct image *image = context;
    if (address >= image->count || address == image->fail_at)
        return 0;
    *value = image->data[address];
    return 1;
}

static void put_word(struct image *image, uint32_t at, uint32_t value, unsigned count)
{
    assert(at + count <= image->count);
    for (unsigned i = 0; i < count; ++i)
        image->data[at + i] = (value >> (4 * i)) & 15;
}

static void seal_library(struct image *image)
{
    uint32_t crc = 0;
    for (unsigned i = 5; i < image->count - 4; ++i)
        crc = (crc >> 4) ^ (((crc ^ image->data[i]) & 15) * 0x1081);
    put_word(image, image->count - 4, crc, 4);
}

struct visits
{
    unsigned count;
    unsigned abort_at;
    uint32_t targets[4];
};

static int visit_reference(void *context, enum hpclang_rpl_library_reference_kind kind,
                           uint32_t field, uint32_t target, uint32_t limit)
{
    struct visits *visits = context;
    assert(visits->count < 4);
    static const uint32_t fields[] = {27, 32, 67, 72};
    static const enum hpclang_rpl_library_reference_kind kinds[] = {
        HPCLANG_RPL_LIBRARY_LINKS, HPCLANG_RPL_LIBRARY_CONFIG, HPCLANG_RPL_LIBRARY_FUNCTION,
        HPCLANG_RPL_LIBRARY_FUNCTION};
    assert(field == fields[visits->count] && kind == kinds[visits->count]);
    assert(target + limit == 86);
    visits->targets[visits->count++] = target;
    return visits->abort_at == visits->count ? 77 : 0;
}

static void test_library_references(void)
{
    struct image image = {.count = 90, .fail_at = UINT32_MAX};
    put_word(&image, 0, 0x2b40, 5);
    put_word(&image, 5, 85, 5);
    put_word(&image, 14, 123, 3);
    put_word(&image, 27, 30, 5); /* Link table at 57. */
    put_word(&image, 32, 5, 5);  /* Configuration at 37. */
    put_word(&image, 37, 0x2d9d, 5);
    put_word(&image, 42, 0x312b, 5);
    put_word(&image, 47, 0x2a4e, 5);
    put_word(&image, 52, 5, 5);
    put_word(&image, 57, 0x2a4e, 5);
    put_word(&image, 62, 15, 5);
    put_word(&image, 67, (37u - 67u) & 0xfffff, 5);
    put_word(&image, 72, (47u - 72u) & 0xfffff, 5);
    seal_library(&image);
    struct image original = image;
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct visits visits = {0};
    assert(hpclang_rpl_library_visit(&reader, 0, 90, visit_reference, &visits) == 0);
    const uint32_t expected[] = {57, 37, 37, 47};
    assert(visits.count == 4 && memcmp(visits.targets, expected, sizeof(expected)) == 0);
    assert(memcmp(&image, &original, sizeof(image)) == 0);
    visits = (struct visits){.abort_at = 2};
    assert(hpclang_rpl_library_visit(&reader, 0, 90, visit_reference, &visits) == 77);
    assert(visits.count == 2);
    for (unsigned end = 0; end < 90; ++end) {
        visits = (struct visits){0};
        assert(hpclang_rpl_library_visit(&reader, 0, end, visit_reference, &visits) < 0);
        assert(visits.count == 0);
    }
    for (unsigned at = 0; at < 90; ++at) {
        image.fail_at = at;
        visits = (struct visits){0};
        assert(hpclang_rpl_library_visit(&reader, 0, 90, visit_reference, &visits) < 0);
        assert(visits.count == 0);
    }
    image = original;
    put_word(&image, 17, 86 - 17, 5); /* Reference into CRC. */
    seal_library(&image);
    assert(hpclang_rpl_library_visit(&reader, 0, 90, visit_reference, &visits) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    image = original;
    put_word(&image, 62, 14, 5); /* Partial five-nibble entry. */
    seal_library(&image);
    assert(hpclang_rpl_library_visit(&reader, 0, 90, visit_reference, &visits) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    image = original;
    image.data[86] ^= 1;
    assert(hpclang_rpl_library_visit(&reader, 0, 90, visit_reference, &visits) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    image = original;
    put_word(&image, 67, (12u - 67u) & 0xfffff, 5); /* Function points into header. */
    seal_library(&image);
    visits = (struct visits){0};
    assert(hpclang_rpl_library_visit(&reader, 0, 90, visit_reference, &visits) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(visits.count == 2); /* Caller discards partial traversal. */
    assert(hpclang_rpl_library_visit(NULL, 0, 90, visit_reference, &visits) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_library_visit(&reader, 0xffff0, 90, visit_reference, &visits) ==
           HPCLANG_RPL_SPAN_BOUNDS);
}

static void test_flash(void)
{
    struct image image = {.fail_at = UINT32_MAX};
    /* A 64-nibble slot: extent 62 starts at nibble 2. Metadata 1 at
     * nibble 7 must not be interpreted as high bits of the extent. */
    append(&image, "35e30001");
    append(&image, library);
    while (image.count < 64)
        append(&image, "f");
    append(&image, "35e30000");
    append(&image, backup);
    append(&image, "ffff");
    assert(image.count == 130);
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_port_cursor cursor = {.end = image.count};
    struct hpclang_rpl_port_entry entry;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 1);
    assert(entry.address == 8 && entry.span == 43 && entry.library_number == 123);
    assert(cursor.address == 64);
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 1);
    assert(entry.address == 72 && entry.span == 54 && cursor.address == 128);
    struct hpclang_rpl_port_entry saved = entry;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 0);
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 0);
    assert(memcmp(&entry, &saved, sizeof(entry)) == 0);

    for (uint32_t end = 0; end < 64; ++end) {
        cursor = (struct hpclang_rpl_port_cursor){.end = end};
        struct hpclang_rpl_port_cursor before = cursor;
        assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) < 0);
        assert(memcmp(&cursor, &before, sizeof(cursor)) == 0);
        assert(memcmp(&entry, &saved, sizeof(entry)) == 0);
    }
    cursor = (struct hpclang_rpl_port_cursor){.end = image.count};
    image.data[0] = 0;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    image.data[0] = 3;
    image.data[2] = 0;
    image.data[3] = 0;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_MALFORMED);
    image.data[2] = 14;
    image.data[3] = 3;
    image.fail_at = 4;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    image.fail_at = UINT32_MAX;
    image.data[50] ^= 1; // Native library CRC damage.
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(cursor.address == 0 && cursor.finished == 0);
    assert(memcmp(&entry, &saved, sizeof(entry)) == 0);

    // Deleted payloads are irrelevant, including the corrupted library CRC.
    image.data[0] = image.data[1] = 0;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 1);
    assert(entry.address == 72 && cursor.address == 128);
    cursor.address = 0;
    image.data[64] = image.data[65] = 0;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == 0);
    assert(cursor.address == 128 && cursor.finished);
    assert(memcmp(&entry, &saved, sizeof(entry)) == 0);
    cursor = (struct hpclang_rpl_port_cursor){.end = image.count};
    image.data[64] = 1;
    assert(hpclang_rpl_flash_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(cursor.address == 0 && !cursor.finished);
    assert(memcmp(&entry, &saved, sizeof(entry)) == 0);
}

int main(void)
{
    test_library_references();
    test_flash();
    /* Owned synthetic library 123, title X, empty table offsets. The backup is
     * the Stock-generated PORTZERO fixture. Adjacency catches CRC over-counting. */
    struct image image = {.fail_at = UINT32_MAX};
    append(&image, library);
    append(&image, backup);
    append(&image, "00000");
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_port_cursor cursor = {.end = image.count};
    struct hpclang_rpl_port_entry entry;
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
    assert(entry.address == 0 && entry.span == 43 && entry.value_span == 43);
    assert(entry.library_number == 123 && entry.name_length == 1 && entry.name[0] == 'X');
    assert(cursor.address == 43);
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
    assert(entry.address == 43 && entry.span == 54 && entry.value_span == 14);
    assert(entry.name_length == 8 && memcmp(entry.name, "PORTZERO", 8) == 0);
    struct hpclang_rpl_port_entry saved = entry;
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
    assert(memcmp(&entry, &saved, sizeof(entry)) == 0);

    /* Truncation, unreadable data and corrupt metadata/CRC must not advance. */
    for (uint32_t end = 0; end < 43; ++end) {
        cursor = (struct hpclang_rpl_port_cursor){.end = end};
        struct hpclang_rpl_port_cursor before = cursor;
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) < 0);
        assert(memcmp(&cursor, &before, sizeof(cursor)) == 0);
        assert(memcmp(&entry, &saved, sizeof(entry)) == 0);
    }
    cursor = (struct hpclang_rpl_port_cursor){.end = image.count};
    image.fail_at = 17;
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    image.fail_at = UINT32_MAX;
    const uint32_t corrupt[] = {10, 14, 17, 30, 42};
    for (uint32_t i = 0; i < sizeof(corrupt) / sizeof(corrupt[0]); ++i) {
        image.data[corrupt[i]] ^= 1;
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_MALFORMED);
        assert(cursor.address == 0 && cursor.finished == 0);
        image.data[corrupt[i]] ^= 1;
    }
    cursor.address = image.count - 4;
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_BOUNDS);
    cursor.address = UINT32_MAX;
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == HPCLANG_RPL_SPAN_BOUNDS);
    return 0;
}
