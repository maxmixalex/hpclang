#ifndef HPCLANG_RUNTIME_HP39_FUNCTION_FIELDS_H
#define HPCLANG_RUNTIME_HP39_FUNCTION_FIELDS_H

#include <stddef.h>
#include <stdint.h>

#include <hpclang/object.h>

#include "rpl-object-span.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT 38u

struct hpclang_hp39_function_field_view {
    const char *name;
    size_t name_length;
    uint32_t object_address;
    uint32_t prologue;
};

/*
 * Runtime-private reader for the fixed-length prefix of a native Function
 * Aplet DORRP. The schema is the HP39 SDK's Sharedvar.h + Funcdir.h layout.
 *
 * This is storage structure, not the final VAR-menu/ApletVars projection.
 * The helper first proves the six-ROMPTR Function tTYPE signature; callers do
 * not get Function semantics merely because a directory happens to contain a
 * compatible sequence of null-name objects.
 */
int hpclang_hp39_function_field_at(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    uint32_t index,
    struct hpclang_hp39_function_field_view *field);

int hpclang_hp39_function_field_find(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_hp39_function_field_view *field);

int hpclang_hp39_function_field_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    uint32_t index,
    struct hpclang_object **object);

#ifdef __cplusplus
}
#endif

#endif
