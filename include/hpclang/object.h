#ifndef HPCLANG_OBJECT_H
#define HPCLANG_OBJECT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Owned calculator-object snapshot.
     *
     * The representation is opaque. A snapshot is independent of later Stock
     * stack/store mutation and must be released with hpclang_object_release().
     * It is process-local and is not a portable serialization format.
     */
    struct hpclang_object;

    enum hpclang_object_kind
    {
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
        HPCLANG_OBJECT_NOTE = 15,
        HPCLANG_OBJECT_TAGGED = 16,
        HPCLANG_OBJECT_BINARY = 17
    };

    enum hpclang_object_kind hpclang_object_kind(const struct hpclang_object *object);

    void hpclang_object_release(struct hpclang_object *object);

    int hpclang_object_create_character(uint8_t value, struct hpclang_object **object);

    /*
     * Construct an exact classic calculator ZINT covering the complete int64_t
     * range. The result is an owned snapshot and can be pushed through the normal
     * bounded stack-mutation service.
     */
    int hpclang_object_create_integer_i64(int64_t value, struct hpclang_object **object);

    /* Exact arbitrary-length decimal integers. Input is optional '+'/'-' then
     * at least one
     * ASCII digit. Output has no redundant leading zero or plus
     * sign. Size/copy exclude a
     * terminator; a short buffer returns ENOBUFS. */
    int hpclang_object_create_integer_decimal(const char *text, size_t length,
                                              struct hpclang_object **object);
    int hpclang_object_integer_decimal_size(const struct hpclang_object *object, size_t *length);
    int hpclang_object_integer_decimal_copy(const struct hpclang_object *object, char *text,
                                            size_t capacity);

    /* Exact classic REAL value = (-1)^negative * significand * 10^exponent.
     * The significand
     * has at most 12 decimal digits. Construction normalizes
     * without rounding and rejects an
     * unrepresentable exponent with ERANGE.
     * This covers the native range beyond C double.
     * The distinct EREAL and
     * LONGREAL formats remain unsupported. */
    struct hpclang_decimal_real
    {
        uint64_t significand;
        int32_t exponent;
        int negative;
    };
    int hpclang_object_create_real_decimal(const struct hpclang_decimal_real *value,
                                           struct hpclang_object **object);
    int hpclang_object_real_decimal(const struct hpclang_object *object,
                                    struct hpclang_decimal_real *value);

    /*
     * Construct a classic Stock DOREAL from a finite C double. The value is
     * rounded to the calculator format's 12 significant decimal digits; signed
     * zero is preserved. NaN and infinities are rejected with errno=EDOM.
     */
    int hpclang_object_create_real_f64(double value, struct hpclang_object **object);

    /* Classic complex values have two independent 12-digit REAL components. */
    int hpclang_object_create_complex_f64(double real, double imaginary,
                                          struct hpclang_object **object);
    int hpclang_object_complex_f64(const struct hpclang_object *object, double *real,
                                   double *imaginary);

    /*
     * Owned homogeneous numeric vectors (rank 1) and matrices (rank 2).
     * Dimensions
     * must be nonzero; count must equal their product. Elements
     * must all be classic REAL or
     * all classic COMPLEX snapshots. Construction,
     * extraction and replacement preserve
     * native decimal digits without a
     * conversion through C double. Indices are zero-based,
     * in row-major order.
     * Inputs remain owned by the caller; every output is an independent
     * snapshot.
     */
    struct hpclang_object_array_info
    {
        uint32_t rank;
        uint32_t dimensions[2]; /* Unused second dimension is zero. */
        size_t count;
        enum hpclang_object_kind element_kind;
    };
    int hpclang_object_create_array(const uint32_t *dimensions, size_t rank,
                                    const struct hpclang_object *const *elements, size_t count,
                                    struct hpclang_object **object);
    int hpclang_object_array_info(const struct hpclang_object *object,
                                  struct hpclang_object_array_info *info);
    int hpclang_object_array_get(const struct hpclang_object *object, size_t index,
                                 struct hpclang_object **element);
    int hpclang_object_array_replace(const struct hpclang_object *object, size_t index,
                                     const struct hpclang_object *element,
                                     struct hpclang_object **replacement);

    /* Owned heterogeneous lists. Indices are zero-based. All constructors and
     * edits copy
     * their inputs; extraction remains valid after releasing the
     * list. Splice removes a
     * range and inserts zero or more elements, returning
     * a new list. An empty list is
     * valid.
     */
    int hpclang_object_create_list(const struct hpclang_object *const *elements, size_t count,
                                   struct hpclang_object **object);
    int hpclang_object_list_size(const struct hpclang_object *object, size_t *count);
    int hpclang_object_list_get(const struct hpclang_object *object, size_t index,
                                struct hpclang_object **element);
    int hpclang_object_list_splice(const struct hpclang_object *object, size_t index,
                                   size_t remove_count,
                                   const struct hpclang_object *const *elements, size_t count,
                                   struct hpclang_object **replacement);

    /* Heterogeneous symbolic matrices (native DOMATRIX), distinct from packed
     * numeric
     * arrays. A nonempty rectangular two-dimensional shape is required.
     * Elements are
     * row-major; get/replace use zero-based row and column indices.
     * Construction is
     * model-independent; native exchange requires a Stock model
     * that implements this object
     * type. */
    int hpclang_object_create_matrix(uint32_t rows, uint32_t columns,
                                     const struct hpclang_object *const *elements, size_t count,
                                     struct hpclang_object **object);
    int hpclang_object_matrix_size(const struct hpclang_object *object, uint32_t *rows,
                                   uint32_t *columns);
    int hpclang_object_matrix_get(const struct hpclang_object *object, size_t row, size_t column,
                                  struct hpclang_object **element);
    int hpclang_object_matrix_replace(const struct hpclang_object *object, size_t row,
                                      size_t column, const struct hpclang_object *element,
                                      struct hpclang_object **replacement);

    /*
     * Construct a classic calculator DOCSTR from raw calculator bytes.
     * No UTF-8 decoding or transcoding is performed. Embedded zero bytes are
     * preserved because the length is explicit.
     */
    int hpclang_object_create_string(const uint8_t *bytes, size_t byte_count,
                                     struct hpclang_object **object);

    enum hpclang_identifier_scope
    {
        HPCLANG_IDENTIFIER_GLOBAL = 0,
        HPCLANG_IDENTIFIER_LOCAL = 1
    };
    /* Names and labels use raw calculator bytes, at most 255 bytes. Creating an
     * identifier
     * does not define or resolve a variable. Copy returns ENOBUFS
     * for a short buffer; size
     * excludes any terminator. */
    int hpclang_object_create_identifier(const uint8_t *name, size_t length,
                                         enum hpclang_identifier_scope scope,
                                         struct hpclang_object **object);
    int hpclang_object_identifier_info(const struct hpclang_object *object, size_t *length,
                                       enum hpclang_identifier_scope *scope);
    int hpclang_object_identifier_copy(const struct hpclang_object *object, uint8_t *name,
                                       size_t capacity);
    int hpclang_object_create_tagged(const uint8_t *label, size_t length,
                                     const struct hpclang_object *value,
                                     struct hpclang_object **object);
    int hpclang_object_tag_size(const struct hpclang_object *object, size_t *length);
    int hpclang_object_tag_copy(const struct hpclang_object *object, uint8_t *label,
                                size_t capacity);
    int hpclang_object_tag_value(const struct hpclang_object *object,
                                 struct hpclang_object **value);

    /* Native binary integers retain their width. Bytes are least significant
     * first;
     * bit_count must be a positive multiple of four. An unused high
     * half-byte must be zero.
     * A short copy buffer returns ENOBUFS.
     * No truncation or implicit sign conversion. */
    int hpclang_object_create_binary(const uint8_t *bytes, size_t bit_count,
                                     struct hpclang_object **object);
    int hpclang_object_binary_size(const struct hpclang_object *object, size_t *bit_count);
    int hpclang_object_binary_copy(const struct hpclang_object *object, uint8_t *bytes,
                                   size_t capacity);

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
    int hpclang_object_integer_i64(const struct hpclang_object *object, int64_t *value);

    /*
     * Convert a classic Stock DOREAL to the nearest representable C double.
     * This is inherently approximate. A nonzero value that would overflow to
     * infinity or underflow to zero returns -1 with errno=ERANGE; EREAL/LONGREAL
     * remain ENOTSUP until their separate formats are frozen.
     */
    int hpclang_object_real_f64(const struct hpclang_object *object, double *value);

    int hpclang_object_character_value(const struct hpclang_object *object, uint8_t *value);

    int hpclang_object_string_size(const struct hpclang_object *object, size_t *byte_count);

    int hpclang_object_string_copy(const struct hpclang_object *object, uint8_t *buffer,
                                   size_t capacity);

    /*
     * Observe a real calculator RPL stack.
     *
     * Levels are 1-based; level 1 is the top object. HP39-family calculators do
     * not gain a synthetic RPL stack and return -1 with errno=ENOSYS.
     */
    int hpclang_stack_depth(uint32_t *depth);

    int hpclang_stack_peek(uint32_t level, struct hpclang_object **object);

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

    int hpclang_stack_replace(uint32_t level, const struct hpclang_object *object);

    /*
     * Reorder existing native references without copying or allocating objects.
     * Both
     * require two levels. Swap exchanges levels 1 and 2; nip removes level
     * 2 while retaining
     * level 1. Expected errors leave the whole stack unchanged.
     * HP39 and Direct execution
     * return ENOSYS, as for other stack mutations.
     */
    int hpclang_stack_swap(void);
    int hpclang_stack_nip(void);

    /* Exchange genuine System RPL TRUE/FALSE constants on classic Stock.
     * Values are 0 or 1;
     * numeric zero/one and other objects are not coerced.
     * Peek/pop preserve the output on
     * failure; failed pop preserves the stack. */
    int hpclang_stack_push_boolean(int value);
    int hpclang_stack_peek_boolean(uint32_t level, int *value);
    int hpclang_stack_pop_boolean(int *value);

#ifdef __cplusplus
}
#endif

#endif
