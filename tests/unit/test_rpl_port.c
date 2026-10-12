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

int main(void)
{
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
