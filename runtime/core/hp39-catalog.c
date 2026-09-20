#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "hp39-catalog.h"
#include "object-snapshot.h"

enum {
    RPL_DOLIST = 0x02a74u,
    RPL_DOCSTR = 0x02a2cu,
    RPL_DOIDNT = 0x02e48u,
    RPL_DOLAM = 0x02e6du,
    RPL_SEMI = 0x0312bu
};

static int read_nibble(
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

static int read_word(
    const struct hpclang_object *object,
    uint32_t offset,
    uint32_t nibbles,
    uint32_t *value)
{
    if (value == NULL || nibbles == 0u || nibbles > 8u ||
        object == NULL || offset > object->nibble_count ||
        object->nibble_count - offset < nibbles)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = 0u;
        if (!read_nibble(object, offset + i, &nibble))
            return 0;
        out |= (uint32_t)nibble << (4u * i);
    }
    *value = out;
    return 1;
}

static int read_byte(
    const struct hpclang_object *object,
    uint32_t offset,
    uint8_t *value)
{
    uint32_t word = 0u;
    if (value == NULL || !read_word(object, offset, 2u, &word))
        return 0;
    *value = (uint8_t)word;
    return 1;
}

static int inline_name(
    const struct hpclang_object *catalog,
    uint32_t offset,
    uint32_t prologue,
    uint32_t *span,
    uint32_t *name_offset,
    size_t *name_length)
{
    if (catalog == NULL || span == NULL ||
        name_offset == NULL || name_length == NULL)
        return 0;

    if (prologue == RPL_DOIDNT || prologue == RPL_DOLAM) {
        uint32_t characters = 0u;
        if (!read_word(catalog, offset + 5u, 2u, &characters))
            return 0;
        if (characters > (UINT32_MAX - 7u) / 2u)
            return 0;
        uint32_t total = 7u + characters * 2u;
        if (offset > catalog->nibble_count ||
            catalog->nibble_count - offset < total)
            return 0;
        *span = total;
        *name_offset = offset + 7u;
        *name_length = (size_t)characters;
        return 1;
    }

    if (prologue == RPL_DOCSTR) {
        uint32_t length = 0u;
        if (!read_word(catalog, offset + 5u, 5u, &length) ||
            length < 5u)
            return 0;
        uint32_t payload_nibbles = length - 5u;
        if ((payload_nibbles & 1u) != 0u ||
            length > UINT32_MAX - 5u)
            return 0;
        uint32_t total = 5u + length;
        if (offset > catalog->nibble_count ||
            catalog->nibble_count - offset < total)
            return 0;
        *span = total;
        *name_offset = offset + 10u;
        *name_length = (size_t)(payload_nibbles / 2u);
        return 1;
    }

    return 0;
}

static int name_matches(
    const struct hpclang_object *catalog,
    uint32_t name_offset,
    size_t stored_length,
    const uint8_t *name,
    size_t name_length)
{
    if (stored_length != name_length)
        return 0;
    for (size_t i = 0u; i < name_length; ++i) {
        uint8_t byte = 0u;
        if (i > (UINT32_MAX - name_offset) / 2u ||
            !read_byte(
                catalog,
                name_offset + (uint32_t)(i * 2u),
                &byte))
            return -1;
        if (byte != name[i])
            return 0;
    }
    return 1;
}

static int scan_catalog(
    const struct hpclang_object *catalog,
    const uint8_t *name,
    size_t name_length,
    uint32_t *count,
    int *found)
{
    if (catalog == NULL ||
        catalog->kind != HPCLANG_OBJECT_LIST ||
        catalog->native_prologue != RPL_DOLIST ||
        (name == NULL && name_length != 0u)) {
        errno = EINVAL;
        return -1;
    }

    uint32_t entries = 0u;
    int matched = 0;
    uint32_t offset = 5u;

    for (;;) {
        uint32_t word = 0u;
        if (!read_word(catalog, offset, 5u, &word)) {
            errno = EILSEQ;
            return -1;
        }

        if (word == RPL_SEMI) {
            if (offset + 5u != catalog->nibble_count) {
                errno = EILSEQ;
                return -1;
            }
            if (count != NULL)
                *count = entries;
            if (found != NULL)
                *found = matched;
            return 0;
        }

        uint32_t span = 0u;
        uint32_t entry_name_offset = 0u;
        size_t entry_name_length = 0u;
        if (!inline_name(
                catalog,
                offset,
                word,
                &span,
                &entry_name_offset,
                &entry_name_length)) {
            /*
             * A catalog entry that is not an inline name would require live
             * Saturn state to interpret. Reject it rather than weakening the
             * owned-snapshot contract.
             */
            errno = ENOTSUP;
            return -1;
        }

        if (entries == UINT32_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        ++entries;

        if (!matched && name != NULL) {
            int same = name_matches(
                catalog,
                entry_name_offset,
                entry_name_length,
                name,
                name_length);
            if (same < 0) {
                errno = EILSEQ;
                return -1;
            }
            matched = same;
        }

        if (span == 0u || offset > UINT32_MAX - span) {
            errno = EILSEQ;
            return -1;
        }
        offset += span;
    }
}

int hpclang_hp39_catalog_name_count(
    const struct hpclang_object *catalog,
    uint32_t *count)
{
    if (count == NULL) {
        errno = EINVAL;
        return -1;
    }
    *count = 0u;
    return scan_catalog(catalog, NULL, 0u, count, NULL);
}

int hpclang_hp39_catalog_contains_name(
    const struct hpclang_object *catalog,
    const uint8_t *name,
    size_t name_length,
    int *found)
{
    if (found == NULL || (name == NULL && name_length != 0u)) {
        errno = EINVAL;
        return -1;
    }
    *found = 0;
    return scan_catalog(catalog, name, name_length, NULL, found);
}
