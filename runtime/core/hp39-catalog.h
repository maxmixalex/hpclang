#ifndef HPCLANG_RUNTIME_HP39_CATALOG_H
#define HPCLANG_RUNTIME_HP39_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#include <hpclang/object.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Runtime-private view over an owned HP39 catalog list.
     *
     * The catalog bridge normalizes Stock catalog output to DOLIST snapshots.
     * This parser deliberately accepts only inline identifier/string entries.
     * A five-nibble reference cell is rejected rather than dereferenced through
     * live Stock state, preserving owned-snapshot semantics.
     */
    int hpclang_hp39_catalog_name_count(const struct hpclang_object *catalog, uint32_t *count);

    int hpclang_hp39_catalog_contains_name(const struct hpclang_object *catalog,
                                           const uint8_t *name, size_t name_length, int *found);

#ifdef __cplusplus
}
#endif

#endif
