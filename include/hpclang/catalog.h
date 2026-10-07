#ifndef HPCLANG_CATALOG_H
#define HPCLANG_CATALOG_H

#include <hpclang/object-store.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Native text catalogs on HP39g+/39gs/40gs Stock. These are independent of
     * HOME directories and Aplet Variables. Other execution contexts use ENOSYS. */
    enum hpclang_catalog
    {
        HPCLANG_CATALOG_PROGRAM = 1,
        HPCLANG_CATALOG_NOTE = 2
    };

    /* An owned list of global identifiers, containing raw calculator names. */
    int hpclang_catalog_list(enum hpclang_catalog catalog, struct hpclang_object **names);

    /* Names contain 1..19 calculator bytes. Get returns an owned string. */
    int hpclang_catalog_get(enum hpclang_catalog catalog, const uint8_t *name, uint32_t name_length,
                            struct hpclang_object **body);

    /* Copy a string into the native catalog using Stock's own save operation,
     * including its program compilation cache. Syntax is checked by Stock when
     * the program is run. Source snapshots retain their original value.
     * Input must fit the native portal's 7000-nibble object limit (E2BIG).
     * The reserved Program Editline record cannot be mutated (EPERM). */
    int hpclang_catalog_put(enum hpclang_catalog catalog, const uint8_t *name, uint32_t name_length,
                            const struct hpclang_object *body,
                            enum hpclang_object_store_put_mode mode);

    /* Remove the source and associated native compiler/editor records. */
    int hpclang_catalog_remove(enum hpclang_catalog catalog, const uint8_t *name,
                               uint32_t name_length);

#ifdef __cplusplus
}
#endif
#endif
