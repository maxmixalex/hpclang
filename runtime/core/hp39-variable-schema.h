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
 * Runtime-private resolver for one five-nibble reference cell found inside a
 * Stock ApletVars schema. Implementations must return an owned snapshot of the
 * referenced data object and must not execute it. expected_prologue is the
 * native object prologue required by the caller.
 */
typedef int (*hpclang_hp39_variable_reference_resolver)(
    void *context,
    uint32_t reference,
    uint32_t expected_prologue,
    struct hpclang_object **object);

/*
 * Search an owned ApletVars# schema snapshot.
 *
 * Inline schemas are accepted directly. Stock/HPGCC schemas may also contain
 * five-nibble supported-entry references to category lists and extended IDs;
 * find_resolved() materializes only those data references through the supplied
 * read-only resolver.
 *
 * HP39 extended variable IDs are DOIDNT objects whose length byte includes
 * three non-display bytes at the end. HPGCC39's sat_skipob() proves those
 * bytes are part of the object boundary. They encode the little-endian Saturn
 * address of a 15-nibble variable descriptor; preceding bytes are the visible
 * variable name.
 */
int hpclang_hp39_variable_schema_find(
    const struct hpclang_object *schema,
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_hp39_variable_schema_match *match);

int hpclang_hp39_variable_schema_find_resolved(
    const struct hpclang_object *schema,
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    hpclang_hp39_variable_reference_resolver resolver,
    void *resolver_context,
    struct hpclang_hp39_variable_schema_match *match);

/*
 * Read the native 15-nibble variable descriptor referenced by an extended ID.
 * The three five-nibble words are the source-proven recall/getter entry, type
 * predicate and store/setter entry. This routine only reads metadata; it does
 * not execute Stock code.
 */
int hpclang_hp39_variable_descriptor_read(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    struct hpclang_hp39_variable_descriptor *descriptor);

#ifdef __cplusplus
}
#endif

#endif
