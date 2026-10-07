#ifndef HPCLANG_APLET_H
#define HPCLANG_APLET_H

#include <hpclang/object.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* HP39 Stock Aplet-owned data, under the supported HPGCC39 template protocol.
     * Get returns an owned snapshot. Set copies its input; caller ownership stays
     * unchanged. The template saves the returned value when the Aplet is exited.
     * Unsupported models, Direct execution or unrecognized callers return ENOSYS.
     * Set accepts at most 7000 native nibbles (3495 string bytes); larger objects
     * return E2BIG. Stock allocation failure returns ENOSPC. */
    int hpclang_aplet_data_get(struct hpclang_object **object);
    int hpclang_aplet_data_set(const struct hpclang_object *object);

    /* Restore the template's initial empty-list value, with the same save timing. */
    int hpclang_aplet_data_clear(void);

    /* Native HP39 Stock Aplet catalog. The list is an owned snapshot of the
     * native display
     * names. Names below contain 1..254 raw calculator bytes. */
    int hpclang_aplet_list(struct hpclang_object **names);

    /* Create an independent named copy through Stock's Aplet save operation.
     * Existing
     * destinations return EEXIST; a missing source returns ENOENT.
     * Stock keeps the selected
     * Aplet unchanged. A large copy may return ENOSPC
     * while the ARM program's memory is
     * reserved. This is not a compiler or an
     * arbitrary Aplet-type constructor. */
    int hpclang_aplet_clone(const uint8_t *source, size_t source_length, const uint8_t *destination,
                            size_t destination_length);

    /* Remove a saved Aplet through Stock. The active Aplet is protected (EBUSY),
     * as are
     * native default Aplets (EPERM). A missing name returns ENOENT. */
    int hpclang_aplet_remove(const uint8_t *name, size_t name_length);

#ifdef __cplusplus
}
#endif
#endif
