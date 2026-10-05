#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <hpclang/object.h>
#include "object-snapshot.h"

static int fail(int error)
{
    errno = error;
    return -1;
}

static void put(struct hpclang_object *object, uint32_t at, uint8_t digit)
{
    if ((at & 1u) == 0u)
        object->data[at / 2u] = (uint8_t)((object->data[at / 2u] & 0xf0u) | digit);
    else
        object->data[at / 2u] = (uint8_t)((object->data[at / 2u] & 15u) | (digit << 4));
}

static uint32_t word(const struct hpclang_object *object, uint32_t at)
{
    uint32_t result = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t digit = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, at + i, &digit))
            return UINT32_MAX;
        result |= (uint32_t)digit << (4u * i);
    }
    return result;
}

static struct hpclang_object *allocate(uint32_t prologue, uint32_t size)
{
    struct hpclang_object *object = calloc(1u, sizeof(*object) + (size + 1u) / 2u);
    if (object == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    object->kind = hpclang_object_kind_from_native_prologue(prologue);
    object->native_prologue = prologue;
    object->nibble_count = size;
    for (uint32_t i = 0u; i < 5u; ++i)
        put(object, i, (uint8_t)((prologue >> (4u * i)) & 15u));
    return object;
}

int hpclang_object_create_integer_decimal(const char *text, size_t length,
                                          struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    if (text == NULL)
        return fail(EFAULT);
    if (length == 0u)
        return fail(EINVAL);
    if (length > 0x100000u)
        return fail(EOVERFLOW);
    size_t start = (text[0] == '-' || text[0] == '+') ? 1u : 0u;
    if (start == length)
        return fail(EINVAL);
    for (size_t i = start; i < length; ++i)
        if (text[i] < '0' || text[i] > '9')
            return fail(EINVAL);
    while (length - start > 1u && text[start] == '0')
        ++start;
    size_t digits = length - start;
    if (digits > 0x100000u - 11u)
        return fail(EOVERFLOW);
    struct hpclang_object *object = allocate(0x02614u, 11u + (uint32_t)digits);
    if (object == NULL)
        return -1;
    uint32_t native_length = 6u + (uint32_t)digits;
    for (uint32_t i = 0u; i < 5u; ++i)
        put(object, 5u + i, (uint8_t)((native_length >> (4u * i)) & 15u));
    for (uint32_t i = 0u; i < digits; ++i)
        put(object, 10u + i, (uint8_t)(text[length - 1u - i] - '0'));
    put(object, 10u + (uint32_t)digits, text[0] == '-' && text[start] != '0' ? 9u : 0u);
    *out = object;
    return 0;
}

static int integer_shape(const struct hpclang_object *object, uint32_t *digits, int *negative)
{
    if (object == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_INTEGER)
        return fail(EINVAL);
    if (object->native_prologue != 0x02614u)
        return fail(ENOTSUP);
    if (object->nibble_count < 12u || object->nibble_count > 0x100000u ||
        word(object, 0u) != 0x02614u || word(object, 5u) != object->nibble_count - 5u)
        return fail(EILSEQ);
    uint32_t count = object->nibble_count - 11u, significant = 1u;
    for (uint32_t i = 0u; i < count; ++i) {
        uint8_t digit = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, 10u + i, &digit) || digit > 9u)
            return fail(EILSEQ);
        if (digit != 0u)
            significant = i + 1u;
    }
    uint8_t sign = 0u, lowest = 0u;
    if (!hpclang_object_snapshot_read_nibble(object, 10u + count, &sign) ||
        (sign != 0u && sign != 9u) || !hpclang_object_snapshot_read_nibble(object, 10u, &lowest))
        return fail(EILSEQ);
    *digits = significant;
    *negative = sign == 9u && (significant != 1u || lowest != 0u);
    return 0;
}

int hpclang_object_integer_decimal_size(const struct hpclang_object *object, size_t *length)
{
    if (length == NULL)
        return fail(EFAULT);
    uint32_t digits = 0u;
    int negative = 0;
    if (integer_shape(object, &digits, &negative) != 0)
        return -1;
    *length = digits + (size_t)negative;
    return 0;
}

int hpclang_object_integer_decimal_copy(const struct hpclang_object *object, char *text,
                                        size_t capacity)
{
    uint32_t digits = 0u;
    int negative = 0;
    if (integer_shape(object, &digits, &negative) != 0)
        return -1;
    if (capacity < digits + (size_t)negative)
        return fail(ENOBUFS);
    if (text == NULL)
        return fail(EFAULT);
    if (negative)
        text[0] = '-';
    for (uint32_t i = 0u; i < digits; ++i) {
        uint8_t digit = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, 10u + digits - 1u - i, &digit))
            return fail(EILSEQ);
        text[(size_t)negative + i] = (char)('0' + digit);
    }
    return 0;
}

int hpclang_object_create_real_decimal(const struct hpclang_decimal_real *value,
                                       struct hpclang_object **out)
{
    if (out == NULL)
        return fail(EFAULT);
    *out = NULL;
    if (value == NULL)
        return fail(EFAULT);
    if (value->negative != 0 && value->negative != 1)
        return fail(EINVAL);
    if (value->significand > UINT64_C(999999999999))
        return fail(ERANGE);
    uint64_t significand = value->significand;
    int64_t exponent = (int64_t)value->exponent + 11;
    if (significand == 0u)
        exponent = 0;
    else
        while (significand < UINT64_C(100000000000)) {
            significand *= 10u;
            --exponent;
        }
    if (exponent < -500 || exponent > 499)
        return fail(ERANGE);
    struct hpclang_object *object = allocate(0x02933u, 21u);
    if (object == NULL)
        return -1;
    uint32_t encoded = (uint32_t)(exponent < 0 ? exponent + 1000 : exponent);
    for (uint32_t i = 0u; i < 3u; ++i) {
        put(object, 5u + i, (uint8_t)(encoded % 10u));
        encoded /= 10u;
    }
    for (uint32_t i = 0u; i < 12u; ++i) {
        put(object, 8u + i, (uint8_t)(significand % 10u));
        significand /= 10u;
    }
    put(object, 20u, value->negative ? 9u : 0u);
    *out = object;
    return 0;
}

int hpclang_object_real_decimal(const struct hpclang_object *object,
                                struct hpclang_decimal_real *value)
{
    if (object == NULL || value == NULL)
        return fail(EFAULT);
    if (object->kind != HPCLANG_OBJECT_REAL)
        return fail(EINVAL);
    if (object->native_prologue != 0x02933u)
        return fail(ENOTSUP);
    if (object->nibble_count != 21u || word(object, 0u) != 0x02933u)
        return fail(EILSEQ);
    uint8_t digits[16];
    for (uint32_t i = 0u; i < 16u; ++i)
        if (!hpclang_object_snapshot_read_nibble(object, 5u + i, &digits[i]) || digits[i] > 9u)
            return fail(EILSEQ);
    if (digits[15] != 0u && digits[15] != 9u)
        return fail(EILSEQ);
    int32_t exponent = digits[0] + 10 * digits[1] + 100 * digits[2];
    if (exponent >= 500)
        exponent -= 1000;
    struct hpclang_decimal_real result = {.exponent = exponent - 11, .negative = digits[15] == 9u};
    for (uint32_t i = 15u; i > 3u; --i)
        result.significand = result.significand * 10u + digits[i - 1u];
    *value = result;
    return 0;
}
