#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rpl-object-store.h"

enum {
    RPL_DORRP = 0x02a96u,
    RPL_USEROB = 0x00080711u,
    RPL_CONTEXT = 0x0008071bu,
    RPL_SPACE_NIBBLES = 0x00100000u,
    RPL_DIRECTORY_ENTRY_LIMIT = 4096u,
    RPL_DIRECTORY_DEPTH_LIMIT = 32u
};

static int checked_add(uint32_t a, uint32_t b, uint32_t *out)
{
    if (out == NULL || a > UINT32_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int read_nibbles(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t count,
    uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL ||
        value == NULL || count == 0u || count > 8u ||
        address >= RPL_SPACE_NIBBLES ||
        count > RPL_SPACE_NIBBLES - address)
        return HPCLANG_RPL_STORE_UNAVAILABLE;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble))
            return HPCLANG_RPL_STORE_UNAVAILABLE;
        if (nibble > 0x0fu)
            return HPCLANG_RPL_STORE_MALFORMED;
        out |= (uint32_t)nibble << (i * 4u);
    }

    *value = out;
    return HPCLANG_RPL_STORE_OK;
}

static int read_name_byte(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint8_t *value)
{
    uint32_t packed = 0u;
    int result = read_nibbles(reader, address, 2u, &packed);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    *value = (uint8_t)packed;
    return HPCLANG_RPL_STORE_OK;
}

int hpclang_rpl_store_home_object(
    const struct hpclang_rpl_reader *reader,
    uint32_t *object_address)
{
    if (object_address == NULL)
        return HPCLANG_RPL_STORE_UNAVAILABLE;
    *object_address = 0u;

    uint32_t object = 0u;
    int result = read_nibbles(reader, RPL_USEROB, 5u, &object);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (object >= RPL_SPACE_NIBBLES - 5u)
        return HPCLANG_RPL_STORE_MALFORMED;

    uint32_t prologue = 0u;
    result = read_nibbles(reader, object, 5u, &prologue);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (prologue != RPL_DORRP)
        return HPCLANG_RPL_STORE_NOT_DIRECTORY;

    *object_address = object;
    return HPCLANG_RPL_STORE_OK;
}

int hpclang_rpl_store_current_object(
    const struct hpclang_rpl_reader *reader,
    uint32_t *object_address)
{
    if (object_address == NULL)
        return HPCLANG_RPL_STORE_UNAVAILABLE;
    *object_address = 0u;

    uint32_t object = 0u;
    int result = read_nibbles(reader, RPL_CONTEXT, 5u, &object);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (object >= RPL_SPACE_NIBBLES - 5u)
        return HPCLANG_RPL_STORE_MALFORMED;

    uint32_t prologue = 0u;
    result = read_nibbles(reader, object, 5u, &prologue);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (prologue != RPL_DORRP)
        return HPCLANG_RPL_STORE_NOT_DIRECTORY;

    *object_address = object;
    return HPCLANG_RPL_STORE_OK;
}

static int directory_chain(
    const struct hpclang_rpl_reader *reader,
    uint32_t directory_object,
    uint32_t *chain)
{
    uint32_t prologue = 0u;
    int result = read_nibbles(reader, directory_object, 5u, &prologue);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (prologue != RPL_DORRP)
        return HPCLANG_RPL_STORE_NOT_DIRECTORY;

    uint32_t cursor = 0u;
    if (!checked_add(directory_object, 5u, &cursor) ||
        cursor >= RPL_SPACE_NIBBLES)
        return HPCLANG_RPL_STORE_MALFORMED;

    uint32_t home = 0u;
    result = hpclang_rpl_store_home_object(reader, &home);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    if (directory_object == home) {
        uint32_t libraries = 0u;
        result = read_nibbles(reader, cursor, 3u, &libraries);
        if (result != HPCLANG_RPL_STORE_OK)
            return result;
        if (libraries > (RPL_SPACE_NIBBLES - cursor) / 13u)
            return HPCLANG_RPL_STORE_MALFORMED;
        cursor += libraries * 13u;
    }

    if (!checked_add(cursor, 3u, &cursor) ||
        cursor > RPL_SPACE_NIBBLES - 5u)
        return HPCLANG_RPL_STORE_MALFORMED;

    *chain = cursor;
    return HPCLANG_RPL_STORE_OK;
}

int hpclang_rpl_directory_cursor_open(
    const struct hpclang_rpl_reader *reader,
    uint32_t directory_object,
    struct hpclang_rpl_directory_cursor *cursor)
{
    if (cursor == NULL)
        return HPCLANG_RPL_STORE_UNAVAILABLE;

    memset(cursor, 0, sizeof(*cursor));
    uint32_t chain = 0u;
    int result = directory_chain(reader, directory_object, &chain);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    uint32_t offset = 0u;
    result = read_nibbles(reader, chain, 5u, &offset);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    cursor->directory = directory_object;
    cursor->chain = chain;
    if (offset == 0u) {
        cursor->finished = 1u;
        return HPCLANG_RPL_STORE_OK;
    }

    uint32_t item = 0u;
    if (!checked_add(chain, offset, &item) ||
        item <= chain || item >= RPL_SPACE_NIBBLES)
        return HPCLANG_RPL_STORE_MALFORMED;

    cursor->item = item;
    return HPCLANG_RPL_STORE_OK;
}

int hpclang_rpl_directory_cursor_next(
    const struct hpclang_rpl_reader *reader,
    struct hpclang_rpl_directory_cursor *cursor,
    struct hpclang_rpl_directory_entry_view *entry)
{
    if (cursor == NULL || entry == NULL)
        return HPCLANG_RPL_STORE_UNAVAILABLE;
    if (cursor->finished)
        return 0;
    if (++cursor->steps > RPL_DIRECTORY_ENTRY_LIMIT)
        return HPCLANG_RPL_STORE_LIMIT;

    uint32_t item = cursor->item;
    uint32_t name_length = 0u;
    int result = read_nibbles(reader, item, 2u, &name_length);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (name_length >= HPCLANG_RPL_STORE_NAME_CAPACITY)
        return HPCLANG_RPL_STORE_MALFORMED;

    uint32_t name_nibbles = name_length * 2u;
    uint32_t object = 0u;
    if (!checked_add(item, 4u + name_nibbles, &object) ||
        object > RPL_SPACE_NIBBLES - 5u)
        return HPCLANG_RPL_STORE_MALFORMED;

    uint32_t trailing_length = 0u;
    result = read_nibbles(reader, object - 2u, 2u, &trailing_length);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (trailing_length != name_length)
        return HPCLANG_RPL_STORE_MALFORMED;

    memset(entry, 0, sizeof(*entry));
    entry->object_address = object;
    entry->name_length = name_length;
    for (uint32_t i = 0u; i < name_length; ++i) {
        result = read_name_byte(reader, item + 2u + i * 2u, &entry->name[i]);
        if (result != HPCLANG_RPL_STORE_OK)
            return result;
    }

    result = read_nibbles(reader, object, 5u, &entry->prologue);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    if (item < 5u)
        return HPCLANG_RPL_STORE_MALFORMED;
    uint32_t link = item - 5u;
    if (link < cursor->chain)
        return HPCLANG_RPL_STORE_MALFORMED;

    uint32_t offset = 0u;
    result = read_nibbles(reader, link, 5u, &offset);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;
    if (offset == 0u) {
        cursor->finished = 1u;
        return 1;
    }
    if (offset > link)
        return HPCLANG_RPL_STORE_MALFORMED;

    uint32_t next = link - offset;
    if (next <= cursor->chain || next >= item)
        return HPCLANG_RPL_STORE_MALFORMED;

    cursor->item = next;
    return 1;
}

int hpclang_rpl_directory_find(
    const struct hpclang_rpl_reader *reader,
    uint32_t directory_object,
    const uint8_t *name,
    uint32_t name_length,
    struct hpclang_rpl_directory_entry_view *entry)
{
    if ((name == NULL && name_length != 0u) ||
        name_length >= HPCLANG_RPL_STORE_NAME_CAPACITY)
        return HPCLANG_RPL_STORE_MALFORMED;

    struct hpclang_rpl_directory_cursor cursor;
    int result =
        hpclang_rpl_directory_cursor_open(reader, directory_object, &cursor);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    struct hpclang_rpl_directory_entry_view candidate;
    while ((result = hpclang_rpl_directory_cursor_next(
                reader, &cursor, &candidate)) > 0) {
        if (candidate.name_length == name_length &&
            (name_length == 0u ||
             memcmp(candidate.name, name, name_length) == 0)) {
            if (entry != NULL)
                *entry = candidate;
            return HPCLANG_RPL_STORE_OK;
        }
    }

    return result == 0 ? HPCLANG_RPL_STORE_NOT_FOUND : result;
}

struct path_search {
    const struct hpclang_rpl_reader *reader;
    uint32_t target;
    uint8_t *path;
    uint32_t capacity;
    uint32_t used;
    uint32_t components;
};

static int path_search_directory(
    struct path_search *search,
    uint32_t directory,
    uint32_t depth)
{
    if (depth > RPL_DIRECTORY_DEPTH_LIMIT)
        return HPCLANG_RPL_STORE_LIMIT;
    if (directory == search->target)
        return HPCLANG_RPL_STORE_OK;

    struct hpclang_rpl_directory_cursor cursor;
    int result =
        hpclang_rpl_directory_cursor_open(search->reader, directory, &cursor);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    struct hpclang_rpl_directory_entry_view entry;
    while ((result = hpclang_rpl_directory_cursor_next(
                search->reader, &cursor, &entry)) > 0) {
        if (entry.prologue != RPL_DORRP)
            continue;

        uint32_t old_used = search->used;
        uint32_t old_components = search->components;
        uint32_t needed = 1u + entry.name_length;
        if (needed > search->capacity - search->used)
            return HPCLANG_RPL_STORE_LIMIT;

        search->path[search->used++] = (uint8_t)entry.name_length;
        memcpy(search->path + search->used, entry.name, entry.name_length);
        search->used += entry.name_length;
        ++search->components;

        result = path_search_directory(
            search, entry.object_address, depth + 1u);
        if (result == HPCLANG_RPL_STORE_OK)
            return result;
        if (result != HPCLANG_RPL_STORE_NOT_FOUND)
            return result;

        search->used = old_used;
        search->components = old_components;
    }

    if (result < 0)
        return result;
    return HPCLANG_RPL_STORE_NOT_FOUND;
}

int hpclang_rpl_directory_path_to(
    const struct hpclang_rpl_reader *reader,
    uint32_t target_directory,
    uint8_t *encoded_path,
    uint32_t capacity,
    uint32_t *encoded_size,
    uint32_t *component_count)
{
    if (encoded_path == NULL || encoded_size == NULL ||
        component_count == NULL)
        return HPCLANG_RPL_STORE_UNAVAILABLE;

    *encoded_size = 0u;
    *component_count = 0u;

    uint32_t home = 0u;
    int result = hpclang_rpl_store_home_object(reader, &home);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    struct path_search search = {
        .reader = reader,
        .target = target_directory,
        .path = encoded_path,
        .capacity = capacity,
        .used = 0u,
        .components = 0u,
    };

    result = path_search_directory(&search, home, 0u);
    if (result != HPCLANG_RPL_STORE_OK)
        return result;

    *encoded_size = search.used;
    *component_count = search.components;
    return HPCLANG_RPL_STORE_OK;
}
