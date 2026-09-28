#include <assert.h>
#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/object.h>

#include "object-snapshot.h"

struct image
{
    uint8_t nibbles[128];
    uint32_t used;
};

static void put_nibbles(struct image *image, uint32_t at, uint32_t value, uint32_t count)
{
    assert(image != NULL);
    assert(at + count <= sizeof(image->nibbles));
    for (uint32_t i = 0u; i < count; ++i)
        image->nibbles[at + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
    if (image->used < at + count)
        image->used = at + count;
}

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct image *image = context;
    if (image == NULL || value == NULL || address >= image->used)
        return 0;
    *value = image->nibbles[address];
    return 1;
}

static struct hpclang_rpl_reader reader_for(struct image *image)
{
    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_nibble,
        .context = image,
    };
    return reader;
}

static struct hpclang_object *snapshot_from_image(struct image *image)
{
    struct hpclang_rpl_reader reader = reader_for(image);
    struct hpclang_object *object = NULL;
    assert(hpclang_object_snapshot_copy(&reader, 0u, image->used, &object) == 0);
    assert(object != NULL);
    return object;
}

static void put_zint(struct image *image, const char *magnitude, int negative)
{
    memset(image, 0, sizeof(*image));
    size_t digits = strlen(magnitude);
    assert(digits <= 100u);

    put_nibbles(image, 0u, 0x02614u, 5u);
    put_nibbles(image, 5u, (uint32_t)digits + 6u, 5u);
    for (size_t i = 0u; i < digits; ++i) {
        char ch = magnitude[digits - 1u - i];
        assert(ch >= '0' && ch <= '9');
        put_nibbles(image, 10u + (uint32_t)i, (uint32_t)(ch - '0'), 1u);
    }
    put_nibbles(image, 10u + (uint32_t)digits, negative ? 9u : 0u, 1u);
}

static void put_real(struct image *image, int exponent, uint64_t mantissa, int negative)
{
    assert(exponent >= -500 && exponent <= 499);
    assert(mantissa <= UINT64_C(999999999999));

    memset(image, 0, sizeof(*image));
    put_nibbles(image, 0u, 0x02933u, 5u);

    uint32_t encoded = exponent < 0 ? (uint32_t)(1000 + exponent) : (uint32_t)exponent;
    for (uint32_t i = 0u; i < 3u; ++i) {
        put_nibbles(image, 5u + i, encoded % 10u, 1u);
        encoded /= 10u;
    }

    uint64_t remaining = mantissa;
    for (uint32_t i = 0u; i < 12u; ++i) {
        put_nibbles(image, 8u + i, (uint32_t)(remaining % UINT64_C(10)), 1u);
        remaining /= UINT64_C(10);
    }
    put_nibbles(image, 20u, negative ? 9u : 0u, 1u);
    image->used = 21u;
}

static void put_string(struct image *image, const uint8_t *bytes, size_t count)
{
    memset(image, 0, sizeof(*image));
    assert(count <= 50u);

    put_nibbles(image, 0u, 0x02a2cu, 5u);
    put_nibbles(image, 5u, 5u + (uint32_t)(count * 2u), 5u);
    for (size_t i = 0u; i < count; ++i)
        put_nibbles(image, 10u + (uint32_t)(i * 2u), bytes[i], 2u);
}

static void test_real_snapshot(void)
{
    struct image image = {0};
    put_nibbles(&image, 0u, 0x02933u, 5u);
    for (uint32_t i = 5u; i < 21u; ++i)
        image.nibbles[i] = (uint8_t)(i & 0x0fu);
    image.used = 21u;

    struct hpclang_rpl_reader reader = reader_for(&image);
    struct hpclang_object *object = NULL;
    assert(hpclang_object_snapshot_copy(&reader, 0u, image.used, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    assert(object->nibble_count == 21u);
    assert(object->native_prologue == 0x02933u);

    for (uint32_t i = 0u; i < image.used; ++i) {
        uint8_t value = 0xffu;
        assert(hpclang_object_snapshot_read_nibble(object, i, &value));
        assert(value == image.nibbles[i]);
    }
    uint8_t value = 0u;
    assert(!hpclang_object_snapshot_read_nibble(object, image.used, &value));

    hpclang_object_release(object);
}

static void test_odd_character_snapshot(void)
{
    struct image image = {0};
    put_nibbles(&image, 0u, 0x029bfu, 5u);
    put_nibbles(&image, 5u, 0x41u, 2u);
    image.used = 7u;

    struct hpclang_rpl_reader reader = reader_for(&image);
    struct hpclang_object *object = NULL;
    assert(hpclang_object_snapshot_copy(&reader, 0u, image.used, &object) == 0);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_CHARACTER);
    assert(object->nibble_count == 7u);

    uint8_t value = 0u;
    assert(hpclang_object_snapshot_read_nibble(object, 6u, &value));
    assert(value == 4u);
    /* Unused high half of the final packed byte stays zero. */
    assert((object->data[3] & 0xf0u) == 0u);
    hpclang_object_release(object);
}

static void test_integer_conversions(void)
{
    struct image image = {0};
    int64_t value = 0;

    put_nibbles(&image, 0u, 0x02911u, 5u);
    put_nibbles(&image, 5u, 0x7ffffu, 5u);
    image.used = 10u;
    struct hpclang_object *object = snapshot_from_image(&image);
    assert(hpclang_object_integer_i64(object, &value) == 0);
    assert(value == 524287);
    hpclang_object_release(object);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02911u, 5u);
    put_nibbles(&image, 5u, 0xfffffu, 5u);
    image.used = 10u;
    object = snapshot_from_image(&image);
    assert(hpclang_object_integer_i64(object, &value) == 0);
    assert(value == -1);
    hpclang_object_release(object);

    put_zint(&image, "9223372036854775807", 0);
    object = snapshot_from_image(&image);
    assert(hpclang_object_integer_i64(object, &value) == 0);
    assert(value == INT64_MAX);
    hpclang_object_release(object);

    put_zint(&image, "9223372036854775808", 1);
    object = snapshot_from_image(&image);
    assert(hpclang_object_integer_i64(object, &value) == 0);
    assert(value == INT64_MIN);
    hpclang_object_release(object);

    put_zint(&image, "9223372036854775808", 0);
    object = snapshot_from_image(&image);
    errno = 0;
    assert(hpclang_object_integer_i64(object, &value) == -1);
    assert(errno == ERANGE);
    hpclang_object_release(object);

    put_zint(&image, "9223372036854775809", 1);
    object = snapshot_from_image(&image);
    errno = 0;
    assert(hpclang_object_integer_i64(object, &value) == -1);
    assert(errno == ERANGE);
    hpclang_object_release(object);

    put_zint(&image, "123", 0);
    object = snapshot_from_image(&image);
    object->data[5] = (uint8_t)((object->data[5] & 0xf0u) | 0x0au);
    errno = 0;
    assert(hpclang_object_integer_i64(object, &value) == -1);
    assert(errno == EILSEQ);
    hpclang_object_release(object);
}

static void test_real_conversion(void)
{
    struct image image = {0};
    double value = 0.0;

    put_real(&image, 0, UINT64_C(100000000000), 0);
    struct hpclang_object *object = snapshot_from_image(&image);
    assert(hpclang_object_real_f64(object, &value) == 0);
    assert(value > 0.999999999999 && value < 1.000000000001);
    hpclang_object_release(object);

    put_real(&image, 2, UINT64_C(123456000000), 1);
    object = snapshot_from_image(&image);
    assert(hpclang_object_real_f64(object, &value) == 0);
    assert(value < -123.455999 && value > -123.456001);
    hpclang_object_release(object);

    put_real(&image, 308, UINT64_C(100000000000), 0);
    object = snapshot_from_image(&image);
    assert(hpclang_object_real_f64(object, &value) == 0);
    assert(value > 9.99e307 && value < 1.01e308);
    hpclang_object_release(object);

    put_real(&image, 309, UINT64_C(100000000000), 0);
    object = snapshot_from_image(&image);
    value = 17.0;
    errno = 0;
    assert(hpclang_object_real_f64(object, &value) == -1);
    assert(errno == ERANGE);
    assert(value == 17.0);
    hpclang_object_release(object);

    put_real(&image, -400, UINT64_C(100000000000), 0);
    object = snapshot_from_image(&image);
    value = 19.0;
    errno = 0;
    assert(hpclang_object_real_f64(object, &value) == -1);
    assert(errno == ERANGE);
    assert(value == 19.0);
    hpclang_object_release(object);

    put_real(&image, 499, 0u, 1);
    object = snapshot_from_image(&image);
    assert(hpclang_object_real_f64(object, &value) == 0);
    assert(value == 0.0);
    hpclang_object_release(object);

    put_real(&image, 0, UINT64_C(100000000000), 0);
    image.nibbles[5] = 0x0au;
    object = snapshot_from_image(&image);
    errno = 0;
    assert(hpclang_object_real_f64(object, &value) == -1);
    assert(errno == EILSEQ);
    hpclang_object_release(object);

    put_real(&image, 0, UINT64_C(100000000000), 0);
    image.nibbles[20] = 8u;
    object = snapshot_from_image(&image);
    errno = 0;
    assert(hpclang_object_real_f64(object, &value) == -1);
    assert(errno == EILSEQ);
    hpclang_object_release(object);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02955u, 5u); /* EREAL */
    image.used = 26u;
    object = snapshot_from_image(&image);
    errno = 0;
    assert(hpclang_object_real_f64(object, &value) == -1);
    assert(errno == ENOTSUP);
    hpclang_object_release(object);
}

static void test_raw_string_conversions(void)
{
    const uint8_t expected[] = {0x41u, 0x00u, 0xffu};
    struct image image = {0};
    put_string(&image, expected, sizeof(expected));

    struct hpclang_object *object = snapshot_from_image(&image);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_STRING);

    size_t size = 0u;
    assert(hpclang_object_string_size(object, &size) == 0);
    assert(size == sizeof(expected));

    uint8_t buffer[3] = {0x11u, 0x22u, 0x33u};
    errno = 0;
    assert(hpclang_object_string_copy(object, buffer, 2u) == -1);
    assert(errno == ENOBUFS);
    assert(buffer[0] == 0x11u && buffer[1] == 0x22u && buffer[2] == 0x33u);

    assert(hpclang_object_string_copy(object, buffer, sizeof(buffer)) == 0);
    assert(memcmp(buffer, expected, sizeof(expected)) == 0);
    hpclang_object_release(object);

    /* Odd payload nibble count cannot represent complete 8-bit bytes. */
    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02a2cu, 5u);
    put_nibbles(&image, 5u, 6u, 5u);
    put_nibbles(&image, 10u, 0x0au, 1u);
    image.used = 11u;
    object = snapshot_from_image(&image);
    errno = 0;
    assert(hpclang_object_string_size(object, &size) == -1);
    assert(errno == EILSEQ);
    hpclang_object_release(object);
}

static void test_errors(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    struct hpclang_object *object = (struct hpclang_object *)(uintptr_t)1u;

    put_nibbles(&image, 0u, 0x026d5u, 5u); /* Aplet: boundary unfrozen */
    image.used = 5u;
    errno = 0;
    assert(hpclang_object_snapshot_copy(&reader, 0u, image.used, &object) == -1);
    assert(object == NULL);
    assert(errno == ENOTSUP);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02a2cu, 5u);
    put_nibbles(&image, 5u, 4u, 5u); /* invalid self-relative length */
    image.used = 10u;
    errno = 0;
    assert(hpclang_object_snapshot_copy(&reader, 0u, image.used, &object) == -1);
    assert(object == NULL);
    assert(errno == EILSEQ);
}

static void test_character_constructor(void)
{
    struct hpclang_object *object = NULL;
    assert(hpclang_object_create_character(0xa5u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_CHARACTER);
    assert(object->nibble_count == 7u);
    assert(object->native_prologue == 0x029bfu);

    uint8_t value = 0u;
    assert(hpclang_object_character_value(object, &value) == 0);
    assert(value == 0xa5u);

    hpclang_object_release(object);
}

int main(void)
{
    test_real_snapshot();
    test_odd_character_snapshot();
    test_character_constructor();
    test_integer_conversions();
    test_real_conversion();
    test_raw_string_conversions();
    test_errors();
    return 0;
}
