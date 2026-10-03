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

#ifdef __cplusplus
}
#endif
#endif
