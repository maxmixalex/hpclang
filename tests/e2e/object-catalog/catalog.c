#include <errno.h>
#include <string.h>

#include <hpclang/catalog.h>
#include <hpclang/aplet.h>

static int text_matches(const struct hpclang_object *object, const char *text)
{
    uint8_t bytes[32];
    size_t length = 0u;
    return hpclang_object_string_size(object, &length) == 0 && length == strlen(text) &&
           hpclang_object_string_copy(object, bytes, sizeof(bytes)) == 0 &&
           memcmp(bytes, text, length) == 0;
}

static int catalog_has(const struct hpclang_object *names, const uint8_t *name, size_t length)
{
    size_t count = 0u;
    if (hpclang_object_list_size(names, &count) != 0)
        return -1;
    for (size_t i = 0u; i < count; ++i) {
        struct hpclang_object *id = NULL;
        enum hpclang_identifier_scope scope;
        size_t size = 0u;
        uint8_t bytes[255];
        if (hpclang_object_list_get(names, i, &id) != 0)
            return -1;
        int ok;
        if (hpclang_object_kind(id) == HPCLANG_OBJECT_STRING) {
            scope = HPCLANG_IDENTIFIER_GLOBAL;
            ok = hpclang_object_string_size(id, &size) == 0 &&
                 hpclang_object_string_copy(id, bytes, sizeof(bytes)) == 0;
        } else {
            ok = hpclang_object_identifier_info(id, &size, &scope) == 0 &&
                 hpclang_object_identifier_copy(id, bytes, sizeof(bytes)) == 0;
        }
        hpclang_object_release(id);
        if (!ok)
            return -1;
        if (scope == HPCLANG_IDENTIFIER_GLOBAL && size == length &&
            memcmp(bytes, name, length) == 0)
            return 1;
    }
    return 0;
}

static uint32_t aplet_lifecycle(void)
{
    static const uint8_t source[] = "Function", copy[] = "HPCATALOGCOPY";
    static const uint8_t current[] = "HPCLANG HPC1 TEST";
    struct hpclang_object *names = NULL, *old = NULL;
    uint32_t failure = 0x3c0u;
    if (hpclang_aplet_list(&names) != 0)
        goto done;
    int second_instance = catalog_has(names, (const uint8_t *)"2", 1u);
    if (second_instance < 0)
        goto done;
    /* Before the harness saves a second instance, the known fixture is active. */
    if (second_instance == 0 &&
        (hpclang_aplet_remove(current, sizeof(current) - 1u) != -1 || errno != EBUSY))
        goto done;
    hpclang_object_release(names);
    names = NULL;
    failure = 0x3c1u;
    if (hpclang_aplet_clone(source, sizeof(source) - 1u, source, sizeof(source) - 1u) != -1 ||
        errno != EEXIST || hpclang_aplet_remove(copy, sizeof(copy) - 1u) != -1 || errno != ENOENT)
        goto done;
    failure = 0x3c2u;
    if (hpclang_aplet_remove(source, sizeof(source) - 1u) != -1 || errno != EPERM)
        goto done;
    failure = 0x3c3u;
    if (hpclang_aplet_clone(source, sizeof(source) - 1u, copy, sizeof(copy) - 1u) != 0 ||
        hpclang_aplet_list(&old) != 0 || catalog_has(old, copy, sizeof(copy) - 1u) != 1)
        goto done;
    failure = 0x3c4u;
    if (hpclang_aplet_remove(copy, sizeof(copy) - 1u) != 0 || hpclang_aplet_list(&names) != 0 ||
        catalog_has(names, copy, sizeof(copy) - 1u) != 0 ||
        catalog_has(old, copy, sizeof(copy) - 1u) != 1)
        goto done;
    failure = 0u;
done:
    hpclang_object_release(names);
    hpclang_object_release(old);
    return failure;
}

uint32_t hpclang_e2e_catalog(void)
{
    static const uint8_t name[] = "HPCATALOG";
    struct hpclang_object *first = NULL, *second = NULL, *old = NULL, *fresh = NULL;
    struct hpclang_object *names = NULL;
    uint32_t failure = 0x3b0u;
    if (hpclang_object_create_string((const uint8_t *)"1+1:", 4u, &first) != 0 ||
        hpclang_object_create_string((const uint8_t *)"2+2:", 4u, &second) != 0)
        goto done;
    for (enum hpclang_catalog catalog = HPCLANG_CATALOG_PROGRAM; catalog <= HPCLANG_CATALOG_NOTE;
         ++catalog) {
        failure = 0x3b1u;
        if (hpclang_catalog_put(catalog, name, sizeof(name) - 1u, first,
                                HPCLANG_OBJECT_STORE_CREATE_ONLY) != 0 ||
            hpclang_catalog_get(catalog, name, sizeof(name) - 1u, &old) != 0 ||
            !text_matches(old, "1+1:"))
            goto done;
        failure = 0x3b2u;
        if (hpclang_catalog_put(catalog, name, sizeof(name) - 1u, second,
                                HPCLANG_OBJECT_STORE_CREATE_ONLY) != -1 ||
            errno != EEXIST || hpclang_catalog_list(catalog, &names) != 0 ||
            catalog_has(names, name, sizeof(name) - 1u) != 1)
            goto done;
        failure = 0x3b3u;
        if (hpclang_catalog_put(catalog, name, sizeof(name) - 1u, second,
                                HPCLANG_OBJECT_STORE_REPLACE_ONLY) != 0 ||
            hpclang_catalog_get(catalog, name, sizeof(name) - 1u, &fresh) != 0 ||
            !text_matches(fresh, "2+2:") || !text_matches(old, "1+1:"))
            goto done;
        hpclang_object_release(fresh);
        fresh = NULL;
        failure = 0x3b4u;
        if (hpclang_catalog_remove(catalog, name, sizeof(name) - 1u) != 0 ||
            hpclang_catalog_get(catalog, name, sizeof(name) - 1u, &fresh) != -1 ||
            errno != ENOENT || fresh != NULL || !text_matches(old, "1+1:") ||
            catalog_has(names, name, sizeof(name) - 1u) != 1)
            goto done;
        hpclang_object_release(names);
        names = NULL;
        failure = 0x3b5u;
        if (hpclang_catalog_list(catalog, &names) != 0 ||
            catalog_has(names, name, sizeof(name) - 1u) != 0 ||
            hpclang_catalog_put(catalog, name, sizeof(name) - 1u, first,
                                HPCLANG_OBJECT_STORE_REPLACE_ONLY) != -1 ||
            errno != ENOENT || hpclang_catalog_remove(catalog, name, sizeof(name) - 1u) != -1 ||
            errno != ENOENT)
            goto done;
        hpclang_object_release(names);
        names = NULL;
        hpclang_object_release(old);
        old = NULL;
    }
    failure = aplet_lifecycle();
done:
    hpclang_object_release(first);
    hpclang_object_release(second);
    hpclang_object_release(old);
    hpclang_object_release(fresh);
    hpclang_object_release(names);
    return failure;
}
