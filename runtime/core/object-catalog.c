#include <errno.h>
#include <string.h>

#include <hpclang/catalog.h>

#include "hp39-catalog-service.h"
#include "object-snapshot.h"

static int valid_catalog(enum hpclang_catalog catalog)
{
    if (catalog == HPCLANG_CATALOG_PROGRAM || catalog == HPCLANG_CATALOG_NOTE)
        return 1;
    errno = EINVAL;
    return 0;
}

static int valid_name(enum hpclang_catalog catalog, const uint8_t *name, uint32_t length,
                      int mutation)
{
    if (!valid_catalog(catalog))
        return 0;
    if (name == NULL || length == 0u || length > 19u) {
        errno = EINVAL;
        return 0;
    }
    if (mutation && catalog == HPCLANG_CATALOG_PROGRAM && length == 8u &&
        memcmp(name, "Editline", 8u) == 0) {
        errno = EPERM;
        return 0;
    }
    return 1;
}

int hpclang_catalog_list(enum hpclang_catalog catalog, struct hpclang_object **names)
{
    if (names == NULL) {
        errno = EINVAL;
        return -1;
    }
    *names = NULL;
    if (!valid_catalog(catalog))
        return -1;
    return catalog == HPCLANG_CATALOG_PROGRAM ? hpclang_hp39_program_catalog_snapshot(names)
                                              : hpclang_hp39_note_catalog_snapshot(names);
}

int hpclang_catalog_get(enum hpclang_catalog catalog, const uint8_t *name, uint32_t name_length,
                        struct hpclang_object **body)
{
    if (body == NULL) {
        errno = EINVAL;
        return -1;
    }
    *body = NULL;
    if (!valid_name(catalog, name, name_length, 0))
        return -1;
    return catalog == HPCLANG_CATALOG_PROGRAM
               ? hpclang_hp39_program_body_snapshot(name, name_length, body)
               : hpclang_hp39_note_body_snapshot(name, name_length, body);
}

int hpclang_catalog_put(enum hpclang_catalog catalog, const uint8_t *name, uint32_t name_length,
                        const struct hpclang_object *body, enum hpclang_object_store_put_mode mode)
{
    if (!valid_name(catalog, name, name_length, 1))
        return -1;
    if (body == NULL || body->kind != HPCLANG_OBJECT_STRING || body->native_prologue != 0x2a2cu ||
        (mode != HPCLANG_OBJECT_STORE_UPSERT && mode != HPCLANG_OBJECT_STORE_CREATE_ONLY &&
         mode != HPCLANG_OBJECT_STORE_REPLACE_ONLY)) {
        errno = EINVAL;
        return -1;
    }
    struct hpclang_object *previous = NULL;
    int found = hpclang_catalog_get(catalog, name, name_length, &previous) == 0;
    hpclang_object_release(previous);
    if (!found && errno != ENOENT)
        return -1;
    if (found && mode == HPCLANG_OBJECT_STORE_CREATE_ONLY) {
        errno = EEXIST;
        return -1;
    }
    if (!found && mode == HPCLANG_OBJECT_STORE_REPLACE_ONLY) {
        errno = ENOENT;
        return -1;
    }
    return hpclang_hp39_catalog_edit(catalog, name, name_length, body);
}

int hpclang_catalog_remove(enum hpclang_catalog catalog, const uint8_t *name, uint32_t name_length)
{
    if (!valid_name(catalog, name, name_length, 1))
        return -1;
    struct hpclang_object *previous = NULL;
    if (hpclang_catalog_get(catalog, name, name_length, &previous) != 0)
        return -1;
    hpclang_object_release(previous);
    return hpclang_hp39_catalog_edit(catalog, name, name_length, NULL);
}
