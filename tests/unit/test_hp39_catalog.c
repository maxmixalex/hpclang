#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>

#include "hp39-catalog.h"
#include "object-snapshot.h"

enum
{
    RPL_DOLIST = 0x02a74u,
    RPL_DOCSTR = 0x02a2cu,
    RPL_DOIDNT = 0x02e48u,
    RPL_SEMI = 0x0312bu
};

static void put_nibble(struct hpclang_object *object, uint32_t offset, uint8_t value)
{
    assert(object != NULL);
    assert(offset < object->nibble_count);
    assert(value <= 0x0fu);
    uint8_t *byte = &object->data[offset >> 1];
    if ((offset & 1u) != 0u)
        *byte = (uint8_t)((*byte & 0x0fu) | (value << 4));
    else
        *byte = (uint8_t)((*byte & 0xf0u) | value);
}

static void put_word(struct hpclang_object *object, uint32_t offset, uint32_t nibbles,
                     uint32_t value)
{
    for (uint32_t i = 0u; i < nibbles; ++i)
        put_nibble(object, offset + i, (uint8_t)((value >> (4u * i)) & 0x0fu));
}

static void put_bytes(struct hpclang_object *object, uint32_t offset, const char *text)
{
    size_t length = strlen(text);
    for (size_t i = 0u; i < length; ++i)
        put_word(object, offset + (uint32_t)(i * 2u), 2u, (uint8_t)text[i]);
}

static struct hpclang_object *make_catalog(void)
{
    /* DOLIST + ID("ABC") + CSTR("Test") + SEMI = 41 nibbles. */
    const uint32_t nibbles = 41u;
    size_t bytes = (nibbles + 1u) / 2u;
    struct hpclang_object *object = calloc(1u, sizeof(*object) + bytes);
    assert(object != NULL);
    object->kind = HPCLANG_OBJECT_LIST;
    object->nibble_count = nibbles;
    object->native_prologue = RPL_DOLIST;

    put_word(object, 0u, 5u, RPL_DOLIST);

    uint32_t offset = 5u;
    put_word(object, offset, 5u, RPL_DOIDNT);
    put_word(object, offset + 5u, 2u, 3u);
    put_bytes(object, offset + 7u, "ABC");
    offset += 13u;

    put_word(object, offset, 5u, RPL_DOCSTR);
    put_word(object, offset + 5u, 5u, 13u);
    put_bytes(object, offset + 10u, "Test");
    offset += 18u;

    put_word(object, offset, 5u, RPL_SEMI);
    assert(offset + 5u == nibbles);
    return object;
}

static void test_inline_catalog_names(void)
{
    struct hpclang_object *catalog = make_catalog();

    uint32_t count = 0u;
    assert(hpclang_hp39_catalog_name_count(catalog, &count) == 0);
    assert(count == 2u);

    int found = 0;
    assert(hpclang_hp39_catalog_contains_name(catalog, (const uint8_t *)"ABC", 3u, &found) == 0);
    assert(found == 1);

    assert(hpclang_hp39_catalog_contains_name(catalog, (const uint8_t *)"Test", 4u, &found) == 0);
    assert(found == 1);

    assert(hpclang_hp39_catalog_contains_name(catalog, (const uint8_t *)"Missing", 7u, &found) ==
           0);
    assert(found == 0);

    free(catalog);
}

static void test_reference_entry_is_rejected(void)
{
    const uint32_t nibbles = 15u;
    struct hpclang_object *catalog = calloc(1u, sizeof(*catalog) + (nibbles + 1u) / 2u);
    assert(catalog != NULL);
    catalog->kind = HPCLANG_OBJECT_LIST;
    catalog->nibble_count = nibbles;
    catalog->native_prologue = RPL_DOLIST;
    put_word(catalog, 0u, 5u, RPL_DOLIST);
    put_word(catalog, 5u, 5u, 0x90000u);
    put_word(catalog, 10u, 5u, RPL_SEMI);

    uint32_t count = 99u;
    errno = 0;
    assert(hpclang_hp39_catalog_name_count(catalog, &count) == -1);
    assert(count == 0u);
    assert(errno == ENOTSUP);
    free(catalog);
}

static void test_malformed_catalog_is_rejected(void)
{
    struct hpclang_object *catalog = make_catalog();
    catalog->nibble_count -= 1u;

    uint32_t count = 99u;
    errno = 0;
    assert(hpclang_hp39_catalog_name_count(catalog, &count) == -1);
    assert(count == 0u);
    assert(errno == EILSEQ);
    free(catalog);

    errno = 0;
    assert(hpclang_hp39_catalog_name_count(NULL, &count) == -1);
    assert(errno == EINVAL);
}

int main(void)
{
    test_inline_catalog_names();
    test_reference_entry_is_rejected();
    test_malformed_catalog_is_rejected();
    return 0;
}
