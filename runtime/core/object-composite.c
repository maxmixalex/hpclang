#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>
#include "object-snapshot.h"

enum
{
    REAL = 0x02933u,
    COMPLEX = 0x02977u,
    ARRAY = 0x029e8u,
    LIMIT = 0x100000u
};

static int fail(int error)
{
    errno = error;
    return -1;
}

static uint32_t word(const struct hpclang_object *object, uint32_t offset)
{
    uint32_t value = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint32_t at = offset + i;
        value |= (uint32_t)((object->data[at / 2u] >> (4u * (at % 2u))) & 15u) << (4u * i);
    }
    return value;
}

static void put_nibble(struct hpclang_object *object, uint32_t offset, uint8_t value)
{
    uint32_t shift = 4u * (offset % 2u);
    object->data[offset / 2u] =
        (uint8_t)((object->data[offset / 2u] & ~(15u << shift)) | ((uint32_t)value << shift));
}

static void put_word(struct hpclang_object *object, uint32_t offset, uint32_t value)
{
    for (uint32_t i = 0u; i < 5u; ++i)
        put_nibble(object, offset + i, (uint8_t)((value >> (4u * i)) & 15u));
}

static void copy_nibbles(struct hpclang_object *destination, uint32_t at,
                         const struct hpclang_object *source, uint32_t from, uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        uint32_t n = from + i;
        put_nibble(destination, at + i, (uint8_t)((source->data[n / 2u] >> (4u * (n % 2u))) & 15u));
    }
}

static struct hpclang_object *allocate(uint32_t prologue, uint32_t nibbles)
{
    struct hpclang_object *object = calloc(1u, sizeof(*object) + (nibbles + 1u) / 2u);
    if (object == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    object->kind = hpclang_object_kind_from_native_prologue(prologue);
    object->native_prologue = prologue;
    object->nibble_count = nibbles;
    put_word(object, 0u, prologue);
    return object;
}

/* Validate decimal encoding without converting, including values outside the
 * C double range. No precision is lost by array construction or replacement. */
static int numeric_body(const struct hpclang_object *object, uint32_t at, uint32_t width)
{
    for (uint32_t i = 0u; i < width; ++i) {
        uint8_t nibble = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, at + i, &nibble) || nibble > 9u ||
            (i % 16u == 15u && nibble != 0u && nibble != 9u))
            return fail(EILSEQ);
    }
    return 0;
}

static int scalar(const struct hpclang_object *object, uint32_t *width)
{
    if (object == NULL)
        return fail(EFAULT);
    if (object->native_prologue != REAL && object->native_prologue != COMPLEX)
        return fail(ENOTSUP);
    *width = object->native_prologue == REAL ? 16u : 32u;
    if (object->nibble_count != 5u + *width || word(object, 0u) != object->native_prologue ||
        object->kind != (uint32_t)hpclang_object_kind_from_native_prologue(object->native_prologue))
        return fail(EILSEQ);
    return numeric_body(object, 5u, *width);
}

int hpclang_object_create_complex_f64(double real, double imaginary, struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    struct hpclang_object *r = NULL, *i = NULL;
    if (hpclang_object_create_real_f64(real, &r) != 0)
        return -1;
    if (hpclang_object_create_real_f64(imaginary, &i) != 0) {
        hpclang_object_release(r);
        return -1;
    }
    struct hpclang_object *object = allocate(COMPLEX, 37u);
    if (object != NULL) {
        copy_nibbles(object, 5u, r, 5u, 16u);
        copy_nibbles(object, 21u, i, 5u, 16u);
    }
    hpclang_object_release(r);
    hpclang_object_release(i);
    *out = object;
    return object == NULL ? -1 : 0;
}

int hpclang_object_complex_f64(const struct hpclang_object *object, double *real, double *imaginary)
{
    if (object == NULL || real == NULL || imaginary == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_COMPLEX)
        return fail(EINVAL);
    uint32_t width = 0u;
    if (scalar(object, &width) != 0)
        return -1;
    struct hpclang_object *part = allocate(REAL, 21u);
    if (part == NULL)
        return -1;
    double r = 0.0, i = 0.0;
    copy_nibbles(part, 5u, object, 5u, 16u);
    int result = hpclang_object_real_f64(part, &r);
    if (result == 0) {
        copy_nibbles(part, 5u, object, 21u, 16u);
        result = hpclang_object_real_f64(part, &i);
    }
    hpclang_object_release(part);
    if (result == 0) {
        *real = r;
        *imaginary = i;
    }
    return result;
}

int hpclang_object_create_array(const uint32_t *dimensions, size_t rank,
                                const struct hpclang_object *const *elements, size_t count,
                                struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    if (dimensions == NULL || elements == NULL)
        return fail(EFAULT);
    if (rank == 0u || rank > 2u)
        return fail(EINVAL);
    uint32_t product = 1u;
    for (size_t i = 0u; i < rank; ++i) {
        if (dimensions[i] == 0u)
            return fail(EINVAL);
        if (dimensions[i] > (LIMIT - 1u) / product)
            return fail(EOVERFLOW);
        product *= dimensions[i];
    }
    if (count != product)
        return fail(EINVAL);
    uint32_t width = 0u;
    if (scalar(elements[0], &width) != 0)
        return -1;
    uint32_t header = 20u + 5u * (uint32_t)rank;
    if (count > (LIMIT - header) / width)
        return fail(EOVERFLOW);
    for (size_t i = 1u; i < count; ++i) {
        uint32_t element_width = 0u;
        if (scalar(elements[i], &element_width) != 0)
            return -1;
        if (element_width != width)
            return fail(EINVAL);
    }
    uint32_t nibbles = header + (uint32_t)count * width;
    struct hpclang_object *object = allocate(ARRAY, nibbles);
    if (object == NULL)
        return -1;
    put_word(object, 5u, nibbles - 5u);
    put_word(object, 10u, elements[0]->native_prologue);
    put_word(object, 15u, (uint32_t)rank);
    for (uint32_t i = 0u; i < rank; ++i)
        put_word(object, 20u + i * 5u, dimensions[i]);
    for (uint32_t i = 0u; i < count; ++i)
        copy_nibbles(object, header + i * width, elements[i], 5u, width);
    *out = object;
    return 0;
}

int hpclang_object_array_info(const struct hpclang_object *object,
                              struct hpclang_object_array_info *info)
{
    if (object == NULL || info == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_ARRAY)
        return fail(EINVAL);
    if (object->native_prologue != ARRAY)
        return fail(ENOTSUP);
    if (object->nibble_count < 25u || object->nibble_count > LIMIT || word(object, 0u) != ARRAY ||
        word(object, 5u) != object->nibble_count - 5u)
        return fail(EILSEQ);
    uint32_t rank = word(object, 15u), type = word(object, 10u);
    if (rank == 0u || rank > 2u)
        return fail(ENOTSUP);
    if (type != REAL && type != COMPLEX)
        return fail(ENOTSUP);
    uint32_t header = 20u + rank * 5u, width = type == REAL ? 16u : 32u;
    if (object->nibble_count < header)
        return fail(EILSEQ);
    struct hpclang_object_array_info result = {
        .rank = rank, .element_kind = hpclang_object_kind_from_native_prologue(type), .count = 1u};
    for (uint32_t i = 0u; i < rank; ++i) {
        result.dimensions[i] = word(object, 20u + 5u * i);
        if (result.dimensions[i] == 0u || result.dimensions[i] > LIMIT / result.count)
            return fail(EILSEQ);
        result.count *= result.dimensions[i];
    }
    if (result.count != (object->nibble_count - header) / width ||
        (object->nibble_count - header) % width != 0u)
        return fail(EILSEQ);
    if (numeric_body(object, header, object->nibble_count - header) != 0)
        return -1;
    *info = result;
    return 0;
}

int hpclang_object_array_get(const struct hpclang_object *object, size_t index,
                             struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    struct hpclang_object_array_info info;
    if (hpclang_object_array_info(object, &info) != 0)
        return -1;
    if (index >= info.count)
        return fail(EINVAL);
    uint32_t width = info.element_kind == HPCLANG_OBJECT_REAL ? 16u : 32u;
    struct hpclang_object *element = allocate(word(object, 10u), 5u + width);
    if (element == NULL)
        return -1;
    copy_nibbles(element, 5u, object, 20u + info.rank * 5u + (uint32_t)index * width, width);
    *out = element;
    return 0;
}

int hpclang_object_array_replace(const struct hpclang_object *object, size_t index,
                                 const struct hpclang_object *element, struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    struct hpclang_object_array_info info;
    if (hpclang_object_array_info(object, &info) != 0)
        return -1;
    if (index >= info.count)
        return fail(EINVAL);
    uint32_t width = 0u;
    if (scalar(element, &width) != 0)
        return -1;
    if (element->kind != (uint32_t)info.element_kind)
        return fail(EINVAL);
    struct hpclang_object *replacement = allocate(ARRAY, object->nibble_count);
    if (replacement == NULL)
        return -1;
    memcpy(replacement->data, object->data, (object->nibble_count + 1u) / 2u);
    copy_nibbles(replacement, 20u + info.rank * 5u + (uint32_t)index * width, element, 5u, width);
    *out = replacement;
    return 0;
}

static int snapshot_reader(void *context, uint32_t address, uint8_t *value)
{
    return hpclang_object_snapshot_read_nibble(context, address, value);
}

static int inline_span(const struct hpclang_object *object, uint32_t offset, uint32_t *span)
{
    if (offset > object->nibble_count)
        return fail(EILSEQ);
    struct hpclang_rpl_reader reader = {.context = (void *)object, .read_nibble = snapshot_reader};
    int result = hpclang_rpl_object_span(&reader, offset, object->nibble_count - offset, span);
    if (result != HPCLANG_RPL_SPAN_OK)
        return fail(result == HPCLANG_RPL_SPAN_UNSUPPORTED ? ENOTSUP : EILSEQ);
    return 0;
}

/* Locate a range while validating the complete list, including its exact
 * terminator. A raw native reference cannot be dereferenced from ARM-owned
 * bytes; it must have been resolved at the native snapshot boundary. */
static int list_range(const struct hpclang_object *object, size_t index, size_t remove_count,
                      size_t *count, uint32_t *begin, uint32_t *end)
{
    if (object == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_LIST || object->native_prologue != 0x02a74u)
        return fail(EINVAL);
    if (object->nibble_count < 10u || object->nibble_count > LIMIT || word(object, 0u) != 0x02a74u)
        return fail(EILSEQ);
    uint32_t offset = 5u;
    size_t size = 0u;
    for (;;) {
        if (object->nibble_count - offset < 5u)
            return fail(EILSEQ);
        if (size == index)
            *begin = offset;
        if (size >= index && size - index == remove_count)
            *end = offset;
        if (word(object, offset) == 0x0312bu) {
            if (offset + 5u != object->nibble_count)
                return fail(EILSEQ);
            break;
        }
        uint32_t span = 0u;
        if (inline_span(object, offset, &span) != 0)
            return -1;
        if (span == 0u || span > object->nibble_count - offset)
            return fail(EILSEQ);
        offset += span;
        ++size;
    }
    if (index > size || remove_count > size - index)
        return fail(EINVAL);
    *count = size;
    return 0;
}

int hpclang_object_list_size(const struct hpclang_object *object, size_t *count)
{
    if (count == NULL)
        return fail(EFAULT);
    size_t size = 0u;
    uint32_t begin = 0u, end = 0u;
    if (list_range(object, 0u, 0u, &size, &begin, &end) != 0)
        return -1;
    *count = size;
    return 0;
}

int hpclang_object_list_get(const struct hpclang_object *object, size_t index,
                            struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    size_t count = 0u;
    uint32_t begin = 0u, end = 0u;
    if (list_range(object, index, 1u, &count, &begin, &end) != 0)
        return -1;
    struct hpclang_object *element = allocate(word(object, begin), end - begin);
    if (element == NULL)
        return -1;
    copy_nibbles(element, 0u, object, begin, end - begin);
    *out = element;
    return 0;
}

static int list_elements_size(const struct hpclang_object *const *elements, size_t count,
                              uint32_t *size)
{
    if (count != 0u && elements == NULL)
        return fail(EFAULT);
    if (count > (LIMIT - 10u) / 5u)
        return fail(EOVERFLOW);
    uint32_t total = 0u;
    for (size_t i = 0u; i < count; ++i) {
        if (elements[i] == NULL)
            return fail(EFAULT);
        uint32_t span = 0u;
        if (inline_span(elements[i], 0u, &span) != 0)
            return -1;
        if (span != elements[i]->nibble_count || span < 5u ||
            word(elements[i], 0u) != elements[i]->native_prologue)
            return fail(EILSEQ);
        if (span > LIMIT - 10u - total)
            return fail(EOVERFLOW);
        total += span;
    }
    *size = total;
    return 0;
}

int hpclang_object_create_list(const struct hpclang_object *const *elements, size_t count,
                               struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    uint32_t size = 0u;
    if (list_elements_size(elements, count, &size) != 0)
        return -1;
    struct hpclang_object *list = allocate(0x02a74u, 10u + size);
    if (list == NULL)
        return -1;
    uint32_t offset = 5u;
    for (size_t i = 0u; i < count; ++i) {
        copy_nibbles(list, offset, elements[i], 0u, elements[i]->nibble_count);
        offset += elements[i]->nibble_count;
    }
    put_word(list, offset, 0x0312bu);
    uint32_t checked = 0u;
    if (inline_span(list, 0u, &checked) != 0 || checked != list->nibble_count) {
        hpclang_object_release(list);
        return fail(EILSEQ);
    }
    *out = list;
    return 0;
}

int hpclang_object_list_splice(const struct hpclang_object *object, size_t index,
                               size_t remove_count, const struct hpclang_object *const *elements,
                               size_t count, struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    size_t old_count = 0u;
    uint32_t begin = 0u, end = 0u, inserted = 0u;
    if (list_range(object, index, remove_count, &old_count, &begin, &end) != 0 ||
        list_elements_size(elements, count, &inserted) != 0)
        return -1;
    uint32_t retained = object->nibble_count - (end - begin);
    if (inserted > LIMIT - retained)
        return fail(EOVERFLOW);
    struct hpclang_object *list = allocate(0x02a74u, retained + inserted);
    if (list == NULL)
        return -1;
    copy_nibbles(list, 0u, object, 0u, begin);
    uint32_t offset = begin;
    for (size_t i = 0u; i < count; ++i) {
        copy_nibbles(list, offset, elements[i], 0u, elements[i]->nibble_count);
        offset += elements[i]->nibble_count;
    }
    copy_nibbles(list, offset, object, end, object->nibble_count - end);
    uint32_t checked = 0u;
    if (inline_span(list, 0u, &checked) != 0 || checked != list->nibble_count) {
        hpclang_object_release(list);
        return fail(EILSEQ);
    }
    *out = list;
    return 0;
}

static int native_name(const struct hpclang_object *object, uint32_t *length, uint32_t *end)
{
    if (object == NULL)
        return fail(EFAULT);
    if (object->nibble_count < 7u || object->nibble_count > LIMIT ||
        word(object, 0u) != object->native_prologue)
        return fail(EILSEQ);
    uint8_t lo = 0u, hi = 0u;
    if (!hpclang_object_snapshot_read_nibble(object, 5u, &lo) ||
        !hpclang_object_snapshot_read_nibble(object, 6u, &hi))
        return fail(EILSEQ);
    *length = lo | ((uint32_t)hi << 4);
    *end = 7u + 2u * *length;
    if (*end > object->nibble_count)
        return fail(EILSEQ);
    return 0;
}

static int matrix_shape(const struct hpclang_object *object, size_t wanted_row,
                        size_t wanted_column, uint32_t *rows, uint32_t *columns, uint32_t *begin,
                        uint32_t *size)
{
    if (object == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_ARRAY || object->native_prologue != 0x02686u)
        return fail(EINVAL);
    if (object->nibble_count < 20u || object->nibble_count > LIMIT ||
        word(object, 0u) != 0x02686u || word(object, object->nibble_count - 5u) != 0x0312bu)
        return fail(EILSEQ);
    uint32_t offset = 5u, row_count = 0u, column_count = 0u;
    while (offset < object->nibble_count - 5u) {
        if (object->nibble_count - offset < 15u || word(object, offset) != 0x02686u)
            return fail(EILSEQ);
        offset += 5u;
        uint32_t count = 0u;
        for (;;) {
            if (object->nibble_count - offset < 10u)
                return fail(EILSEQ);
            uint32_t type = word(object, offset);
            if (type == 0x0312bu) {
                offset += 5u;
                break;
            }
            if (type == 0x02686u)
                return fail(ENOTSUP);
            uint32_t span = 0u;
            if (inline_span(object, offset, &span) != 0)
                return -1;
            if (span == 0u || span > object->nibble_count - offset - 10u)
                return fail(EILSEQ);
            if (row_count == wanted_row && count == wanted_column) {
                *begin = offset;
                *size = span;
            }
            offset += span;
            ++count;
        }
        if (count == 0u || (row_count != 0u && count != column_count))
            return fail(EILSEQ);
        column_count = count;
        ++row_count;
    }
    if (offset != object->nibble_count - 5u || row_count == 0u)
        return fail(EILSEQ);
    *rows = row_count;
    *columns = column_count;
    return 0;
}

int hpclang_object_create_matrix(uint32_t rows, uint32_t columns,
                                 const struct hpclang_object *const *elements, size_t count,
                                 struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    if (rows == 0u || columns == 0u)
        return fail(EINVAL);
    if (rows > (LIMIT - 10u) / 10u || columns > LIMIT / rows)
        return fail(EOVERFLOW);
    if (count != (size_t)rows * columns)
        return fail(EINVAL);
    uint32_t payload = 0u;
    if (list_elements_size(elements, count, &payload) != 0)
        return -1;
    for (size_t i = 0u; i < count; ++i)
        if (elements[i]->native_prologue == 0x02686u)
            return fail(ENOTSUP);
    uint32_t overhead = 10u + rows * 10u;
    if (payload > LIMIT - overhead)
        return fail(EOVERFLOW);
    struct hpclang_object *matrix = allocate(0x02686u, overhead + payload);
    if (matrix == NULL)
        return -1;
    uint32_t offset = 5u;
    size_t index = 0u;
    for (uint32_t row = 0u; row < rows; ++row) {
        put_word(matrix, offset, 0x02686u);
        offset += 5u;
        for (uint32_t col = 0u; col < columns; ++col) {
            const struct hpclang_object *element = elements[index++];
            copy_nibbles(matrix, offset, element, 0u, element->nibble_count);
            offset += element->nibble_count;
        }
        put_word(matrix, offset, 0x0312bu);
        offset += 5u;
    }
    put_word(matrix, offset, 0x0312bu);
    uint32_t checked = 0u;
    if (inline_span(matrix, 0u, &checked) != 0 || checked != matrix->nibble_count) {
        hpclang_object_release(matrix);
        return fail(EILSEQ);
    }
    *out = matrix;
    return 0;
}

int hpclang_object_matrix_size(const struct hpclang_object *object, uint32_t *rows,
                               uint32_t *columns)
{
    if (rows == NULL || columns == NULL)
        return fail(EFAULT);
    uint32_t begin = 0u, size = 0u;
    return matrix_shape(object, SIZE_MAX, SIZE_MAX, rows, columns, &begin, &size);
}

int hpclang_object_matrix_get(const struct hpclang_object *object, size_t row, size_t column,
                              struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    uint32_t rows = 0u, columns = 0u, begin = 0u, size = 0u;
    if (matrix_shape(object, row, column, &rows, &columns, &begin, &size) != 0)
        return -1;
    if (row >= rows || column >= columns)
        return fail(EINVAL);
    struct hpclang_object *element = allocate(word(object, begin), size);
    if (element == NULL)
        return -1;
    copy_nibbles(element, 0u, object, begin, size);
    *out = element;
    return 0;
}

int hpclang_object_matrix_replace(const struct hpclang_object *object, size_t row, size_t column,
                                  const struct hpclang_object *element, struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    uint32_t rows = 0u, columns = 0u, begin = 0u, old_size = 0u, size = 0u;
    if (matrix_shape(object, row, column, &rows, &columns, &begin, &old_size) != 0)
        return -1;
    if (row >= rows || column >= columns)
        return fail(EINVAL);
    if (element == NULL)
        return fail(EFAULT);
    if (element->native_prologue == 0x02686u)
        return fail(ENOTSUP);
    if (inline_span(element, 0u, &size) != 0)
        return -1;
    if (size != element->nibble_count)
        return fail(EILSEQ);
    uint32_t retained = object->nibble_count - old_size;
    if (size > LIMIT - retained)
        return fail(EOVERFLOW);
    struct hpclang_object *matrix = allocate(0x02686u, retained + size);
    if (matrix == NULL)
        return -1;
    copy_nibbles(matrix, 0u, object, 0u, begin);
    copy_nibbles(matrix, begin, element, 0u, size);
    copy_nibbles(matrix, begin + size, object, begin + old_size,
                 object->nibble_count - begin - old_size);
    uint32_t checked = 0u;
    if (inline_span(matrix, 0u, &checked) != 0 || checked != matrix->nibble_count) {
        hpclang_object_release(matrix);
        return fail(EILSEQ);
    }
    *out = matrix;
    return 0;
}

static int copy_bytes(const struct hpclang_object *object, uint32_t at, size_t size, uint8_t *bytes,
                      size_t capacity)
{
    if (capacity < size)
        return fail(ENOBUFS);
    if (size != 0u && bytes == NULL)
        return fail(EFAULT);
    for (size_t i = 0u; i < size; ++i) {
        uint8_t lo = 0u, hi = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, at + (uint32_t)i * 2u, &lo) ||
            !hpclang_object_snapshot_read_nibble(object, at + (uint32_t)i * 2u + 1u, &hi))
            return fail(EILSEQ);
        bytes[i] = (uint8_t)(lo | (hi << 4));
    }
    return 0;
}

static void put_bytes(struct hpclang_object *object, uint32_t at, const uint8_t *bytes, size_t size)
{
    for (size_t i = 0u; i < size; ++i) {
        put_nibble(object, at + (uint32_t)i * 2u, bytes[i] & 15u);
        put_nibble(object, at + (uint32_t)i * 2u + 1u, bytes[i] >> 4);
    }
}

int hpclang_object_create_identifier(const uint8_t *name, size_t length,
                                     enum hpclang_identifier_scope scope,
                                     struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    if (length == 0u || length > 255u ||
        (scope != HPCLANG_IDENTIFIER_GLOBAL && scope != HPCLANG_IDENTIFIER_LOCAL))
        return fail(EINVAL);
    if (name == NULL)
        return fail(EFAULT);
    struct hpclang_object *object = allocate(
        scope == HPCLANG_IDENTIFIER_GLOBAL ? 0x02e48u : 0x02e6du, 7u + 2u * (uint32_t)length);
    if (object == NULL)
        return -1;
    put_nibble(object, 5u, (uint8_t)(length & 15u));
    put_nibble(object, 6u, (uint8_t)(length >> 4));
    put_bytes(object, 7u, name, length);
    *out = object;
    return 0;
}

int hpclang_object_identifier_info(const struct hpclang_object *object, size_t *length,
                                   enum hpclang_identifier_scope *scope)
{
    if (object == NULL || length == NULL || scope == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_IDENTIFIER ||
        (object->native_prologue != 0x02e48u && object->native_prologue != 0x02e6du))
        return fail(EINVAL);
    uint32_t size = 0u, end = 0u;
    if (native_name(object, &size, &end) != 0)
        return -1;
    if (end != object->nibble_count || size == 0u)
        return fail(EILSEQ);
    *length = size;
    *scope =
        object->native_prologue == 0x02e48u ? HPCLANG_IDENTIFIER_GLOBAL : HPCLANG_IDENTIFIER_LOCAL;
    return 0;
}

int hpclang_object_identifier_copy(const struct hpclang_object *object, uint8_t *name,
                                   size_t capacity)
{
    size_t size = 0u;
    enum hpclang_identifier_scope scope;
    if (hpclang_object_identifier_info(object, &size, &scope) != 0)
        return -1;
    return copy_bytes(object, 7u, size, name, capacity);
}

static int tag_info(const struct hpclang_object *object, uint32_t *length, uint32_t *begin)
{
    if (object == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_TAGGED || object->native_prologue != 0x02afcu)
        return fail(EINVAL);
    if (native_name(object, length, begin) != 0)
        return -1;
    uint32_t span = 0u;
    if (inline_span(object, *begin, &span) != 0)
        return -1;
    if (span != object->nibble_count - *begin)
        return fail(EILSEQ);
    return 0;
}

int hpclang_object_create_tagged(const uint8_t *label, size_t length,
                                 const struct hpclang_object *value, struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    if (length > 255u)
        return fail(EINVAL);
    if ((length != 0u && label == NULL) || value == NULL)
        return fail(EFAULT);
    uint32_t span = 0u;
    if (inline_span(value, 0u, &span) != 0)
        return -1;
    if (span != value->nibble_count || span < 5u)
        return fail(EILSEQ);
    uint32_t header = 7u + (uint32_t)length * 2u;
    if (span > LIMIT - header)
        return fail(EOVERFLOW);
    struct hpclang_object *object = allocate(0x02afcu, header + span);
    if (object == NULL)
        return -1;
    put_nibble(object, 5u, (uint8_t)(length & 15u));
    put_nibble(object, 6u, (uint8_t)(length >> 4));
    put_bytes(object, 7u, label, length);
    copy_nibbles(object, header, value, 0u, span);
    uint32_t checked = 0u;
    if (inline_span(object, 0u, &checked) != 0 || checked != object->nibble_count) {
        hpclang_object_release(object);
        return fail(EILSEQ);
    }
    *out = object;
    return 0;
}

int hpclang_object_tag_size(const struct hpclang_object *object, size_t *length)
{
    if (length == NULL)
        return fail(EFAULT);
    uint32_t size = 0u, begin = 0u;
    if (tag_info(object, &size, &begin) != 0)
        return -1;
    *length = size;
    return 0;
}

int hpclang_object_tag_copy(const struct hpclang_object *object, uint8_t *label, size_t capacity)
{
    uint32_t size = 0u, begin = 0u;
    if (tag_info(object, &size, &begin) != 0)
        return -1;
    return copy_bytes(object, 7u, size, label, capacity);
}

int hpclang_object_tag_value(const struct hpclang_object *object, struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    uint32_t size = 0u, begin = 0u;
    if (tag_info(object, &size, &begin) != 0)
        return -1;
    struct hpclang_object *value = allocate(word(object, begin), object->nibble_count - begin);
    if (value == NULL)
        return -1;
    copy_nibbles(value, 0u, object, begin, value->nibble_count);
    *out = value;
    return 0;
}

int hpclang_object_create_binary(const uint8_t *bytes, size_t bit_count,
                                 struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    if (bit_count == 0u || bit_count % 4u != 0u)
        return fail(EINVAL);
    if (bit_count / 4u > LIMIT - 10u)
        return fail(EOVERFLOW);
    if (bytes == NULL)
        return fail(EFAULT);
    if (bit_count % 8u != 0u && (bytes[bit_count / 8u] & 0xf0u) != 0u)
        return fail(EINVAL);
    uint32_t nibbles = (uint32_t)(bit_count / 4u);
    struct hpclang_object *object = allocate(0x02a4eu, 10u + nibbles);
    if (object == NULL)
        return -1;
    put_word(object, 5u, 5u + nibbles);
    for (uint32_t i = 0u; i < nibbles; ++i)
        put_nibble(object, 10u + i, (uint8_t)((bytes[i / 2u] >> (4u * (i % 2u))) & 15u));
    *out = object;
    return 0;
}

int hpclang_object_binary_size(const struct hpclang_object *object, size_t *bit_count)
{
    if (object == NULL || bit_count == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_BINARY || object->native_prologue != 0x02a4eu)
        return fail(EINVAL);
    if (object->nibble_count <= 10u || object->nibble_count > LIMIT ||
        word(object, 0u) != 0x02a4eu || word(object, 5u) != object->nibble_count - 5u)
        return fail(EILSEQ);
    *bit_count = (object->nibble_count - 10u) * 4u;
    return 0;
}

int hpclang_object_binary_copy(const struct hpclang_object *object, uint8_t *bytes, size_t capacity)
{
    size_t bits = 0u;
    if (hpclang_object_binary_size(object, &bits) != 0)
        return -1;
    size_t size = (bits + 7u) / 8u;
    if (capacity < size)
        return fail(ENOBUFS);
    if (bytes == NULL)
        return fail(EFAULT);
    memcpy(bytes, object->data + 5u, size);
    if (bits % 8u != 0u)
        bytes[size - 1u] &= 15u;
    return 0;
}
