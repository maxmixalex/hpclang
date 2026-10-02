#include <stdint.h>
#include <string.h>
#include <hpclang/object.h>
#include <hpclang/object-store.h>
#include "rpl-object-store.h"
#include "rpl-stock-memory.h"

int hpclang_object_store_replace_existing(const struct hpclang_object_store *, const uint8_t *,
                                          uint32_t, const struct hpclang_object *);
int hpclang_object_store_create_new(const struct hpclang_object_store *, const uint8_t *, uint32_t,
                                    const struct hpclang_object *);
int hpclang_object_store_remove_existing(const struct hpclang_object_store *, const uint8_t *,
                                         uint32_t);

static int matches(const struct hpclang_object *object, const char *expected)
{
    uint8_t bytes[32];
    size_t size = 0;
    return object != NULL && hpclang_object_string_size(object, &size) == 0 &&
           size == strlen(expected) && size <= sizeof(bytes) &&
           hpclang_object_string_copy(object, bytes, size) == 0 &&
           memcmp(bytes, expected, size) == 0;
}

static int named_matches(const struct hpclang_object_store *store, const char *name,
                         const char *expected)
{
    struct hpclang_object *object = NULL;
    int ok = hpclang_object_store_get(store, (const uint8_t *)name, (uint32_t)strlen(name),
                                      &object) == 0 &&
             matches(object, expected);
    hpclang_object_release(object);
    return ok;
}

/* Test-only alias construction: reserve a cell through the public stack API,
 * then point it at the named Stock object. This exercises real Stock roots,
 * without adding a borrowed-pointer API to the SDK. Resolve after allocation. */
static int alias_value(void)
{
    struct hpclang_rpl_stock_memory memory;
    struct hpclang_rpl_mutation_io io;
    if (hpclang_rpl_stock_mutation_io(&io, &memory) == 0)
        return -1;
    struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
    uint32_t address = 0;
    if (hpclang_rpl_store_home_object(&reader, &address) != 0)
        return -1;
    const char *path[] = {"ZZM7DIR", "INNER", "VALUE"};
    for (uint32_t i = 0; i < 3; ++i) {
        struct hpclang_rpl_directory_entry_view entry;
        if (hpclang_rpl_directory_find(&reader, address, (const uint8_t *)path[i],
                                       (uint32_t)strlen(path[i]), &entry) != 0)
            return -1;
        address = entry.object_address;
    }
    uint32_t top = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (io.read_nibble(io.context, 0x806f8u + i, &nibble) == 0)
            return -1;
        top |= (uint32_t)nibble << (4u * i);
    }
    for (uint32_t i = 0; i < 5; ++i)
        if (io.write_nibble(io.context, top + i, (uint8_t)((address >> (4u * i)) & 15u)) == 0)
            return -1;
    return 0;
}

uint32_t hpclang_e2e_nested_mutation(void)
{
    struct hpclang_object_store *home = hpclang_object_store_home();
    struct hpclang_object_store *root =
        hpclang_object_store_open_child(home, (const uint8_t *)"ZZM7DIR", 7);
    struct hpclang_object_store *inner =
        hpclang_object_store_open_child(root, (const uint8_t *)"INNER", 5);
    struct hpclang_object *original = NULL, *longer = NULL, *retained = NULL;
    uint32_t failure = 0x100u, depth = 0, final_depth = 0;
    int pushed = 0;
    if (inner == NULL || hpclang_stack_depth(&depth) != 0 ||
        hpclang_object_store_get(inner, (const uint8_t *)"VALUE", 5, &original) != 0 ||
        !matches(original, "NESTED") ||
        hpclang_object_create_string((const uint8_t *)"NESTED-LONGER", 13, &longer) != 0)
        goto done;
    for (uint32_t operation = 0; operation < 2; ++operation) {
        failure = 0x101u + operation * 4u;
        if (hpclang_stack_push(original) != 0)
            goto done;
        pushed = 1;
        if (alias_value() != 0)
            goto done;
        ++failure;
        int status =
            operation == 0
                ? hpclang_object_store_replace_existing(inner, (const uint8_t *)"VALUE", 5, longer)
                : hpclang_object_store_remove_existing(inner, (const uint8_t *)"VALUE", 5);
        if (status != 0)
            goto done;
        ++failure;
        if (hpclang_stack_peek(1, &retained) != 0 || !matches(retained, "NESTED"))
            goto done;
        hpclang_object_release(retained);
        retained = NULL;
        if (hpclang_stack_drop(1) != 0)
            goto done;
        pushed = 0;
        ++failure;
        if (operation == 0 && !named_matches(inner, "VALUE", "NESTED-LONGER"))
            goto done;
        status =
            operation == 0
                ? hpclang_object_store_replace_existing(inner, (const uint8_t *)"VALUE", 5,
                                                        original)
                : hpclang_object_store_create_new(inner, (const uint8_t *)"VALUE", 5, original);
        if (status != 0 || !named_matches(inner, "VALUE", "NESTED"))
            goto done;
    }
    failure = 0x109u;
    if (!named_matches(inner, "KEEP", "KEEP") || !named_matches(root, "OUTSIDE", "OUTSIDE") ||
        hpclang_stack_depth(&final_depth) != 0 || final_depth != depth)
        goto done;
    failure = 0;
done:
    if (pushed)
        (void)hpclang_stack_drop(1);
    hpclang_object_release(retained);
    hpclang_object_release(longer);
    hpclang_object_release(original);
    hpclang_object_store_close(inner);
    hpclang_object_store_close(root);
    hpclang_object_store_close(home);
    return failure;
}
