#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hp39-transfer-directory.h"

static const uint8_t hp39_directory_header[] = {'H', 'P', '3', '9', 'A', 's', 'c', 'A', ' '};

static int parse_decimal_length(const uint8_t *data, size_t size, size_t *offset, size_t *value)
{
    if (data == NULL || offset == NULL || value == NULL || *offset >= size)
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;

    size_t cursor = *offset;
    size_t out = 0u;
    size_t digits = 0u;
    while (cursor < size && data[cursor] >= '0' && data[cursor] <= '9') {
        size_t digit = (size_t)(data[cursor] - '0');
        if (out > (SIZE_MAX - digit) / 10u)
            return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;
        out = out * 10u + digit;
        ++cursor;
        ++digits;
    }

    if (digits == 0u || cursor >= size || data[cursor] != ' ')
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;

    *offset = cursor + 1u;
    *value = out;
    return HPCLANG_HP39_TRANSFER_DIRECTORY_OK;
}

int hpclang_hp39_transfer_directory_cursor_open(
    const uint8_t *data, size_t size, struct hpclang_hp39_transfer_directory_cursor *cursor)
{
    if (cursor == NULL || (data == NULL && size != 0u))
        return HPCLANG_HP39_TRANSFER_DIRECTORY_INVALID;

    memset(cursor, 0, sizeof(*cursor));
    if (size < sizeof(hp39_directory_header) ||
        memcmp(data, hp39_directory_header, sizeof(hp39_directory_header)) != 0)
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;

    cursor->data = data;
    cursor->size = size;
    cursor->offset = sizeof(hp39_directory_header);
    return HPCLANG_HP39_TRANSFER_DIRECTORY_OK;
}

int hpclang_hp39_transfer_directory_cursor_next(
    struct hpclang_hp39_transfer_directory_cursor *cursor,
    struct hpclang_hp39_transfer_directory_entry_view *entry)
{
    if (cursor == NULL || entry == NULL || cursor->data == NULL)
        return HPCLANG_HP39_TRANSFER_DIRECTORY_INVALID;
    if (cursor->offset > cursor->size)
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;
    if (cursor->offset == cursor->size)
        return 0;

    size_t offset = cursor->offset;
    if (cursor->entries != 0u) {
        if (cursor->data[offset] != ' ')
            return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;
        ++offset;
        if (offset == cursor->size) {
            cursor->offset = offset;
            return 0;
        }
    }

    if (cursor->entries >= HPCLANG_HP39_TRANSFER_DIRECTORY_ENTRY_LIMIT)
        return HPCLANG_HP39_TRANSFER_DIRECTORY_LIMIT;

    uint8_t subtype = cursor->data[offset++];
    if (subtype < 'A' || subtype > 'K' || offset >= cursor->size || cursor->data[offset] != ' ')
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;
    ++offset;

    size_t filename_length = 0u;
    int result = parse_decimal_length(cursor->data, cursor->size, &offset, &filename_length);
    if (result != HPCLANG_HP39_TRANSFER_DIRECTORY_OK)
        return result;
    if (filename_length > cursor->size - offset)
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;
    const uint8_t *filename = cursor->data + offset;
    offset += filename_length;

    size_t name_length = 0u;
    result = parse_decimal_length(cursor->data, cursor->size, &offset, &name_length);
    if (result != HPCLANG_HP39_TRANSFER_DIRECTORY_OK)
        return result;
    if (name_length > cursor->size - offset)
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;
    const uint8_t *name = cursor->data + offset;
    offset += name_length;

    if (offset < cursor->size && cursor->data[offset] != ' ')
        return HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED;

    entry->subtype = subtype;
    entry->filename = filename;
    entry->filename_length = filename_length;
    entry->name = name;
    entry->name_length = name_length;
    cursor->offset = offset;
    ++cursor->entries;
    return 1;
}
