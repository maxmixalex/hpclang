#ifndef HPCLANG_OBJECT_H
#define HPCLANG_OBJECT_H

#include <stddef.h>
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

int hpclang_object_create_character(
    uint8_t value,
    struct hpclang_object **object);

/*
 * Construct a classic calculator DOCSTR from raw calculator bytes.
 * No UTF-8 decoding or transcoding is performed. Embedded zero bytes are
 * preserved because the length is explicit.
 */
int hpclang_object_create_string(
    const uint8_t *bytes,
    size_t byte_count,
    struct hpclang_object **object);

/*
 * Convert native scalar/string snapshots without exposing native prologues.
 *
 * Integer conversion is exact. It accepts classic 20-bit BINT and arbitrary
 * precision decimal ZINT only when the value fits int64_t; otherwise ERANGE.
 *
 * HP48/49/50 character and string objects are calculator 8-bit codes/bytes.
 * They are not implicitly decoded as UTF-8. Use the byte APIs when lossless
 * Stock-object round-trip matters.
 */
int hpclang_object_integer_i64(
    const struct hpclang_object *object,
    int64_t *value);

/*
 * Convert a classic Stock DOREAL to the nearest representable C double.
 * This is inherently approximate. A nonzero value that would overflow to
 * infinity or underflow to zero returns -1 with errno=ERANGE; EREAL/LONGREAL
 * remain ENOTSUP until their separate formats are frozen.
 */
int hpclang_object_real_f64(
    const struct hpclang_object *object,
    double *value);

int hpclang_object_character_value(
    const struct hpclang_object *object,
    uint8_t *value);

int hpclang_object_string_size(
    const struct hpclang_object *object,
    size_t *byte_count);

int hpclang_object_string_copy(
    const struct hpclang_object *object,
    uint8_t *buffer,
    size_t capacity);

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

/*
 * Mutating stack services are available only when the Stock launcher can prove
 * that native hpclang pages are fenced away from the live RPL arena.
 *
 * hpclang_stack_pop() is failure-safe for ordinary errors: it creates the
 * owned snapshot first and drops level 1 only after snapshot creation succeeds.
 */
int hpclang_stack_drop(uint32_t count);

int hpclang_stack_pop(struct hpclang_object **object);

int hpclang_stack_push(const struct hpclang_object *object);

int hpclang_stack_replace(
    uint32_t level,
    const struct hpclang_object *object);

#ifdef __cplusplus
}
#endif

#endif
