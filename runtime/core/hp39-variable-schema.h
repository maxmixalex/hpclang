#ifndef HPCLANG_RUNTIME_HP39_VARIABLE_SCHEMA_H
#define HPCLANG_RUNTIME_HP39_VARIABLE_SCHEMA_H

#include <stddef.h>
#include <stdint.h>

#include <hpclang/object.h>

#include "rpl-object-span.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hpclang_hp39_variable_schema_match {
    uint32_t id_offset;
    uint32_t id_span;
    size_t visible_name_length;
    uint32_t descriptor_address;
};

struct hpclang_hp39_variable_descriptor {
    uint32_t address;
    uint32_t recall_word;
    uint32_t type_predicate_word;
    uint32_t store_word;
};

/*
 * Search an owned ApletVars# schema snapshot:
 *
 *   { "Category" { extended-id ... } ... }
 *
 * HP39 extended variable IDs are ordinary DOIDNT objects whose length byte
 * includes three non-display bytes at the end. HPGCC39's sat_skipob() proves
 * those bytes are part of the ID object boundary. Stock variable IDs use them
 * as a little-endian Saturn descriptor address; the visible name is the
 * preceding payload.
 */
int hpclang_hp39_variable_schema_find(
    const struct hpclang_object *schema,
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_hp39_variable_schema_match *match);

/*
 * Read the native 15-nibble variable descriptor referenced by an extended ID.
 * The three five-nibble words are the source-proven recall/getter entry, type
 * predicate and store/setter entry. This routine only snapshots metadata; it
 * does not execute any Stock code.
 */
int hpclang_hp39_variable_descriptor_read(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    struct hpclang_hp39_variable_descriptor *descriptor);

#ifdef __cplusplus
}
#endif

#endif
