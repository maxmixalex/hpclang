#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>

#include "object-snapshot.h"

enum {
    RPL_DOINT = 0x02614u,
    RPL_DOCSTR = 0x02a2cu,
    RPL_SPACE_NIBBLES = 0x00100000u
};

static void set_nibble(
    struct hpclang_object *object,
    uint32_t offset,
    uint8_t value)
{
    uint8_t *packed = &object->data[offset >> 1];
    if ((offset & 1u) == 0u)
        *packed = (uint8_t)((*packed & 0xf0u) | (value & 0x0fu));
    else
        *packed = (uint8_t)((*packed & 0x0fu) | ((value & 0x0fu) << 4));
}

static void set_nibbles(
    struct hpclang_object *object,
    uint32_t offset,
    uint32_t value,
    uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i)
        set_nibble(
            object,
            offset + i,
            (uint8_t)((value >> (i * 4u)) & 0x0fu));
}

int hpclang_object_create_integer_i64(
    int64_t value,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    uint64_t magnitude = value < 0
        ? UINT64_C(0) - (uint64_t)value
        : (uint64_t)value;
    uint64_t remaining = magnitude;
    uint32_t digits = 1u;
    while (remaining >= UINT64_C(10)) {
        remaining /= UINT64_C(10);
        ++digits;
    }

    /*
     * DOINT stores a 5-nibble self-relative length, least-significant decimal
     * BCD digit first, then one sign nibble (0 positive, 9 negative). The
     * length counts its own five nibbles plus the decimal digits and sign.
     */
    uint32_t length = 6u + digits;
    uint32_t nibble_count = 5u + length;
    size_t packed_bytes = ((size_t)nibble_count + 1u) / 2u;

    struct hpclang_object *snapshot =
        calloc(1u, sizeof(*snapshot) + packed_bytes);
    if (snapshot == NULL) {
        errno = ENOMEM;
        return -1;
    }

    snapshot->kind = HPCLANG_OBJECT_INTEGER;
    snapshot->nibble_count = nibble_count;
    snapshot->native_prologue = RPL_DOINT;
    set_nibbles(snapshot, 0u, RPL_DOINT, 5u);
    set_nibbles(snapshot, 5u, length, 5u);

    remaining = magnitude;
    for (uint32_t i = 0u; i < digits; ++i) {
        set_nibble(
            snapshot,
            10u + i,
            (uint8_t)(remaining % UINT64_C(10)));
        remaining /= UINT64_C(10);
    }
    set_nibble(snapshot, 10u + digits, value < 0 ? 9u : 0u);

    *object = snapshot;
    return 0;
}

int hpclang_object_create_string(
    const uint8_t *bytes,
    size_t byte_count,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    if (byte_count != 0u && bytes == NULL) {
        errno = EFAULT;
        return -1;
    }

    /*
     * DOCSTR stores a 5-nibble self-relative length followed by two nibbles
     * per calculator byte. Keep the complete native object inside the classic
     * 20-bit Saturn address space so every constructed snapshot can be pushed
     * through the same bounded mutation path as a Stock snapshot.
     */
    if (byte_count > (RPL_SPACE_NIBBLES - 10u) / 2u) {
        errno = EOVERFLOW;
        return -1;
    }

    uint32_t payload_nibbles = (uint32_t)(byte_count * 2u);
    uint32_t length = 5u + payload_nibbles;
    uint32_t nibble_count = 5u + length;
    size_t packed_bytes = (size_t)nibble_count / 2u;

    if (packed_bytes > SIZE_MAX - sizeof(struct hpclang_object)) {
        errno = ENOMEM;
        return -1;
    }

    struct hpclang_object *snapshot =
        calloc(1u, sizeof(*snapshot) + packed_bytes);
    if (snapshot == NULL) {
        errno = ENOMEM;
        return -1;
    }

    snapshot->kind = HPCLANG_OBJECT_STRING;
    snapshot->nibble_count = nibble_count;
    snapshot->native_prologue = RPL_DOCSTR;
    set_nibbles(snapshot, 0u, RPL_DOCSTR, 5u);
    set_nibbles(snapshot, 5u, length, 5u);

    if (byte_count != 0u)
        memcpy(snapshot->data + 5u, bytes, byte_count);

    *object = snapshot;
    return 0;
}