#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "rpl-object-store.h"

enum {
    NIBBLES = 0x100000u,
    USEROB = 0x80711u,
    CONTEXT = 0x8071bu,
    DORRP = 0x02a96u,
    DOCHAR = 0x029bfu,
    DOCSTR = 0x02a2cu
};

static uint8_t memory[NIBBLES];

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    if (value == NULL || address >= NIBBLES)
        return 0;
    *value = memory[address] & 0x0fu;
    return 1;
}

static const struct hpclang_rpl_reader reader = {
    .read_nibble = read_nibble,
    .context = NULL,
};

static void put_nibbles(uint32_t address, uint32_t value, uint32_t count)
{
    assert(address + count <= NIBBLES);
    for (uint32_t i = 0u; i < count; ++i)
        memory[address + i] =
            (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static uint32_t put_item(
    uint32_t item,
    const char *name,
    uint32_t prologue)
{
    uint32_t length = (uint32_t)strlen(name);
    assert(length < 256u);

    put_nibbles(item, length, 2u);
    for (uint32_t i = 0u; i < length; ++i)
        put_nibbles(item + 2u + i * 2u, (uint8_t)name[i], 2u);

    uint32_t trailing = item + 2u + length * 2u;
    put_nibbles(trailing, length, 2u);
    uint32_t object = trailing + 2u;
    put_nibbles(object, prologue, 5u);
    return object;
}

static void prepare_tree(void)
{
    memset(memory, 0, sizeof(memory));

    const uint32_t home = 0x82000u;
    const uint32_t home_chain = home + 8u; /* no attached libraries */
    const uint32_t item_b = 0x82100u;
    const uint32_t item_a = 0x82200u;

    put_nibbles(USEROB, home, 5u);
    put_nibbles(CONTEXT, 0x8210au, 5u);
    put_nibbles(home, DORRP, 5u);
    put_nibbles(home + 5u, 0u, 3u);
    put_nibbles(home_chain, item_a - home_chain, 5u);

    uint32_t object_a = put_item(item_a, "A", DOCHAR);
    (void)object_a;
    put_nibbles(item_a - 5u, item_a - item_b - 5u, 5u);

    uint32_t object_b = put_item(item_b, "SUB", DORRP);
    assert(object_b == 0x8210au);
    put_nibbles(item_b - 5u, 0u, 5u);

    const uint32_t sub_chain = object_b + 8u;
    const uint32_t item_c = 0x82180u;
    put_nibbles(sub_chain, item_c - sub_chain, 5u);
    uint32_t object_c = put_item(item_c, "TXT", DOCSTR);
    (void)object_c;
    put_nibbles(item_c - 5u, 0u, 5u);
}

static void test_home_and_current(void)
{
    prepare_tree();

    uint32_t object = 0u;
    assert(hpclang_rpl_store_home_object(&reader, &object) ==
           HPCLANG_RPL_STORE_OK);
    assert(object == 0x82000u);

    assert(hpclang_rpl_store_current_object(&reader, &object) ==
           HPCLANG_RPL_STORE_OK);
    assert(object == 0x82108u);

    uint8_t path[64];
    uint32_t bytes = 0u;
    uint32_t components = 0u;
    assert(hpclang_rpl_directory_path_to(
               &reader, object, path, sizeof(path), &bytes, &components) ==
           HPCLANG_RPL_STORE_OK);
    assert(components == 1u);
    assert(bytes == 4u);
    assert(path[0] == 3u);
    assert(memcmp(path + 1u, "SUB", 3u) == 0);
}

static void test_enumeration_and_find(void)
{
    prepare_tree();

    struct hpclang_rpl_directory_cursor cursor;
    assert(hpclang_rpl_directory_cursor_open(
               &reader, 0x82000u, &cursor) ==
           HPCLANG_RPL_STORE_OK);

    struct hpclang_rpl_directory_entry_view entry;
    assert(hpclang_rpl_directory_cursor_next(
               &reader, &cursor, &entry) == 1);
    assert(entry.name_length == 1u);
    assert(entry.name[0] == 'A');
    assert(entry.prologue == DOCHAR);

    assert(hpclang_rpl_directory_cursor_next(
               &reader, &cursor, &entry) == 1);
    assert(entry.name_length == 3u);
    assert(memcmp(entry.name, "SUB", 3u) == 0);
    assert(entry.prologue == DORRP);

    assert(hpclang_rpl_directory_cursor_next(
               &reader, &cursor, &entry) == 0);

    assert(hpclang_rpl_directory_find(
               &reader, 0x82000u,
               (const uint8_t *)"SUB", 3u, &entry) ==
           HPCLANG_RPL_STORE_OK);
    assert(entry.object_address == 0x82108u);

    assert(hpclang_rpl_directory_find(
               &reader, 0x82000u,
               (const uint8_t *)"NO", 2u, &entry) ==
           HPCLANG_RPL_STORE_NOT_FOUND);
}

static void test_malformed_chain(void)
{
    prepare_tree();

    /* First link points forward to A; make A's next link point forward again. */
    put_nibbles(0x82200u - 5u, 0x82200u, 5u);

    struct hpclang_rpl_directory_cursor cursor;
    assert(hpclang_rpl_directory_cursor_open(
               &reader, 0x82000u, &cursor) ==
           HPCLANG_RPL_STORE_OK);

    struct hpclang_rpl_directory_entry_view entry;
    assert(hpclang_rpl_directory_cursor_next(
               &reader, &cursor, &entry) ==
           HPCLANG_RPL_STORE_MALFORMED);
}

int main(void)
{
    test_home_and_current();
    test_enumeration_and_find();
    test_malformed_chain();
    return 0;
}
