#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "object-snapshot.h"

enum {
    RPL_DOINT      = 0x02614u,
    RPL_DOLNGREAL  = 0x0263au,
    RPL_DOLNGCMP   = 0x02660u,
    RPL_DOMATRIX   = 0x02686u,
    RPL_DOAPLET    = 0x026d5u,
    RPL_DOBINT     = 0x02911u,
    RPL_DOREAL     = 0x02933u,
    RPL_DOEREL     = 0x02955u,
    RPL_DOCMP      = 0x02977u,
    RPL_DOECMP     = 0x0299du,
    RPL_DOCHAR     = 0x029bfu,
    RPL_DOARRY     = 0x029e8u,
    RPL_DOLNKARRY  = 0x02a0au,
    RPL_DOCSTR     = 0x02a2cu,
    RPL_DOLIST     = 0x02a74u,
    RPL_DORRP      = 0x02a96u,
    RPL_DOSYMB     = 0x02ab8u,
    RPL_DOEXT      = 0x02adau,
    RPL_DOGROB     = 0x02b1eu,
    RPL_DOLIB      = 0x02b40u,
    RPL_DOCOL      = 0x02d9du,
    RPL_DOCODE     = 0x02dccu,
    RPL_DOIDNT     = 0x02e48u,
    RPL_DOLAM      = 0x02e6du
};

static enum hpclang_object_kind classify(uint32_t prologue)
{
    switch (prologue) {
    case RPL_DOINT:
    case RPL_DOBINT:
        return HPCLANG_OBJECT_INTEGER;
    case RPL_DOREAL:
    case RPL_DOEREL:
    case RPL_DOLNGREAL:
        return HPCLANG_OBJECT_REAL;
    case RPL_DOCMP:
    case RPL_DOECMP:
    case RPL_DOLNGCMP:
        return HPCLANG_OBJECT_COMPLEX;
    case RPL_DOCSTR:
        return HPCLANG_OBJECT_STRING;
    case RPL_DOCHAR:
        return HPCLANG_OBJECT_CHARACTER;
    case RPL_DOIDNT:
    case RPL_DOLAM:
        return HPCLANG_OBJECT_IDENTIFIER;
    case RPL_DOLIST:
        return HPCLANG_OBJECT_LIST;
    case RPL_DOARRY:
    case RPL_DOLNKARRY:
    case RPL_DOMATRIX:
        return HPCLANG_OBJECT_ARRAY;
    case RPL_DOSYMB:
    case RPL_DOEXT:
        return HPCLANG_OBJECT_SYMBOLIC;
    case RPL_DOCOL:
    case RPL_DOCODE:
        return HPCLANG_OBJECT_PROGRAM;
    case RPL_DORRP:
        return HPCLANG_OBJECT_DIRECTORY;
    case RPL_DOGROB:
        return HPCLANG_OBJECT_GRAPHIC;
    case RPL_DOLIB:
        return HPCLANG_OBJECT_LIBRARY;
    case RPL_DOAPLET:
        return HPCLANG_OBJECT_APLET;
    default:
        return HPCLANG_OBJECT_UNKNOWN;
    }
}

static int read_prologue(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t *prologue)
{
    if (reader == NULL || reader->read_nibble == NULL || prologue == NULL)
        return 0;

    uint32_t value = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        if (address > UINT32_MAX - i)
            return 0;
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble) ||
            nibble > 0x0fu)
            return 0;
        value |= (uint32_t)nibble << (i * 4u);
    }

    *prologue = value;
    return 1;
}

static int span_errno(int result)
{
    switch (result) {
    case HPCLANG_RPL_SPAN_UNSUPPORTED:
        return ENOTSUP;
    case HPCLANG_RPL_SPAN_UNAVAILABLE:
        return EIO;
    case HPCLANG_RPL_SPAN_BOUNDS:
    case HPCLANG_RPL_SPAN_DEPTH:
    case HPCLANG_RPL_SPAN_MALFORMED:
    default:
        return EILSEQ;
    }
}

int hpclang_object_snapshot_copy(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    uint32_t span = 0u;
    int result = hpclang_rpl_object_span(reader, address, limit, &span);
    if (result != HPCLANG_RPL_SPAN_OK || span == 0u) {
        errno = span_errno(result);
        return -1;
    }

    uint32_t prologue = 0u;
    if (!read_prologue(reader, address, &prologue)) {
        errno = EIO;
        return -1;
    }

    size_t bytes = ((size_t)span + 1u) / 2u;
    if (bytes > SIZE_MAX - sizeof(struct hpclang_object)) {
        errno = ENOMEM;
        return -1;
    }

    struct hpclang_object *snapshot =
        malloc(sizeof(*snapshot) + bytes);
    if (snapshot == NULL) {
        errno = ENOMEM;
        return -1;
    }

    snapshot->kind = (uint32_t)classify(prologue);
    snapshot->nibble_count = span;
    snapshot->native_prologue = prologue;

    for (size_t i = 0u; i < bytes; ++i)
        snapshot->data[i] = 0u;

    for (uint32_t i = 0u; i < span; ++i) {
        if (address > UINT32_MAX - i) {
            free(snapshot);
            errno = EILSEQ;
            return -1;
        }

        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble)) {
            free(snapshot);
            errno = EIO;
            return -1;
        }
        if (nibble > 0x0fu) {
            free(snapshot);
            errno = EILSEQ;
            return -1;
        }

        if ((i & 1u) == 0u)
            snapshot->data[i >> 1] = nibble;
        else
            snapshot->data[i >> 1] |= (uint8_t)(nibble << 4);
    }

    *object = snapshot;
    return 0;
}

enum hpclang_object_kind hpclang_object_kind(
    const struct hpclang_object *object)
{
    if (object == NULL)
        return HPCLANG_OBJECT_UNKNOWN;
    return (enum hpclang_object_kind)object->kind;
}

void hpclang_object_release(struct hpclang_object *object)
{
    free(object);
}

int hpclang_object_create_character(
    uint8_t value,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    struct hpclang_object *snapshot =
        calloc(1u, sizeof(*snapshot) + 4u);
    if (snapshot == NULL) {
        errno = ENOMEM;
        return -1;
    }

    snapshot->kind = HPCLANG_OBJECT_CHARACTER;
    snapshot->nibble_count = 7u;
    snapshot->native_prologue = RPL_DOCHAR;

    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble =
            (uint8_t)((RPL_DOCHAR >> (i * 4u)) & 0x0fu);
        if ((i & 1u) == 0u)
            snapshot->data[i >> 1] = nibble;
        else
            snapshot->data[i >> 1] |= (uint8_t)(nibble << 4);
    }
    snapshot->data[2] |= (uint8_t)((value & 0x0fu) << 4);
    snapshot->data[3] = (uint8_t)(value >> 4);

    *object = snapshot;
    return 0;
}

static int object_read_nibbles(
    const struct hpclang_object *object,
    uint32_t offset,
    uint32_t count,
    uint32_t *value)
{
    if (object == NULL || value == NULL || count > 8u ||
        offset > object->nibble_count ||
        count > object->nibble_count - offset)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        uint8_t nibble = 0u;
        if (!hpclang_object_snapshot_read_nibble(
                object, offset + i, &nibble))
            return 0;
        out |= (uint32_t)nibble << (i * 4u);
    }

    *value = out;
    return 1;
}

int hpclang_object_integer_i64(
    const struct hpclang_object *object,
    int64_t *value)
{
    if (object == NULL || value == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (object->kind != HPCLANG_OBJECT_INTEGER) {
        errno = EINVAL;
        return -1;
    }

    if (object->native_prologue == RPL_DOBINT) {
        if (object->nibble_count != 10u) {
            errno = EILSEQ;
            return -1;
        }

        uint32_t raw = 0u;
        if (!object_read_nibbles(object, 5u, 5u, &raw)) {
            errno = EILSEQ;
            return -1;
        }

        int32_t signed_value =
            (raw & UINT32_C(0x00080000)) != 0u
                ? (int32_t)(raw | UINT32_C(0xfff00000))
                : (int32_t)raw;
        *value = (int64_t)signed_value;
        return 0;
    }

    if (object->native_prologue != RPL_DOINT) {
        errno = ENOTSUP;
        return -1;
    }

    uint32_t length = 0u;
    if (object->nibble_count < 11u ||
        !object_read_nibbles(object, 5u, 5u, &length) ||
        length < 6u ||
        length > UINT32_MAX - 5u ||
        object->nibble_count != 5u + length) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t digits = length - 6u;
    uint32_t sign_offset = 10u + digits;
    uint8_t sign = 0u;
    if (!hpclang_object_snapshot_read_nibble(
            object, sign_offset, &sign) ||
        (sign != 0u && sign != 9u)) {
        errno = EILSEQ;
        return -1;
    }

    const uint64_t positive_limit = (uint64_t)INT64_MAX;
    const uint64_t negative_limit = positive_limit + UINT64_C(1);
    uint64_t limit = sign == 9u ? negative_limit : positive_limit;
    uint64_t magnitude = 0u;

    for (uint32_t i = digits; i != 0u; --i) {
        uint8_t digit = 0u;
        if (!hpclang_object_snapshot_read_nibble(
                object, 10u + i - 1u, &digit) ||
            digit > 9u) {
            errno = EILSEQ;
            return -1;
        }

        if (magnitude > (limit - digit) / UINT64_C(10)) {
            errno = ERANGE;
            return -1;
        }
        magnitude = magnitude * UINT64_C(10) + digit;
    }

    if (sign == 9u) {
        if (magnitude == negative_limit)
            *value = INT64_MIN;
        else
            *value = -(int64_t)magnitude;
    } else {
        *value = (int64_t)magnitude;
    }
    return 0;
}

static int object_read_bcd(
    const struct hpclang_object *object,
    uint32_t offset,
    uint32_t digits,
    uint64_t *value)
{
    if (object == NULL || value == NULL ||
        offset > object->nibble_count ||
        digits > object->nibble_count - offset)
        return 0;

    uint64_t out = 0u;
    for (uint32_t i = digits; i != 0u; --i) {
        uint8_t digit = 0u;
        if (!hpclang_object_snapshot_read_nibble(
                object, offset + i - 1u, &digit) ||
            digit > 9u)
            return 0;

        if (out > (UINT64_MAX - digit) / UINT64_C(10))
            return 0;
        out = out * UINT64_C(10) + digit;
    }

    *value = out;
    return 1;
}

int hpclang_object_real_f64(
    const struct hpclang_object *object,
    double *value)
{
    if (object == NULL || value == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (object->kind != HPCLANG_OBJECT_REAL) {
        errno = EINVAL;
        return -1;
    }
    if (object->native_prologue != RPL_DOREAL) {
        errno = ENOTSUP;
        return -1;
    }
    if (object->nibble_count != 21u) {
        errno = EILSEQ;
        return -1;
    }

    uint64_t encoded_exponent = 0u;
    uint64_t mantissa = 0u;
    if (!object_read_bcd(object, 5u, 3u, &encoded_exponent) ||
        !object_read_bcd(object, 8u, 12u, &mantissa)) {
        errno = EILSEQ;
        return -1;
    }

    uint8_t sign = 0u;
    if (!hpclang_object_snapshot_read_nibble(object, 20u, &sign) ||
        (sign != 0u && sign != 9u)) {
        errno = EILSEQ;
        return -1;
    }

    int exponent = (int)encoded_exponent;
    if (exponent > 499)
        exponent -= 1000;

    if (mantissa == 0u) {
        double zero = 0.0;
        *value = sign == 9u ? -zero : zero;
        return 0;
    }

    double result = (double)mantissa;
    int scale = exponent - 11;

    while (scale > 0) {
        if (result > DBL_MAX / 10.0) {
            errno = ERANGE;
            return -1;
        }
        result *= 10.0;
        --scale;
    }

    while (scale < 0) {
        result /= 10.0;
        if (result == 0.0) {
            errno = ERANGE;
            return -1;
        }
        ++scale;
    }

    *value = sign == 9u ? -result : result;
    return 0;
}

static int object_string_length(
    const struct hpclang_object *object,
    size_t *byte_count)
{
    if (object == NULL || byte_count == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (object->kind != HPCLANG_OBJECT_STRING) {
        errno = EINVAL;
        return -1;
    }
    if (object->native_prologue != RPL_DOCSTR) {
        errno = ENOTSUP;
        return -1;
    }

    uint32_t length = 0u;
    if (object->nibble_count < 10u ||
        !object_read_nibbles(object, 5u, 5u, &length) ||
        length < 5u ||
        length > UINT32_MAX - 5u ||
        object->nibble_count != 5u + length) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t payload_nibbles = length - 5u;
    if ((payload_nibbles & 1u) != 0u) {
        errno = EILSEQ;
        return -1;
    }

    *byte_count = (size_t)(payload_nibbles >> 1);
    return 0;
}

int hpclang_object_string_size(
    const struct hpclang_object *object,
    size_t *byte_count)
{
    return object_string_length(object, byte_count);
}

int hpclang_object_string_copy(
    const struct hpclang_object *object,
    uint8_t *buffer,
    size_t capacity)
{
    size_t byte_count = 0u;
    if (object_string_length(object, &byte_count) != 0)
        return -1;
    if (byte_count != 0u && buffer == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (capacity < byte_count) {
        errno = ENOBUFS;
        return -1;
    }

    for (size_t i = 0u; i < byte_count; ++i) {
        uint32_t packed = 0u;
        uint32_t offset = 10u + (uint32_t)(i * 2u);
        if (!object_read_nibbles(object, offset, 2u, &packed)) {
            errno = EILSEQ;
            return -1;
        }
        buffer[i] = (uint8_t)packed;
    }
    return 0;
}

int hpclang_object_character_value(
    const struct hpclang_object *object,
    uint8_t *value)
{
    if (object == NULL || value == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (object->kind != HPCLANG_OBJECT_CHARACTER) {
        errno = EINVAL;
        return -1;
    }
    if (object->nibble_count != 7u ||
        object->native_prologue != RPL_DOCHAR) {
        errno = EILSEQ;
        return -1;
    }

    uint8_t low = 0u;
    uint8_t high = 0u;
    if (!hpclang_object_snapshot_read_nibble(object, 5u, &low) ||
        !hpclang_object_snapshot_read_nibble(object, 6u, &high)) {
        errno = EILSEQ;
        return -1;
    }

    *value = (uint8_t)(low | (uint8_t)(high << 4));
    return 0;
}

int hpclang_object_snapshot_read_nibble(
    const struct hpclang_object *object,
    uint32_t offset,
    uint8_t *value)
{
    if (object == NULL || value == NULL || offset >= object->nibble_count)
        return 0;

    uint8_t packed = object->data[offset >> 1];
    *value = (offset & 1u) != 0u
        ? (uint8_t)(packed >> 4)
        : (uint8_t)(packed & 0x0fu);
    return 1;
}
