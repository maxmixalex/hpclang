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

#define HPCLANG_HP39_FUNCTION_VARIABLE_COUNT 50u
#define HPCLANG_HP39_FUNCTION_VARIABLE_NO_FIELD UINT32_MAX

enum hpclang_hp39_function_variable_projection_kind {
    HPCLANG_HP39_FUNCTION_VARIABLE_UNRESOLVED = 0,
    HPCLANG_HP39_FUNCTION_VARIABLE_DIRECT_FIELD = 1,
    HPCLANG_HP39_FUNCTION_VARIABLE_BINT_UNCOERCE = 2,
    HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL = 3,
    HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL_INVERTED = 4,
    HPCLANG_HP39_FUNCTION_VARIABLE_REAL_ARRAY_LIST = 5
};

struct hpclang_hp39_function_variable_projection {
    const char *category;
    size_t category_length;
    const char *name;
    size_t name_length;
    enum hpclang_hp39_function_variable_projection_kind kind;
    uint32_t fixed_field_index;
    uint32_t mask;
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

/*
 * Runtime-private projection of Function's native ApletVars schema.
 *
 * projection_at/find describe all 50 published schema entries, including
 * entries whose value semantics are not yet closed. snapshot() succeeds only
 * for source-proven direct fields, BINT->REAL UNCOERCE projections, flag
 * projections and NumIndep's real-array->LIST projection. Unresolved entries
 * return ENOTSUP rather than guessing from similarly named storage fields.
 */
int hpclang_hp39_function_variable_projection_at(
    uint32_t index,
    struct hpclang_hp39_function_variable_projection *projection);

int hpclang_hp39_function_variable_projection_find(
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_hp39_function_variable_projection *projection);

int hpclang_hp39_function_variable_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object);

#ifdef __cplusplus
}
#endif

#endif
