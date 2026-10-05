#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/object.h>
#include "object-snapshot.h"

static void set_word(struct hpclang_object *object, uint32_t at, uint32_t value)
{
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint32_t offset = at + i, shift = 4u * (offset % 2u);
        object->data[offset / 2u] = (uint8_t)((object->data[offset / 2u] & ~(15u << shift)) |
                                              (((value >> (4u * i)) & 15u) << shift));
    }
}

static void test_complex(void)
{
    struct hpclang_object *value = NULL;
    assert(hpclang_object_create_complex_f64(1.25, -0.0, &value) == 0);
    assert(hpclang_object_kind(value) == HPCLANG_OBJECT_COMPLEX);
    assert(value->nibble_count == 37u);
    double r = 0.0, i = 1.0;
    assert(hpclang_object_complex_f64(value, &r, &i) == 0);
    assert(r == 1.25 && i == 0.0 && signbit(i));
    /* Malformed second component cannot partially update caller outputs. */
    value->data[18] = (uint8_t)((value->data[18] & 0xf0u) | 1u);
    r = 42.0;
    i = 43.0;
    assert(hpclang_object_complex_f64(value, &r, &i) == -1 && errno == EILSEQ);
    assert(r == 42.0 && i == 43.0);
    hpclang_object_release(value);
    value = (struct hpclang_object *)(uintptr_t)1u;
    assert(hpclang_object_create_complex_f64(2.0, INFINITY, &value) == -1 && errno == EDOM);
    assert(value == NULL);
    assert(hpclang_object_create_complex_f64(NAN, 2.0, &value) == -1 && errno == EDOM);
    assert(value == NULL);
}

static void test_arrays(void)
{
    struct hpclang_object *a = NULL, *b = NULL, *array = NULL, *changed = NULL, *item = NULL;
    assert(hpclang_object_create_real_f64(1.25, &a) == 0);
    assert(hpclang_object_create_real_f64(-2.5, &b) == 0);
    const struct hpclang_object *elements[] = {a, b, b, a};
    uint32_t dimensions[] = {2u, 2u};
    assert(hpclang_object_create_array(dimensions, 2u, elements, 4u, &array) == 0);
    hpclang_object_release(b);
    b = NULL;
    struct hpclang_object_array_info info;
    assert(hpclang_object_array_info(array, &info) == 0);
    assert(info.rank == 2u && info.count == 4u && info.dimensions[0] == 2u &&
           info.dimensions[1] == 2u && info.element_kind == HPCLANG_OBJECT_REAL);
    assert(hpclang_object_array_get(array, 1u, &item) == 0);
    double number = 0.0;
    assert(hpclang_object_real_f64(item, &number) == 0 && number == -2.5);
    hpclang_object_release(item);
    assert(hpclang_object_array_replace(array, 1u, a, &changed) == 0);
    hpclang_object_release(a);
    a = NULL;
    assert(hpclang_object_array_get(array, 1u, &item) == 0);
    assert(hpclang_object_real_f64(item, &number) == 0 && number == -2.5);
    hpclang_object_release(item);
    hpclang_object_release(array);
    assert(hpclang_object_array_get(changed, 1u, &item) == 0);
    assert(hpclang_object_real_f64(item, &number) == 0 && number == 1.25);
    hpclang_object_release(item);
    assert(hpclang_object_array_get(changed, 4u, &item) == -1 && errno == EINVAL && item == NULL);
    set_word(changed, 20u, 3u); /* Shape and actual payload no longer agree. */
    assert(hpclang_object_array_info(changed, &info) == -1 && errno == EILSEQ);
    hpclang_object_release(changed);

    assert(hpclang_object_create_complex_f64(3.5, -2.0, &a) == 0);
    /* Native exponent 400 exceeds double but must survive exact extraction. */
    a->data[2] = (uint8_t)(a->data[2] & 15u);
    a->data[3] = 0x40u;
    elements[0] = a;
    dimensions[0] = 1u;
    assert(hpclang_object_create_array(dimensions, 1u, elements, 1u, &array) == 0);
    assert(hpclang_object_array_info(array, &info) == 0);
    assert(info.rank == 1u && info.dimensions[1] == 0u &&
           info.element_kind == HPCLANG_OBJECT_COMPLEX);
    assert(hpclang_object_array_get(array, 0u, &item) == 0);
    assert(item->nibble_count == a->nibble_count && memcmp(item->data, a->data, 19u) == 0);
    double real = 4.0, imaginary = 5.0;
    assert(hpclang_object_complex_f64(item, &real, &imaginary) == -1 && errno == ERANGE);
    assert(real == 4.0 && imaginary == 5.0);
    hpclang_object_release(item);
    hpclang_object_release(array);
    assert(hpclang_object_create_real_f64(1.0, &b) == 0);
    elements[1] = b;
    dimensions[0] = 2u;
    assert(hpclang_object_create_array(dimensions, 1u, elements, 2u, &array) == -1 &&
           errno == EINVAL);
    assert(array == NULL);
    dimensions[0] = UINT32_MAX;
    assert(hpclang_object_create_array(dimensions, 1u, elements, UINT32_MAX, &array) == -1 &&
           errno == EOVERFLOW);
    dimensions[0] = 0xfffffu;
    assert(hpclang_object_create_array(dimensions, 1u, elements, 0xfffffu, &array) == -1 &&
           errno == EOVERFLOW);
    dimensions[0] = 0u;
    assert(hpclang_object_create_array(dimensions, 1u, elements, 0u, &array) == -1 &&
           errno == EINVAL);
    hpclang_object_release(a);
    hpclang_object_release(b);
}

static void test_lists(void)
{
    struct hpclang_object *a = NULL, *b = NULL, *list = NULL, *nested = NULL;
    struct hpclang_object *changed = NULL, *item = NULL, *empty = NULL;
    assert(hpclang_object_create_character(0x80u, &a) == 0);
    assert(hpclang_object_create_real_f64(7.5, &b) == 0);
    const struct hpclang_object *elements[] = {a, b};
    assert(hpclang_object_create_list(elements, 2u, &list) == 0);
    const struct hpclang_object *outer[] = {list, a};
    assert(hpclang_object_create_list(outer, 2u, &nested) == 0);
    hpclang_object_release(a);
    hpclang_object_release(b);
    hpclang_object_release(list);
    size_t count = 99u;
    assert(hpclang_object_list_size(nested, &count) == 0 && count == 2u);
    assert(hpclang_object_list_get(nested, 0u, &list) == 0);
    hpclang_object_release(nested);
    assert(hpclang_object_list_get(list, 1u, &item) == 0);
    double value = 0.0;
    assert(hpclang_object_real_f64(item, &value) == 0 && value == 7.5);
    const struct hpclang_object *insert[] = {item};
    assert(hpclang_object_list_splice(list, 0u, 2u, insert, 1u, &changed) == 0);
    hpclang_object_release(item);
    assert(hpclang_object_list_size(list, &count) == 0 && count == 2u);
    assert(hpclang_object_list_size(changed, &count) == 0 && count == 1u);
    assert(hpclang_object_list_splice(changed, 0u, 1u, NULL, 0u, &empty) == 0);
    assert(hpclang_object_list_size(empty, &count) == 0 && count == 0u);
    assert(hpclang_object_list_get(empty, 0u, &item) == -1 && errno == EINVAL && item == NULL);
    assert(hpclang_object_list_splice(list, SIZE_MAX, 1u, NULL, 0u, &item) == -1 &&
           errno == EINVAL && item == NULL);
    assert(hpclang_object_list_splice(list, 1u, SIZE_MAX, NULL, 0u, &item) == -1 &&
           errno == EINVAL && item == NULL);
    hpclang_object_release(empty);
    hpclang_object_release(changed);
    /* A broken terminator must not allow an early, apparently valid get. */
    set_word(list, list->nibble_count - 5u, 0x02933u);
    assert(hpclang_object_list_get(list, 0u, &item) == -1 && errno == EILSEQ && item == NULL);
    hpclang_object_release(list);
    assert(hpclang_object_create_list(NULL, 0u, &empty) == 0);
    assert(hpclang_object_list_size(empty, &count) == 0 && count == 0u);
    hpclang_object_release(empty);
}

static void test_named_and_binary(void)
{
    const uint8_t name[] = {'A', 0u, 0x80u};
    const uint8_t bits[] = {0xefu, 0xcdu, 0xabu, 0x09u};
    struct hpclang_object *id = NULL, *binary = NULL, *tag = NULL, *value = NULL;
    for (int local = 0; local != 2; ++local) {
        enum hpclang_identifier_scope scope = (enum hpclang_identifier_scope)local;
        assert(hpclang_object_create_identifier(name, sizeof(name), scope, &id) == 0);
        enum hpclang_identifier_scope observed;
        size_t length = 0u;
        assert(hpclang_object_identifier_info(id, &length, &observed) == 0);
        assert(length == sizeof(name) && observed == scope);
        uint8_t copied[sizeof(name)] = {0};
        assert(hpclang_object_identifier_copy(id, copied, sizeof(copied)) == 0);
        assert(memcmp(copied, name, sizeof(name)) == 0);
        assert(hpclang_object_identifier_copy(id, copied, sizeof(copied) - 1u) == -1 &&
               errno == ENOBUFS);
        hpclang_object_release(id);
    }
    assert(hpclang_object_create_identifier(name, 0u, HPCLANG_IDENTIFIER_GLOBAL, &id) == -1 &&
           errno == EINVAL && id == NULL);
    assert(hpclang_object_create_binary(bits, 28u, &binary) == 0);
    size_t width = 0u;
    assert(hpclang_object_binary_size(binary, &width) == 0 && width == 28u);
    assert(hpclang_object_create_tagged(name, sizeof(name), binary, &tag) == 0);
    hpclang_object_release(binary);
    assert(hpclang_object_tag_size(tag, &width) == 0 && width == sizeof(name));
    uint8_t label[sizeof(name)] = {0};
    assert(hpclang_object_tag_copy(tag, label, sizeof(label)) == 0);
    assert(memcmp(label, name, sizeof(name)) == 0);
    assert(hpclang_object_tag_value(tag, &value) == 0);
    hpclang_object_release(tag);
    uint8_t copied[sizeof(bits)] = {0};
    assert(hpclang_object_binary_copy(value, copied, sizeof(copied)) == 0);
    assert(memcmp(copied, bits, sizeof(bits)) == 0);
    assert(hpclang_object_binary_copy(value, copied, sizeof(copied) - 1u) == -1 &&
           errno == ENOBUFS);
    assert(hpclang_object_create_tagged(NULL, 0u, value, &tag) == 0);
    assert(hpclang_object_tag_size(tag, &width) == 0 && width == 0u);
    assert(hpclang_object_tag_copy(tag, NULL, 0u) == 0);
    hpclang_object_release(tag);
    hpclang_object_release(value);
    uint8_t invalid = 0xf0u;
    assert(hpclang_object_create_binary(&invalid, 4u, &binary) == -1 && errno == EINVAL &&
           binary == NULL);
    assert(hpclang_object_create_binary(bits, 27u, &binary) == -1 && errno == EINVAL);
    assert(hpclang_object_create_binary(bits, SIZE_MAX - 3u, &binary) == -1 && errno == EOVERFLOW);
}

static int read_native(void *context, uint32_t address, uint8_t *value)
{
    if (address >= 128u)
        return 0;
    *value = ((uint8_t *)context)[address];
    return 1;
}

static void test_symbolic_matrix(void)
{
    struct hpclang_object *id = NULL, *number = NULL, *matrix = NULL, *changed = NULL, *item = NULL;
    const uint8_t name[] = {'X'};
    assert(hpclang_object_create_identifier(name, 1u, HPCLANG_IDENTIFIER_GLOBAL, &id) == 0);
    assert(hpclang_object_create_real_f64(4.0, &number) == 0);
    const struct hpclang_object *elements[] = {id, number, number, id};
    assert(hpclang_object_create_matrix(2u, 2u, elements, 4u, &matrix) == 0);
    assert(hpclang_object_matrix_replace(matrix, 1u, 1u, number, &changed) == 0);
    hpclang_object_release(id);
    hpclang_object_release(number);
    uint32_t rows = 0u, cols = 0u;
    assert(hpclang_object_matrix_size(matrix, &rows, &cols) == 0 && rows == 2u && cols == 2u);
    assert(hpclang_object_matrix_get(matrix, 1u, 1u, &item) == 0);
    assert(hpclang_object_kind(item) == HPCLANG_OBJECT_IDENTIFIER);
    hpclang_object_release(item);
    hpclang_object_release(matrix);
    assert(hpclang_object_matrix_get(changed, 1u, 1u, &item) == 0);
    hpclang_object_release(changed);
    double value = 0.0;
    assert(hpclang_object_real_f64(item, &value) == 0 && value == 4.0);
    elements[0] = item;
    assert(hpclang_object_create_matrix(1u, 1u, elements, 1u, &matrix) == 0);
    hpclang_object_release(item);
    assert(hpclang_object_matrix_get(matrix, 0u, 1u, &item) == -1 && errno == EINVAL &&
           item == NULL);
    assert(hpclang_object_matrix_get(matrix, SIZE_MAX, 0u, &item) == -1 && errno == EINVAL);
    elements[0] = matrix;
    assert(hpclang_object_create_matrix(1u, 1u, elements, 1u, &changed) == -1 && errno == ENOTSUP);
    set_word(matrix, matrix->nibble_count - 10u, 0x02933u);
    assert(hpclang_object_matrix_get(matrix, 0u, 0u, &item) == -1 && errno == EILSEQ &&
           item == NULL);
    hpclang_object_release(matrix);
}

static void test_exact_decimal(void)
{
    const char wide[] = "-123456789012345678901234567890123456789012345678901234567890";
    struct hpclang_object *object = NULL;
    assert(hpclang_object_create_integer_decimal(wide, sizeof(wide) - 1u, &object) == 0);
    size_t length = 0u;
    assert(hpclang_object_integer_decimal_size(object, &length) == 0 &&
           length == sizeof(wide) - 1u);
    char copied[sizeof(wide)] = {0};
    assert(hpclang_object_integer_decimal_copy(object, copied, sizeof(copied)) == 0);
    assert(strcmp(copied, wide) == 0);
    int64_t small = 91;
    assert(hpclang_object_integer_i64(object, &small) == -1 && errno == ERANGE && small == 91);
    memset(copied, 'X', sizeof(copied));
    assert(hpclang_object_integer_decimal_copy(object, copied, 1u) == -1 && errno == ENOBUFS);
    assert(copied[0] == 'X');
    hpclang_object_release(object);
    assert(hpclang_object_create_integer_decimal("-000", 4u, &object) == 0);
    assert(hpclang_object_integer_decimal_size(object, &length) == 0 && length == 1u);
    assert(hpclang_object_integer_decimal_copy(object, copied, sizeof(copied)) == 0 &&
           copied[0] == '0');
    hpclang_object_release(object);
    assert(hpclang_object_create_integer_decimal("1.0", 3u, &object) == -1 && errno == EINVAL &&
           object == NULL);
    assert(hpclang_object_create_integer_decimal("+", 1u, &object) == -1 && errno == EINVAL);
    assert(hpclang_object_create_integer_decimal("1", SIZE_MAX, &object) == -1 &&
           errno == EOVERFLOW);

    const int32_t exponents[] = {-500, 499};
    for (size_t i = 0u; i < 2u; ++i) {
        struct hpclang_decimal_real input = {
            .significand = 1u, .exponent = exponents[i], .negative = 1};
        struct hpclang_decimal_real output = {0};
        assert(hpclang_object_create_real_decimal(&input, &object) == 0);
        assert(hpclang_object_real_decimal(object, &output) == 0);
        assert(output.significand == UINT64_C(100000000000) &&
               output.exponent == exponents[i] - 11 && output.negative == 1);
        double approximate = 17.0;
        assert(hpclang_object_real_f64(object, &approximate) == -1 && errno == ERANGE &&
               approximate == 17.0);
        hpclang_object_release(object);
    }
    struct hpclang_decimal_real input = {.significand = 1u, .exponent = 500};
    assert(hpclang_object_create_real_decimal(&input, &object) == -1 && errno == ERANGE &&
           object == NULL);
    input.exponent = INT32_MIN;
    assert(hpclang_object_create_real_decimal(&input, &object) == -1 && errno == ERANGE);
    input.exponent = 0;
    input.significand = UINT64_C(1000000000000);
    assert(hpclang_object_create_real_decimal(&input, &object) == -1 && errno == ERANGE);
}

static void native_word(uint8_t *memory, uint32_t at, uint32_t value)
{
    for (uint32_t i = 0u; i < 5u; ++i)
        memory[at + i] = (uint8_t)((value >> (4u * i)) & 15u);
}

static void test_list_native_ownership(void)
{
    uint8_t memory[128] = {0};
    native_word(memory, 0u, 0x02a74u);
    native_word(memory, 5u, 64u); /* Borrowed native string reference. */
    native_word(memory, 10u, 0x0312bu);
    native_word(memory, 64u, 0x02a2cu);
    native_word(memory, 69u, 9u);
    memory[74] = 1u;
    memory[75] = 4u;
    memory[76] = 2u;
    memory[77] = 4u;
    struct hpclang_rpl_reader reader = {.context = memory, .read_nibble = read_native};
    struct hpclang_object *list = NULL, *item = NULL;
    struct hpclang_object *schema = NULL, *tag = NULL;
    assert(hpclang_object_snapshot_copy_encoded(&reader, 0u, 15u, &schema) == 0);
    assert(schema->nibble_count == 15u); /* Schema references keep their identity. */
    hpclang_object_release(schema);
    native_word(memory, 80u, 0x02afcu);
    memory[85] = 0u;
    memory[86] = 0u;
    memcpy(memory + 87u, memory, 15u);
    assert(hpclang_object_snapshot_copy(&reader, 80u, 22u, &tag) == 0);
    assert(hpclang_object_snapshot_copy(&reader, 0u, 15u, &list) == 0);
    memset(memory, 0xff, sizeof(memory)); /* Native GC/removal cannot affect the copy. */
    struct hpclang_object *tagged_list = NULL, *tagged_item = NULL;
    assert(hpclang_object_tag_value(tag, &tagged_list) == 0);
    hpclang_object_release(tag);
    assert(hpclang_object_list_get(tagged_list, 0u, &tagged_item) == 0);
    hpclang_object_release(tagged_list);
    uint8_t tagged_bytes[2] = {0};
    assert(hpclang_object_string_copy(tagged_item, tagged_bytes, 2u) == 0);
    assert(tagged_bytes[0] == 'A' && tagged_bytes[1] == 'B');
    hpclang_object_release(tagged_item);
    assert(hpclang_object_list_get(list, 0u, &item) == 0);
    hpclang_object_release(list);
    uint8_t bytes[2] = {0};
    assert(hpclang_object_string_copy(item, bytes, sizeof(bytes)) == 0);
    assert(bytes[0] == 'A' && bytes[1] == 'B');
    hpclang_object_release(item);

    memset(memory, 0, sizeof(memory));
    native_word(memory, 32u, 0x02a74u);
    native_word(memory, 37u, 32u); /* Cyclic native list is rejected, never followed forever. */
    native_word(memory, 42u, 0x0312bu);
    assert(hpclang_object_snapshot_copy(&reader, 32u, 15u, &list) == -1 && errno == EILSEQ &&
           list == NULL);
}

int main(void)
{
    test_complex();
    test_arrays();
    test_lists();
    test_named_and_binary();
    test_list_native_ownership();
    test_symbolic_matrix();
    test_exact_decimal();
    return 0;
}
