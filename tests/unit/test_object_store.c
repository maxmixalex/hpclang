#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <hpclang/target.h>

#include "saturn-view.h"

enum
{
    NIBBLES = 0x100000u,
    USEROB = 0x80711u,
    CONTEXT = 0x8071bu,
    DORRP = 0x02a96u,
    DOCHAR = 0x029bfu,
    DOCSTR = 0x02a2cu
};

static uint8_t memory[NIBBLES];
static uint32_t writes;

static struct hpclang_rpl_memory_bounds fake_bounds = {
    .magic = HPCLANG_RPL_BOUNDS_MAGIC,
    .temp_limit = 0x86000u,
    .user_limit = 0x88000u,
    .stack_reserve_nibbles = 0u,
};

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

int test_saturn_write_nibble(struct hpclang_saturn_address address, uint8_t value)
{
    if (fake_boot.saturn_state == 0u || address.nibble >= NIBBLES || value > 0x0fu)
        return 0;
    memory[address.nibble] = value;
    ++writes;
    return 1;
}

int hpclang_saturn_read_nibble(struct hpclang_saturn_address address, uint8_t *value)
{
    if (value == NULL || fake_boot.saturn_state == 0u || address.nibble >= NIBBLES)
        return 0;
    *value = memory[address.nibble] & 0x0fu;
    return 1;
}

static void put_nibbles(uint32_t address, uint32_t value, uint32_t count)
{
    assert(address + count <= NIBBLES);
    for (uint32_t i = 0u; i < count; ++i)
        memory[address + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static uint32_t put_item(uint32_t item, const uint8_t *name, uint32_t name_length,
                         uint32_t prologue)
{
    assert((name != NULL || name_length == 0u) && name_length < 256u);

    put_nibbles(item, name_length, 2u);
    for (uint32_t i = 0u; i < name_length; ++i)
        put_nibbles(item + 2u + i * 2u, name[i], 2u);

    uint32_t object = item + 2u;
    if (name_length != 0u) {
        uint32_t trailing = item + 2u + name_length * 2u;
        put_nibbles(trailing, name_length, 2u);
        object = trailing + 2u;
    }
    put_nibbles(object, prologue, 5u);
    return object;
}

static void prepare_tree(void)
{
    static const uint8_t name_a[] = {'A'};
    static const uint8_t name_sub[] = {'S', 'U', 'B'};
    static const uint8_t name_txt[] = {'T', 'X', 'T'};

    memset(memory, 0, sizeof(memory));
    fake_boot.saturn_state = 1u;
    fake_boot.platform_cookie = (uintptr_t)&fake_bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
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

    uint32_t object_a = put_item(item_a, name_a, sizeof(name_a), DOCHAR);
    put_nibbles(object_a + 5u, 'Z', 2u);
    put_nibbles(item_a - 5u, item_a - item_sub - 5u, 5u);

    uint32_t object_sub = put_item(item_sub, name_sub, sizeof(name_sub), DORRP);
    put_nibbles(item_sub - 5u, 0u, 5u);
    put_nibbles(CONTEXT, object_sub, 5u);

    const uint32_t sub_chain = object_sub + 8u;
    const uint32_t item_txt = 0x82180u;
    put_nibbles(sub_chain, item_txt - sub_chain, 5u);

    uint32_t object_txt = put_item(item_txt, name_txt, sizeof(name_txt), DOCSTR);
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
    assert(hpclang_object_store_get(home, (const uint8_t *)"A", 1u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_CHARACTER);
    uint8_t value = 0u;
    assert(hpclang_object_character_value(object, &value) == 0);
    assert(value == 'Z');
    hpclang_object_release(object);

    errno = 0;
    assert(hpclang_object_store_get(home, (const uint8_t *)"SUB", 3u, &object) == -1);
    assert(object == NULL);
    assert(errno == EISDIR);

    struct hpclang_object_store *sub =
        hpclang_object_store_open_child(home, (const uint8_t *)"SUB", 3u);
    assert(sub != NULL);

    assert(hpclang_object_store_get(sub, (const uint8_t *)"TXT", 3u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_STRING);
    size_t length = 0u;
    assert(hpclang_object_string_size(object, &length) == 0);
    assert(length == 2u);
    uint8_t text[2] = {0u, 0u};
    assert(hpclang_object_string_copy(object, text, sizeof(text)) == 0);
    assert(text[0] == 'O' && text[1] == 'K');
    hpclang_object_release(object);

    errno = 0;
    assert(hpclang_object_store_get(sub, (const uint8_t *)"NO", 2u, &object) == -1);
    assert(object == NULL);
    assert(errno == ENOENT);

    hpclang_object_store_close(sub);
    hpclang_object_store_close(home);
}

static void test_current_is_logical_path_snapshot(void)
{
    prepare_tree();

    struct hpclang_object_store *current = hpclang_object_store_current();
    assert(current != NULL);

    /* Change Stock cwd after the handle exists. The handle stays at /SUB. */
    put_nibbles(CONTEXT, 0x82000u, 5u);

    struct hpclang_object_store_iterator *iterator = hpclang_object_store_iterator_open(current);
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
    struct hpclang_object_store_iterator *iterator = hpclang_object_store_iterator_open(home);
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

static void test_hidden_directory_logical_handle(void)
{
    memset(memory, 0, sizeof(memory));
    fake_boot.saturn_state = 1u;
    fake_boot.platform_cookie = (uintptr_t)&fake_bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    fake_device.model = HPCLANG_MODEL_HP50G;
    fake_device.family = HPCLANG_FAMILY_HP4950;

    const uint32_t home = 0x82000u;
    const uint32_t home_chain = home + 8u;
    const uint32_t hidden_item = 0x82100u;

    put_nibbles(USEROB, home, 5u);
    put_nibbles(home, DORRP, 5u);
    put_nibbles(home + 5u, 0u, 3u);
    put_nibbles(home_chain, hidden_item - home_chain, 5u);

    uint32_t hidden = put_item(hidden_item, NULL, 0u, DORRP);
    assert(hidden == hidden_item + 2u);
    put_nibbles(hidden_item - 5u, 0u, 5u);
    put_nibbles(hidden + 8u, 0u, 5u);
    put_nibbles(CONTEXT, hidden, 5u);

    struct hpclang_object_store *current = hpclang_object_store_current();
    assert(current != NULL);

    /* The logical path must resolve its zero-length component later. */
    put_nibbles(CONTEXT, home, 5u);
    struct hpclang_object_store_iterator *iterator = hpclang_object_store_iterator_open(current);
    assert(iterator != NULL);

    struct hpclang_object_store_entry entry = blank_entry();
    assert(hpclang_object_store_iterator_read(iterator, &entry) == 0);

    hpclang_object_store_iterator_close(iterator);
    hpclang_object_store_close(current);

    struct hpclang_object_store *home_store = hpclang_object_store_home();
    assert(home_store != NULL);
    struct hpclang_object_store *hidden_store =
        hpclang_object_store_open_child(home_store, NULL, 0u);
    assert(hidden_store != NULL);
    hpclang_object_store_close(hidden_store);
    hpclang_object_store_close(home_store);
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

static void test_public_mutation_modes(void)
{
    prepare_tree();
    struct hpclang_object_store *home = hpclang_object_store_home();
    struct hpclang_object_store *sub =
        hpclang_object_store_open_child(home, (const uint8_t *)"SUB", 3u);
    struct hpclang_object *character = NULL, *string = NULL, *snapshot = NULL;
    assert(hpclang_object_store_get(home, (const uint8_t *)"A", 1u, &character) == 0);
    assert(hpclang_object_store_get(sub, (const uint8_t *)"TXT", 3u, &string) == 0);
    assert(hpclang_object_store_put(home, (const uint8_t *)"SUB", 3u, character,
                                    HPCLANG_OBJECT_STORE_UPSERT) == -1 &&
           errno == EISDIR);
    assert(hpclang_object_store_remove(home, (const uint8_t *)"SUB", 3u) == -1 && errno == EISDIR);
    hpclang_object_store_close(sub);
    memset(memory, 0, sizeof(memory));
    put_nibbles(USEROB, 0x92000u, 5u);
    put_nibbles(CONTEXT, 0x92000u, 5u);
    put_nibbles(0x92000u, DORRP, 5u);
    put_nibbles(0x806eeu, 0x84000u, 5u);
    put_nibbles(0x806f3u, 0x85000u, 5u);
    put_nibbles(0x806f8u, 0x90000u, 5u);
    put_nibbles(0x806fdu, 0x90005u, 5u);
    put_nibbles(0x80e9bu, (0x90000u - 0x85000u) / 5u, 5u);
    const uint8_t name[] = "VALUE";
    assert(hpclang_object_store_put(home, name, 5u, character,
                                    (enum hpclang_object_store_put_mode)3) == -1 &&
           errno == EINVAL);
    assert(hpclang_object_store_put(home, name, 5u, character, HPCLANG_OBJECT_STORE_REPLACE_ONLY) ==
               -1 &&
           errno == ENOENT);
    assert(hpclang_object_store_put(home, name, 5u, character, HPCLANG_OBJECT_STORE_UPSERT) == 0);
    assert(hpclang_object_store_put(home, name, 5u, string, HPCLANG_OBJECT_STORE_CREATE_ONLY) ==
               -1 &&
           errno == EEXIST);
    assert(hpclang_object_store_put(home, name, 5u, string, HPCLANG_OBJECT_STORE_UPSERT) == 0);
    assert(hpclang_object_store_get(home, name, 5u, &snapshot) == 0);
    assert(hpclang_object_store_put(home, name, 5u, character, HPCLANG_OBJECT_STORE_REPLACE_ONLY) ==
           0);
    assert(hpclang_object_store_remove(home, name, 5u) == 0);
    assert(hpclang_object_store_remove(home, name, 5u) == -1 && errno == ENOENT);
    uint8_t bytes[2];
    assert(hpclang_object_string_copy(snapshot, bytes, sizeof(bytes)) == 0 &&
           memcmp(bytes, "OK", 2u) == 0);
    assert(hpclang_object_store_put(home, name, 5u, string, HPCLANG_OBJECT_STORE_CREATE_ONLY) == 0);
    assert(hpclang_object_store_remove(home, name, 5u) == 0);
    hpclang_object_release(snapshot);
    hpclang_object_release(string);
    hpclang_object_release(character);
    hpclang_object_store_close(home);
}

static uint32_t word5(uint32_t address)
{
    uint32_t value = 0u;
    for (uint32_t i = 0; i < 5u; ++i)
        value |= (uint32_t)memory[address + i] << (i * 4u);
    return value;
}

static void test_public_directory_lifecycle(void)
{
    fake_boot.saturn_state = 1u;
    fake_boot.platform_cookie = (uintptr_t)&fake_bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    fake_device.model = HPCLANG_MODEL_HP50G;
    fake_device.family = HPCLANG_FAMILY_HP4950;
    memset(memory, 0, sizeof(memory));
    put_nibbles(USEROB, 0x92000u, 5u);
    put_nibbles(CONTEXT, 0x92000u, 5u);
    put_nibbles(0x92000u, DORRP, 5u);
    put_nibbles(0x806eeu, 0x84000u, 5u);
    put_nibbles(0x806f3u, 0x85000u, 5u);
    put_nibbles(0x806f8u, 0x90000u, 5u);
    put_nibbles(0x806fdu, 0x9000au, 5u); /* one existing stack reference */
    put_nibbles(0x90000u, 0x82000u, 5u);
    put_nibbles(0x80e9bu, (0x90000u - 0x85000u) / 5u, 5u);
    struct hpclang_object_store *home = hpclang_object_store_home();
    assert(home != NULL);
    uint32_t fence = fake_bounds.user_limit;
    fake_bounds.user_limit = word5(0x806f8u);
    writes = 0u;
    assert(hpclang_object_store_mkdir(home, (const uint8_t *)"D", 1u) == -1 && errno == ENOSPC);
    assert(writes == 0u);
    fake_bounds.user_limit = fence;
    assert(hpclang_object_store_mkdir(home, (const uint8_t *)"D", 1u) == 0);
    assert(hpclang_object_store_mkdir(home, (const uint8_t *)"D", 1u) == -1 && errno == EEXIST);
    struct hpclang_object_store *child =
        hpclang_object_store_open_child(home, (const uint8_t *)"D", 1u);
    assert(child != NULL);
    assert(hpclang_object_store_mkdir(child, (const uint8_t *)"E", 1u) == 0);
    struct hpclang_object_store *nested =
        hpclang_object_store_open_child(child, (const uint8_t *)"E", 1u);
    assert(nested != NULL);
    struct hpclang_object *value = NULL;
    assert(hpclang_object_create_string((const uint8_t *)"saved", 5u, &value) == 0);
    assert(value != NULL);
    assert(hpclang_object_store_put(nested, (const uint8_t *)"V", 1u, value,
                                    HPCLANG_OBJECT_STORE_CREATE_ONLY) == 0);
    assert(hpclang_object_store_rmdir(nested, (const uint8_t *)"V", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_TREE) == -1 &&
           errno == ENOTDIR);
    assert(hpclang_object_store_rmdir(home, (const uint8_t *)"D", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_EMPTY) == -1 &&
           errno == ENOTEMPTY);
    assert(hpclang_object_store_remove(home, (const uint8_t *)"D", 1u) == -1 && errno == EISDIR);

    /* Find nested objects through the live native links, then keep an interior
     * value reference on the Stock stack across recursive removal. */
    uint32_t h = word5(USEROB);
    uint32_t d = h + 8u + word5(h + 8u) + 6u;
    uint32_t e = d + 8u + word5(d + 8u) + 6u;
    uint32_t v = e + 8u + word5(e + 8u) + 6u;
    put_nibbles(word5(0x806f8u), v, 5u);
    put_nibbles(CONTEXT, e, 5u);
    writes = 0u;
    assert(hpclang_object_store_rmdir(home, (const uint8_t *)"D", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_TREE) == -1 &&
           errno == EBUSY);
    assert(word5(CONTEXT) == e && word5(USEROB) == h && writes == 0u);
    assert(hpclang_object_store_rmdir(child, (const uint8_t *)"E", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_TREE) == -1 &&
           errno == EBUSY);
    put_nibbles(CONTEXT, h, 5u);
    uint32_t temp_limit = fake_bounds.temp_limit;
    fake_bounds.temp_limit = word5(0x806f3u);
    writes = 0u;
    assert(hpclang_object_store_rmdir(home, (const uint8_t *)"D", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_TREE) == -1 &&
           errno == ENOSPC);
    assert(writes == 0u && word5(word5(0x806f8u)) == v);
    fake_bounds.temp_limit = temp_limit;
    /* Corrupt a nested chain link: reject the whole tree before any write. */
    uint32_t saved_link = word5(v - 11u);
    put_nibbles(v - 11u, 0xfffffu, 5u);
    writes = 0u;
    assert(hpclang_object_store_rmdir(home, (const uint8_t *)"D", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_TREE) == -1);
    assert(writes == 0u);
    put_nibbles(v - 11u, saved_link, 5u);
    assert(hpclang_object_store_rmdir(home, (const uint8_t *)"D", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_TREE) == 0);
    assert(word5(CONTEXT) == word5(USEROB));
    uint32_t preserved = word5(word5(0x806f8u));
    assert(preserved != v && word5(preserved) == DOCSTR);
    assert(memory[preserved + 10u] == ('s' & 15) && memory[preserved + 11u] == ('s' >> 4));
    struct hpclang_object *out = NULL;
    assert(hpclang_object_store_get(nested, (const uint8_t *)"V", 1u, &out) == -1 &&
           errno == ENOENT);
    assert(hpclang_object_store_rmdir(home, (const uint8_t *)"D", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_TREE) == -1 &&
           errno == ENOENT);
    /* Logical handles resolve the same name again after recreation. */
    assert(hpclang_object_store_mkdir(home, (const uint8_t *)"D", 1u) == 0);
    assert(hpclang_object_store_mkdir(child, NULL, 0u) == 0);
    assert(hpclang_object_store_rmdir(child, NULL, 0u, HPCLANG_OBJECT_STORE_REMOVE_EMPTY) == 0);
    assert(hpclang_object_store_rmdir(home, (const uint8_t *)"D", 1u,
                                      HPCLANG_OBJECT_STORE_REMOVE_EMPTY) == 0);
    hpclang_object_release(value);
    hpclang_object_store_close(nested);
    hpclang_object_store_close(child);
    hpclang_object_store_close(home);
}

int main(void)
{
    test_home_lookup_and_child();
    test_current_is_logical_path_snapshot();
    test_iterator_snapshots_metadata();
    test_hidden_directory_logical_handle();
    test_unsupported_environments();
    test_public_mutation_modes();
    test_public_directory_lifecycle();
    return 0;
}
