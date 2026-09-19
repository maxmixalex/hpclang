#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <hpclang/target.h>

#include "saturn-view.h"

enum {
    NIBBLES = 0x100000u,
    USEROB = 0x80711u,
    CONTEXT = 0x8071bu,
    DORRP = 0x02a96u,
    DOCHAR = 0x029bfu,
    DOCSTR = 0x02a2cu
};

static uint8_t memory[NIBBLES];

static struct hpclang_boot_info fake_boot = {
    .model = HPCLANG_MODEL_HP50G,
    .saturn_state = 1u,
};

static struct hpclang_device_info fake_device = {
    .size = sizeof(struct hpclang_device_info),
    .version = HPCLANG_DEVICE_INFO_VERSION,
    .model = HPCLANG_MODEL_HP50G,
    .family = HPCLANG_FAMILY_HP4950,
    .display_width = 131u,
    .display_height = 80u,
};

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &fake_boot;
}

const struct hpclang_device_info *hpclang_device_info(void)
{
    return &fake_device;
}

int hpclang_saturn_read_nibble(
    struct hpclang_saturn_address address,
    uint8_t *value)
{
    if (value == NULL ||
        fake_boot.saturn_state == 0u ||
        address.nibble >= NIBBLES)
        return 0;
    *value = memory[address.nibble] & 0x0fu;
    return 1;
}

static void put_nibbles(uint32_t address, uint32_t value, uint32_t count)
{
    assert(address + count <= NIBBLES);
    for (uint32_t i = 0u; i < count; ++i)
        memory[address + i] =
            (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static uint32_t put_item(
    uint32_t item,
    const uint8_t *name,
    uint32_t name_length,
    uint32_t prologue)
{
    assert(name != NULL);
    assert(name_length < 256u);

    put_nibbles(item, name_length, 2u);
    for (uint32_t i = 0u; i < name_length; ++i)
        put_nibbles(item + 2u + i * 2u, name[i], 2u);

    uint32_t trailing = item + 2u + name_length * 2u;
    put_nibbles(trailing, name_length, 2u);
    uint32_t object = trailing + 2u;
    put_nibbles(object, prologue, 5u);
    return object;
}

static void prepare_tree(void)
{
    static const uint8_t name_a[] = { 'A' };
    static const uint8_t name_sub[] = { 'S', 'U', 'B' };
    static const uint8_t name_txt[] = { 'T', 'X', 'T' };

    memset(memory, 0, sizeof(memory));
    fake_boot.saturn_state = 1u;
    fake_device.model = HPCLANG_MODEL_HP50G;
    fake_device.family = HPCLANG_FAMILY_HP4950;

    const uint32_t home = 0x82000u;
    const uint32_t home_chain = home + 8u;
    const uint32_t item_sub = 0x82100u;
    const uint32_t item_a = 0x82200u;

    put_nibbles(USEROB, home, 5u);
    put_nibbles(home, DORRP, 5u);
    put_nibbles(home + 5u, 0u, 3u);
    put_nibbles(home_chain, item_a - home_chain, 5u);

    uint32_t object_a =
        put_item(item_a, name_a, sizeof(name_a), DOCHAR);
    put_nibbles(object_a + 5u, 'Z', 2u);
    put_nibbles(item_a - 5u, item_a - item_sub - 5u, 5u);

    uint32_t object_sub =
        put_item(item_sub, name_sub, sizeof(name_sub), DORRP);
    put_nibbles(item_sub - 5u, 0u, 5u);
    put_nibbles(CONTEXT, object_sub, 5u);

    const uint32_t sub_chain = object_sub + 8u;
    const uint32_t item_txt = 0x82180u;
    put_nibbles(sub_chain, item_txt - sub_chain, 5u);

    uint32_t object_txt =
        put_item(item_txt, name_txt, sizeof(name_txt), DOCSTR);
    put_nibbles(object_txt + 5u, 9u, 5u);
    put_nibbles(object_txt + 10u, 'O', 2u);
    put_nibbles(object_txt + 12u, 'K', 2u);
    put_nibbles(item_txt - 5u, 0u, 5u);
}

static struct hpclang_object_store_entry blank_entry(void)
{
    const struct hpclang_object_store_entry entry = {
        .size = sizeof(struct hpclang_object_store_entry),
        .version = HPCLANG_OBJECT_STORE_ENTRY_VERSION,
    };
    return entry;
}

static void test_home_lookup_and_child(void)
{
    prepare_tree();

    struct hpclang_object_store *home = hpclang_object_store_home();
    assert(home != NULL);

    struct hpclang_object *object = NULL;
    assert(hpclang_object_store_get(
               home, (const uint8_t *)"A", 1u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_CHARACTER);
    uint8_t value = 0u;
    assert(hpclang_object_character_value(object, &value) == 0);
    assert(value == 'Z');
    hpclang_object_release(object);

    errno = 0;
    assert(hpclang_object_store_get(
               home, (const uint8_t *)"SUB", 3u, &object) == -1);
    assert(object == NULL);
    assert(errno == EISDIR);

    struct hpclang_object_store *sub =
        hpclang_object_store_open_child(
            home, (const uint8_t *)"SUB", 3u);
    assert(sub != NULL);

    assert(hpclang_object_store_get(
               sub, (const uint8_t *)"TXT", 3u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_STRING);
    size_t length = 0u;
    assert(hpclang_object_string_size(object, &length) == 0);
    assert(length == 2u);
    uint8_t text[2] = { 0u, 0u };
    assert(hpclang_object_string_copy(object, text, sizeof(text)) == 0);
    assert(text[0] == 'O' && text[1] == 'K');
    hpclang_object_release(object);

    errno = 0;
    assert(hpclang_object_store_get(
               sub, (const uint8_t *)"NO", 2u, &object) == -1);
    assert(object == NULL);
    assert(errno == ENOENT);

    hpclang_object_store_close(sub);
    hpclang_object_store_close(home);
}

static void test_current_is_logical_path_snapshot(void)
{
    prepare_tree();

    struct hpclang_object_store *current =
        hpclang_object_store_current();
    assert(current != NULL);

    /* Change Stock cwd after the handle exists. The handle stays at /SUB. */
    put_nibbles(CONTEXT, 0x82000u, 5u);

    struct hpclang_object_store_iterator *iterator =
        hpclang_object_store_iterator_open(current);
    assert(iterator != NULL);

    struct hpclang_object_store_entry entry = blank_entry();
    assert(hpclang_object_store_iterator_read(iterator, &entry) == 1);
    assert(entry.name_length == 3u);
    assert(memcmp(entry.name, "TXT", 3u) == 0);
    assert(entry.kind == HPCLANG_OBJECT_STRING);
    assert((entry.flags & HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY) == 0u);
    assert(hpclang_object_store_iterator_read(iterator, &entry) == 0);

    hpclang_object_store_iterator_close(iterator);
    hpclang_object_store_close(current);
}

static void test_iterator_snapshots_metadata(void)
{
    prepare_tree();

    struct hpclang_object_store *home = hpclang_object_store_home();
    assert(home != NULL);
    struct hpclang_object_store_iterator *iterator =
        hpclang_object_store_iterator_open(home);
    assert(iterator != NULL);

    /* The iterator owns an ARM-side snapshot; mutate the live first name. */
    put_nibbles(0x82200u + 2u, 'Q', 2u);

    struct hpclang_object_store_entry entry = blank_entry();
    assert(hpclang_object_store_iterator_read(iterator, &entry) == 1);
    assert(entry.name_length == 1u);
    assert(entry.name[0] == 'A');
    assert(entry.kind == HPCLANG_OBJECT_CHARACTER);

    assert(hpclang_object_store_iterator_read(iterator, &entry) == 1);
    assert(entry.name_length == 3u);
    assert(memcmp(entry.name, "SUB", 3u) == 0);
    assert(entry.kind == HPCLANG_OBJECT_DIRECTORY);
    assert((entry.flags & HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY) != 0u);
    assert(hpclang_object_store_iterator_read(iterator, &entry) == 0);

    hpclang_object_store_iterator_close(iterator);
    hpclang_object_store_close(home);
}

static void test_unsupported_environments(void)
{
    prepare_tree();

    fake_device.model = HPCLANG_MODEL_HP39GS;
    fake_device.family = HPCLANG_FAMILY_HP39;
    errno = 0;
    assert(hpclang_object_store_home() == NULL);
    assert(errno == ENOSYS);

    fake_device.model = HPCLANG_MODEL_HP50G;
    fake_device.family = HPCLANG_FAMILY_HP4950;
    fake_boot.saturn_state = 0u;
    errno = 0;
    assert(hpclang_object_store_current() == NULL);
    assert(errno == ENOSYS);
}

int main(void)
{
    test_home_lookup_and_child();
    test_current_is_logical_path_snapshot();
    test_iterator_snapshots_metadata();
    test_unsupported_environments();
    return 0;
}
