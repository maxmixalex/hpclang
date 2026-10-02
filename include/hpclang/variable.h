#ifndef HPCLANG_VARIABLE_H
#define HPCLANG_VARIABLE_H

#include <stddef.h>
#include <stdint.h>
#include <hpclang/object.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* HP39-family variables in the active native Aplet context, selected by exact
     * category and variable names (raw calculator bytes). These operations preserve
     * the current Aplet/topic. Unknown names return ENOENT; unsupported execution
     * environments return ENOSYS. Values are owned snapshots. */
    int hpclang_variable_get(const uint8_t *category, size_t category_length, const uint8_t *name,
                             size_t name_length, struct hpclang_object **object);

    /* Copy a value into an existing schema-defined variable through Stock's own
     * descriptor/type dispatcher. Input ownership stays with the caller. Native
     * variables are predefined; directory create/remove semantics do not apply.
     * Values exceeding 7000 Saturn nibbles (3495 string bytes) return E2BIG
     * before Stock is invoked. Native type rejection returns EINVAL; insufficient
     * Stock memory returns ENOSPC. Other native rejections also map to EINVAL. */
    int hpclang_variable_set(const uint8_t *category, size_t category_length, const uint8_t *name,
                             size_t name_length, const struct hpclang_object *object);

#ifdef __cplusplus
}
#endif
#endif
