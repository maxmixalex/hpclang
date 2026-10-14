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

static void test_library_catalog_lookup(void)
{
    struct image image = {.count = 35, .fail_at = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put_word(&image, 0, 2, 3);
    put_word(&image, 3, 161, 3);
    put_word(&image, 6, 0x4dee9, 5);
    put_word(&image, 11, 0x22f2e, 5);
    put_word(&image, 16, 0x123, 3);
    put_word(&image, 19, 275, 3);
    put_word(&image, 22, 0xdd56a, 5);
    put_word(&image, 27, 0, 5);
    struct hpclang_rpl_library_binding binding = {1, 2, 3};
    assert(hpclang_rpl_library_lookup(&reader, 0, 35, 161, &binding) == 1);
    assert(binding.address == 0x4dee9 && binding.bank_switch == 0x22f2e && binding.flags == 0x123);
    assert(hpclang_rpl_library_lookup(&reader, 0, 35, 275, &binding) == 1);
    assert(binding.address == 0xdd56a && binding.bank_switch == 0);
    struct hpclang_rpl_library_binding before = binding;
    assert(hpclang_rpl_library_lookup(&reader, 0, 35, 900, &binding) == 0);
    assert(memcmp(&before, &binding, sizeof(binding)) == 0);
    for (uint32_t limit = 0; limit < 35; ++limit) {
        assert(hpclang_rpl_library_lookup(&reader, 0, limit, 161, &binding) < 0);
        assert(memcmp(&before, &binding, sizeof(binding)) == 0);
    }
    for (uint32_t failed = 0; failed < 22; ++failed) {
        image.fail_at = failed;
        assert(hpclang_rpl_library_lookup(&reader, 0, 35, 161, &binding) < 0);
        assert(memcmp(&before, &binding, sizeof(binding)) == 0);
    }
    image.fail_at = UINT32_MAX;
    put_word(&image, 19, 161, 3);
    assert(hpclang_rpl_library_lookup(&reader, 0, 35, 161, &binding) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(memcmp(&before, &binding, sizeof(binding)) == 0);
    assert(hpclang_rpl_library_lookup(&reader, 0xfffff, 35, 161, &binding) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_library_lookup(&reader, 0, 35, 0x1000, &binding) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_library_lookup(&reader, 0, 35, 161, NULL) == HPCLANG_RPL_SPAN_UNAVAILABLE);
}

static void test_library_command_resolution(void)
{
    struct image image = {.count = 100, .fail_at = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put_word(&image, 5, 161, 3);
    put_word(&image, 18, 32, 5);
    put_word(&image, 50, 0x02a4e, 5);
    put_word(&image, 55, 20, 5);
    put_word(&image, 60, (30u - 60u) & 0xfffff, 5);
    put_word(&image, 65, 25, 5);
    put_word(&image, 70, 0, 5);
    uint32_t target = 123;
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 0, &target) == 1);
    assert(target == 30);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 1, &target) == 1);
    assert(target == 90);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 2, &target) == 0);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 3, &target) == 0);
    assert(target == 90);
    assert(hpclang_rpl_library_command(&reader, 0, 74, 5, 161, 0, &target) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 162, 0, &target) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(target == 90);
    const uint32_t reads[] = {5, 6, 7, 18, 22, 50, 54, 55, 59, 60, 64};
    for (unsigned i = 0; i < sizeof(reads) / sizeof(reads[0]); ++i) {
        image.fail_at = reads[i];
        assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 0, &target) < 0);
        assert(target == 90);
    }
    image.fail_at = UINT32_MAX;
    put_word(&image, 55, 21, 5);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 0, &target) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    put_word(&image, 55, 20, 5);
    put_word(&image, 60, 40, 5);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 0, &target) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    put_word(&image, 18, 0, 5);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 5, 161, 0, &target) == 0);
    assert(target == 90);
    assert(hpclang_rpl_library_command(&reader, 0, 100, 99, 161, 0, &target) ==
           HPCLANG_RPL_SPAN_BOUNDS);
}

static void test_bank_lookup(void)
{
    struct image image = {.count = 125, .fail_at = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    for (uint32_t i = 0; i < 16; ++i)
        put_word(&image, 7 * i, 0x12345 + i * i, 5);
    put_word(&image, 120, 0x45678, 5);
    struct hpclang_rpl_bank_binding binding = {HPCLANG_RPL_BANK_ERAM, 91};
    for (uint32_t i = 0; i < 16; ++i) {
        assert(hpclang_rpl_bank_lookup(&reader, 0, 120, 0x12345 + i * i, &binding) == 1);
        assert(binding.kind == HPCLANG_RPL_BANK_FLASH && binding.index == i);
    }
    assert(hpclang_rpl_bank_lookup(&reader, 0, 120, 0x45678, &binding) == 1);
    assert(binding.kind == HPCLANG_RPL_BANK_ERAM && binding.index == 0);
    struct hpclang_rpl_bank_binding before = binding;
    assert(hpclang_rpl_bank_lookup(&reader, 0, 120, 0x99999, &binding) == 0);
    assert(memcmp(&before, &binding, sizeof(binding)) == 0);
    image.fail_at = 120;
    assert(hpclang_rpl_bank_lookup(&reader, 0, 120, 0x12345, &binding) < 0);
    assert(memcmp(&before, &binding, sizeof(binding)) == 0);
    image.fail_at = UINT32_MAX;
    put_word(&image, 120, 0x12345, 5);
    assert(hpclang_rpl_bank_lookup(&reader, 0, 120, 0x12345, &binding) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(memcmp(&before, &binding, sizeof(binding)) == 0);
    put_word(&image, 120, 0, 5);
    assert(hpclang_rpl_bank_lookup(&reader, 0, 120, 0x12345, &binding) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_bank_lookup(&reader, 0xfffff, 120, 0x12345, &binding) ==
           HPCLANG_RPL_SPAN_BOUNDS);
}

static int mapped_nibble(void *context, uint32_t address, uint8_t *value)
{
    if (address < 0x40000u)
        return 0;
    return read_nibble(context, address - 0x40000u, value);
}

static void test_flashptr_bank(void)
{
    struct image image = {.count = 160, .fail_at = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &image};
    for (uint32_t i = 0; i < 16u; ++i)
        put_word(&image, 80u + 5u * i, 0x7f020u + i * i, 5);
    put_word(&image, 5, 0x7f024u, 5);  /* Selector 1 maps physical bank 2. */
    put_word(&image, 75, 0x7f020u, 5); /* Selector 15 maps physical bank 0. */
    uint32_t bank = 99;
    assert(hpclang_rpl_flashptr_bank(&reader, 0, 80, 1, &bank) == 1 && bank == 2);
    assert(hpclang_rpl_flashptr_bank(&reader, 0, 80, 15, &bank) == 1 && bank == 0);
    bank = 99;
    assert(!hpclang_rpl_flashptr_bank(&reader, 0, 80, 5, &bank) && bank == 99);
    put_word(&image, 25, 0x12345, 5);
    assert(!hpclang_rpl_flashptr_bank(&reader, 0, 80, 5, &bank) && bank == 99);
    for (unsigned at = 0; at < 160; ++at) {
        if (!(at >= 80 || (at >= 5 && at < 10)))
            continue;
        image.fail_at = at;
        assert(hpclang_rpl_flashptr_bank(&reader, 0, 80, 1, &bank) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(bank == 99);
    }
    image.fail_at = UINT32_MAX;
    put_word(&image, 155, 0x7f024, 5);
    assert(hpclang_rpl_flashptr_bank(&reader, 0, 80, 1, &bank) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(bank == 99);
    assert(hpclang_rpl_flashptr_bank(&reader, 0xfffff, 80, 1, &bank) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_flashptr_bank(&reader, 0, 80, 16, &bank) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(bank == 99);
}

static void test_flashptr_special_table(void)
{
    struct image image = {.count = 15, .fail_at = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {mapped_nibble, &image};
    put_word(&image, 0, 2, 5);
    put_word(&image, 5, 0x4d7ce, 5);
    put_word(&image, 10, 0x6f8e, 5);
    uint32_t target = 99;
    assert(hpclang_rpl_flashptr_table_lookup(&reader, 0x40000, 0x4000f, 0x40000, 0, &target) == 1);
    assert(target == 0x4d7ce);
    assert(hpclang_rpl_flashptr_table_lookup(&reader, 0x40000, 0x4000f, 0x40000, 1, &target) == 1);
    assert(target == 0x6f8e);
    assert(!hpclang_rpl_flashptr_table_lookup(&reader, 0x40000, 0x4000f, 0x40000, 2, &target));
    assert(target == 0x6f8e);
    for (uint32_t end = 0; end < 15; ++end) {
        assert(hpclang_rpl_flashptr_table_lookup(&reader, 0x40000, 0x40000 + end, 0x40000, 0,
                                                 &target) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(target == 0x6f8e);
    }
    for (uint32_t at = 0; at < 10; ++at) {
        image.fail_at = at;
        assert(hpclang_rpl_flashptr_table_lookup(&reader, 0x40000, 0x4000f, 0x40000, 0, &target) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(target == 0x6f8e);
    }
    image.fail_at = UINT32_MAX;
    put_word(&image, 0, 0, 5);
    assert(!hpclang_rpl_flashptr_table_lookup(&reader, 0x40000, 0x4000f, 0x40000, 0, &target));
    put_word(&image, 0, 2, 5);
    put_word(&image, 5, 0x80000, 5);
    assert(hpclang_rpl_flashptr_table_lookup(&reader, 0x40000, 0x4000f, 0x40000, 0, &target) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(target == 0x6f8e);
    /* This special format must not bypass ordinary ROM-bank identity checks. */
    assert(hpclang_rpl_flashptr_lookup(&reader, 0x40000, 0, 0, &target) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(target == 0x6f8e);
}

static void test_flashptr_resolution(void)
{
    struct image source = {.count = 12, .fail_at = UINT32_MAX};
    struct image table = {.count = 49, .fail_at = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &source};
    const struct hpclang_rpl_reader mapped = {mapped_nibble, &table};
    put_word(&source, 0, 0x26ac, 5);
    put_word(&source, 5, 6, 3);
    put_word(&source, 8, 1, 4);
    struct hpclang_rpl_flashptr pointer = {99, 99};
    assert(hpclang_rpl_flashptr_decode(&reader, 0, 12, &pointer) == 1);
    assert(pointer.selector == 6 && pointer.index == 1);
    const struct hpclang_rpl_flashptr before = pointer;
    for (unsigned i = 0; i < 12; ++i) {
        assert(hpclang_rpl_flashptr_decode(&reader, 0, i, &pointer) == HPCLANG_RPL_SPAN_BOUNDS);
        source.fail_at = i;
        assert(hpclang_rpl_flashptr_decode(&reader, 0, 12, &pointer) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&before, &pointer, sizeof pointer));
    }
    source.fail_at = UINT32_MAX;
    put_word(&table, 0, 0x60f, 4);
    put_word(&table, 29, 3, 5);
    put_word(&table, 34, 0x6f8e, 5);
    put_word(&table, 39, 0x4f622, 5);
    put_word(&table, 44, 0, 5);
    uint32_t target = 0xaaaaa;
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                       &target) == 1);
    assert(target == 0x4f622);
    pointer.index = 0;
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                       &target) == 1);
    assert(target == 0x6f8e);
    pointer.index = 2;
    assert(
        !hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index, &target));
    pointer.index = 3;
    assert(
        !hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index, &target));
    assert(target == 0x6f8e);
    pointer = before;
    for (unsigned i = 0; i < 49; ++i) {
        if (!(i < 4 || (i >= 29 && i < 34) || (i >= 39 && i < 44)))
            continue;
        table.fail_at = i;
        assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                           &target) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(target == 0x6f8e);
    }
    table.fail_at = UINT32_MAX;
    put_word(&table, 0, 0x6f0, 4);
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                       &target) == HPCLANG_RPL_SPAN_MALFORMED);
    put_word(&table, 0, 0x50f, 4);
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                       &target) == HPCLANG_RPL_SPAN_MALFORMED);
    put_word(&table, 0, 0x60f, 4);
    put_word(&table, 29, 0xfffff, 5);
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                       &target) == HPCLANG_RPL_SPAN_BOUNDS);
    put_word(&table, 29, 3, 5);
    put_word(&table, 39, 0x80000, 5);
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                       &target) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x7ffff, pointer.selector, pointer.index,
                                       &target) == HPCLANG_RPL_SPAN_BOUNDS);
    pointer.selector = 16;
    assert(hpclang_rpl_flashptr_lookup(&mapped, 0x40000, pointer.selector, pointer.index,
                                       &target) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(target == 0x6f8e);
}

static void test_acptr_resolution(void)
{
    struct image rom = {.count = 125, .fail_at = UINT32_MAX};
    struct image source = {.count = 15, .fail_at = UINT32_MAX};
    const struct hpclang_rpl_reader tables = {read_nibble, &rom};
    const struct hpclang_rpl_reader reader = {read_nibble, &source};
    for (uint32_t i = 0; i < 16; ++i)
        put_word(&rom, i * 7u, 0x12345u + i * i, 5);
    put_word(&rom, 120, 0x45678u, 5);
    put_word(&source, 0, 0x02baau, 5);
    const uint32_t pointers[] = {0x40000u, 0x7327eu, 0x7ffffu};
    struct hpclang_rpl_banked_target target;
    for (uint32_t bank = 0; bank <= 16; ++bank) {
        put_word(&source, 10, bank < 16 ? 0x12345u + bank * bank : 0x45678u, 5);
        for (unsigned i = 0; i < sizeof pointers / sizeof pointers[0]; ++i) {
            put_word(&source, 5, pointers[i], 5);
            assert(hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target) == 1);
            assert(target.bank.kind ==
                   (bank < 16 ? HPCLANG_RPL_BANK_FLASH : HPCLANG_RPL_BANK_ERAM));
            assert(target.bank.index == (bank < 16 ? bank : 0));
            assert(target.offset == pointers[i] - 0x40000u);
        }
    }
    const struct hpclang_rpl_banked_target before = target;
    for (unsigned limit = 0; limit < 15; ++limit) {
        assert(hpclang_rpl_acptr_resolve(&reader, 0, limit, &tables, 0, 120, &target) ==
               HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&target, &before, sizeof target));
    }
    for (unsigned at = 0; at < 15; ++at) {
        source.fail_at = at;
        assert(hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&target, &before, sizeof target));
    }
    source.fail_at = UINT32_MAX;
    const uint32_t invalid[] = {0, 0x3ffffu, 0x80000u, 0xfffffu};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        put_word(&source, 5, invalid[i], 5);
        assert(hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target) ==
               HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&target, &before, sizeof target));
    }
    put_word(&source, 5, 0x50000u, 5);
    put_word(&source, 10, 0x99999u, 5);
    assert(!hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target));
    assert(!memcmp(&target, &before, sizeof target));
    put_word(&source, 10, 0x45678u, 5);
    rom.fail_at = 120;
    assert(hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&target, &before, sizeof target));
    rom.fail_at = UINT32_MAX;
    put_word(&rom, 0, 0x45678u, 5);
    assert(hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&target, &before, sizeof target));
    source.data[0] = 16;
    assert(hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&target, &before, sizeof target));
    put_word(&source, 0, 0x2614u, 5);
    assert(hpclang_rpl_acptr_resolve(&reader, 0, 15, &tables, 0, 120, &target) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&target, &before, sizeof target));
}

int main(void)
{
    test_acptr_resolution();
    test_flashptr_resolution();
    test_flashptr_bank();
    test_flashptr_special_table();
    test_bank_lookup();
    test_library_command_resolution();
    test_library_catalog_lookup();
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
