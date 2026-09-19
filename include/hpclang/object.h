#ifndef HPCLANG_OBJECT_H
#define HPCLANG_OBJECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Owned calculator-object snapshot.
 *
 * The representation is opaque. A snapshot is independent of later Stock
 * stack/store mutation and must be released with hpclang_object_release().
 * It is process-local and is not a portable serialization format.
 */
struct hpclang_object;

enum hpclang_object_kind {
    HPCLANG_OBJECT_UNKNOWN = 0,
    HPCLANG_OBJECT_INTEGER = 1,
    HPCLANG_OBJECT_REAL = 2,
    HPCLANG_OBJECT_COMPLEX = 3,
    HPCLANG_OBJECT_STRING = 4,
    HPCLANG_OBJECT_CHARACTER = 5,
    HPCLANG_OBJECT_IDENTIFIER = 6,
    HPCLANG_OBJECT_LIST = 7,
    HPCLANG_OBJECT_ARRAY = 8,
    HPCLANG_OBJECT_SYMBOLIC = 9,
    HPCLANG_OBJECT_PROGRAM = 10,
    HPCLANG_OBJECT_DIRECTORY = 11,
    HPCLANG_OBJECT_GRAPHIC = 12,
    HPCLANG_OBJECT_LIBRARY = 13,
    HPCLANG_OBJECT_APLET = 14,
    HPCLANG_OBJECT_NOTE = 15
};

enum hpclang_object_kind hpclang_object_kind(
    const struct hpclang_object *object);

void hpclang_object_release(struct hpclang_object *object);

/*
 * Observe a real calculator RPL stack.
 *
 * Levels are 1-based; level 1 is the top object. HP39-family calculators do
 * not gain a synthetic RPL stack and return -1 with errno=ENOSYS.
 */
int hpclang_stack_depth(uint32_t *depth);

int hpclang_stack_peek(
    uint32_t level,
    struct hpclang_object **object);

#ifdef __cplusplus
}
#endif

#endif
