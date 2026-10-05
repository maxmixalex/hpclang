#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/catalog.h>
#include "hp39-catalog-service.h"
#include "object-snapshot.h"

static int available = 1, present[2], writes, fail_write;
static uint8_t stored[2][32];
static size_t lengths[2];

static int list(struct hpclang_object **object)
{
    *object = NULL;
    if (!available) {
        errno = ENOSYS;
        return -1;
    }
    static const uint8_t empty[] = {0x74, 0x2a, 0xb0, 0x12, 0x03};
    *object = malloc(sizeof(**object) + sizeof(empty));
    assert(*object != NULL);
    (*object)->kind = HPCLANG_OBJECT_LIST;
    (*object)->native_prologue = 0x2a74u;
    (*object)->nibble_count = 10u;
    memcpy((*object)->data, empty, sizeof(empty));
    return 0;
}

int hpclang_hp39_program_catalog_snapshot(struct hpclang_object **object)
{
    return list(object);
}

int hpclang_hp39_note_catalog_snapshot(struct hpclang_object **object)
{
    return list(object);
}

static int get(unsigned index, const uint8_t *name, size_t length, struct hpclang_object **object)
{
    assert(name != NULL && length > 0u && length <= 19u);
    *object = NULL;
    if (!available || !present[index]) {
        errno = available ? ENOENT : ENOSYS;
        return -1;
    }
    return hpclang_object_create_string(stored[index], lengths[index], object);
}

int hpclang_hp39_program_body_snapshot(const uint8_t *name, size_t length,
                                       struct hpclang_object **object)
{
    return get(0u, name, length, object);
}

int hpclang_hp39_note_body_snapshot(const uint8_t *name, size_t length,
                                    struct hpclang_object **object)
{
    return get(1u, name, length, object);
}

int hpclang_hp39_catalog_edit(enum hpclang_catalog catalog, const uint8_t *name, uint32_t length,
                              const struct hpclang_object *body)
{
    assert(available && name != NULL && length > 0u && length <= 19u);
    unsigned index = (unsigned)catalog - 1u;
    assert(index < 2u);
    if (fail_write) {
        errno = ENOMEM;
        return -1;
    }
    if (body != NULL) {
        assert(hpclang_object_string_size(body, &lengths[index]) == 0);
        assert(hpclang_object_string_copy(body, stored[index], sizeof(stored[index])) == 0);
    }
    present[index] = body != NULL;
    ++writes;
    return 0;
}

int main(void)
{
    const uint8_t name[] = "TEST";
    struct hpclang_object *body = NULL, *value = NULL, *old = NULL;
    assert(hpclang_object_create_string((const uint8_t *)"old", 3u, &body) == 0);
    for (enum hpclang_catalog c = HPCLANG_CATALOG_PROGRAM; c <= HPCLANG_CATALOG_NOTE; ++c) {
        assert(hpclang_catalog_put(c, name, 4u, body, HPCLANG_OBJECT_STORE_REPLACE_ONLY) == -1);
        assert(errno == ENOENT);
        assert(hpclang_catalog_put(c, name, 4u, body, HPCLANG_OBJECT_STORE_CREATE_ONLY) == 0);
        assert(hpclang_catalog_get(c, name, 4u, &old) == 0);
        int before = writes;
        assert(hpclang_catalog_put(c, name, 4u, body, HPCLANG_OBJECT_STORE_CREATE_ONLY) == -1);
        assert(errno == EEXIST && writes == before);
        assert(hpclang_object_create_string((const uint8_t *)"replacement", 11u, &value) == 0);
        fail_write = 1;
        assert(hpclang_catalog_put(c, name, 4u, value, HPCLANG_OBJECT_STORE_UPSERT) == -1);
        assert(errno == ENOMEM && writes == before && lengths[c - 1] == 3u);
        fail_write = 0;
        assert(hpclang_catalog_put(c, name, 4u, value, HPCLANG_OBJECT_STORE_REPLACE_ONLY) == 0);
        hpclang_object_release(value);
        value = NULL;
        assert(hpclang_catalog_get(c, name, 4u, &value) == 0);
        size_t length;
        assert(hpclang_object_string_size(value, &length) == 0 && length == 11u);
        assert(hpclang_object_string_size(old, &length) == 0 && length == 3u);
        assert(hpclang_catalog_remove(c, name, 4u) == 0);
        assert(hpclang_catalog_remove(c, name, 4u) == -1 && errno == ENOENT);
        hpclang_object_release(value);
        hpclang_object_release(old);
        value = old = NULL;
        assert(hpclang_catalog_get(c, name, 4u, &value) == -1 && value == NULL);
        assert(hpclang_catalog_list(c, &value) == 0 && value->kind == HPCLANG_OBJECT_LIST);
        before = writes;
        assert(hpclang_catalog_put(c, name, 4u, value, HPCLANG_OBJECT_STORE_UPSERT) == -1);
        assert(errno == EINVAL && writes == before);
        hpclang_object_release(value);
        value = NULL;
    }
    int before = writes;
    assert(hpclang_catalog_put(HPCLANG_CATALOG_PROGRAM, (const uint8_t *)"Editline", 8u, body,
                               HPCLANG_OBJECT_STORE_UPSERT) == -1 &&
           errno == EPERM);
    assert(hpclang_catalog_remove(HPCLANG_CATALOG_PROGRAM, (const uint8_t *)"Editline", 8u) == -1 &&
           errno == EPERM);
    assert(hpclang_catalog_put(HPCLANG_CATALOG_PROGRAM, name, 0u, body,
                               HPCLANG_OBJECT_STORE_UPSERT) == -1 &&
           errno == EINVAL);
    assert(hpclang_catalog_get(HPCLANG_CATALOG_NOTE, name, 20u, &value) == -1);
    assert(hpclang_catalog_get(HPCLANG_CATALOG_NOTE, NULL, 1u, &value) == -1);
    assert(hpclang_catalog_get(HPCLANG_CATALOG_NOTE, name, 4u, NULL) == -1);
    assert(hpclang_catalog_list((enum hpclang_catalog)0, &value) == -1);
    assert(hpclang_catalog_list(HPCLANG_CATALOG_NOTE, NULL) == -1);
    assert(hpclang_catalog_put(HPCLANG_CATALOG_NOTE, name, 4u, body,
                               (enum hpclang_object_store_put_mode)9) == -1);
    assert(writes == before);
    available = 0;
    assert(hpclang_catalog_get(HPCLANG_CATALOG_NOTE, name, 4u, &value) == -1 && errno == ENOSYS);
    assert(hpclang_catalog_put(HPCLANG_CATALOG_NOTE, name, 4u, body, HPCLANG_OBJECT_STORE_UPSERT) ==
               -1 &&
           errno == ENOSYS);
    assert(hpclang_catalog_remove(HPCLANG_CATALOG_NOTE, name, 4u) == -1 && errno == ENOSYS);
    assert(hpclang_catalog_list(HPCLANG_CATALOG_NOTE, &value) == -1 && errno == ENOSYS);
    assert(value == NULL && writes == before);
    hpclang_object_release(body);
    return 0;
}
