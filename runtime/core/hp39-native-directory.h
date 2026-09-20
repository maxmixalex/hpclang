#ifndef HPCLANG_RUNTIME_HP39_NATIVE_DIRECTORY_H
#define HPCLANG_RUNTIME_HP39_NATIVE_DIRECTORY_H

#include <stddef.h>
#include <stdint.h>

#include <hpclang/object.h>

#include "rpl-object-span.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read the HP39 Program/Note private directory format without entering the
 * Stock evaluator. context_pointer_address is the five-nibble RAM cell used by
 * PGMCXT@/NOTESCXT@. The returned catalog is an owned DOLIST of base DOIDNT
 * names, matching CatThisCxt's user-visible apostrophe-record projection.
 */
int hpclang_hp39_native_catalog_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t context_pointer_address,
    struct hpclang_object **object);

/*
 * Snapshot the DOCSTR stored by the apostrophe-prefixed directory record for
 * one Program/Note name. A missing name returns -1 with errno=ENOENT.
 */
int hpclang_hp39_native_body_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t context_pointer_address,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object);

#ifdef __cplusplus
}
#endif

#endif
