#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <hpclang/object.h>

#include "hp39-native-directory.h"
#include "object-snapshot.h"
#include "rpl-object-store.h"

#define HP39_SATURN_LIMIT UINT32_C(0x00100000)
#define HP39_RPL_DORRP UINT32_C(0x00002a96)
#define HP39_RPL_DOCSTR UINT32_C(0x00002a2c)
#define HP39_RPL_DOLIST UINT32_C(0x00002a74)
#define HP39_RPL_DOIDNT UINT32_C(0x00002e48)
#define HP39_RPL_SEMI UINT32_C(0x0000312b)
#define HP39_CATALOG_PREFIX UINT8_C(0x27) /* apostrophe */

struct hp39_buffer_reader {
    const uint8_t *bytes;
    uint32_t nibble_count;
};

struct hp39_catalog_plan {
    uint32_t nibble_count;
};

struct hp39_catalog_writer {
    uint8_t *bytes;
    uint32_t capacity_nibbles;
    uint32_t offset;
};

struct hp39_body_search {
    const uint8_t *name;
    size_t name_length;
    struct hpclang_object **object;
};

typedef int (*hp39_directory_visitor)(
    const struct hpclang_rpl_reader *reader,
    uint32_t item_address,
    uint32_t name_length,
    uint32_t object_address,
    void *context);

static int hp39_read_word(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t nibbles,
    uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL || value == NULL ||
        nibbles == 0u || nibbles > 8u ||
        address > HP39_SATURN_LIMIT ||
        nibbles > HP39_SATURN_LIMIT - address)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble) ||
            nibble > 0x0fu)
            return 0;
        out |= (uint32_t)nibble << (i * 4u);
    }
    *value = out;
    return 1;
}

static int hp39_read_byte(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint8_t *value)
{
    uint32_t raw = 0u;
    if (value == NULL || !hp39_read_word(reader, address, 2u, &raw))
        return 0;
    *value = (uint8_t)raw;
    return 1;
}

static int hp39_directory_base(
    const struct hpclang_rpl_reader *reader,
    uint32_t context_pointer_address,
    uint32_t *directory_address)
{
    uint32_t root = 0u;
    uint32_t prologue = 0u;
    if (!hp39_read_word(reader, context_pointer_address, 5u, &root)) {
        errno = EIO;
        return -1;
    }
    if (root >= HP39_SATURN_LIMIT - 8u) {
        errno = EILSEQ;
        return -1;
    }
    if (!hp39_read_word(reader, root, 5u, &prologue)) {
        errno = EIO;
        return -1;
    }
    if (prologue != HP39_RPL_DORRP) {
        errno = EILSEQ;
        return -1;
    }

    *directory_address = root + 8u;
    return 0;
}

/*
 * Mirror HPGCC's sat_dirfindfirst/sat_dirfindnext/sat_itemgetname/sat_sknameup
 * traversal, but never expose raw Saturn addresses to callers. Items are
 * followed from newest to oldest, which is also CatThisCxt's catalog order.
 */
static int hp39_walk_directory(
    const struct hpclang_rpl_reader *reader,
    uint32_t context_pointer_address,
    hp39_directory_visitor visitor,
    void *context)
{
    if (reader == NULL || reader->read_nibble == NULL || visitor == NULL) {
        errno = EINVAL;
        return -1;
    }

    uint32_t directory = 0u;
    if (hp39_directory_base(
            reader, context_pointer_address, &directory) != 0)
        return -1;

    uint32_t offset = 0u;
    if (!hp39_read_word(reader, directory, 5u, &offset)) {
        errno = EIO;
        return -1;
    }
    if (offset == 0u)
        return 0;
    if (offset >= HP39_SATURN_LIMIT - directory) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t item = directory + offset;
    for (uint32_t steps = 0u; steps < HP39_SATURN_LIMIT / 5u; ++steps) {
        if (item <= directory || item < 5u ||
            item > HP39_SATURN_LIMIT - 2u) {
            errno = EILSEQ;
            return -1;
        }

        uint32_t name_length = 0u;
        if (!hp39_read_word(reader, item, 2u, &name_length)) {
            errno = EIO;
            return -1;
        }

        uint32_t name_and_zero = 2u + 2u * (name_length + 1u);
        if (name_and_zero > HP39_SATURN_LIMIT - item ||
            5u > HP39_SATURN_LIMIT - item - name_and_zero) {
            errno = EILSEQ;
            return -1;
        }
        uint32_t object_address = item + name_and_zero;

        int visit = visitor(
            reader, item, name_length, object_address, context);
        if (visit != 0)
            return visit;

        uint32_t link = item - 5u;
        uint32_t previous_offset = 0u;
        if (!hp39_read_word(reader, link, 5u, &previous_offset)) {
            errno = EIO;
            return -1;
        }
        if (previous_offset == 0u)
            return 0;
        if (previous_offset >= link) {
            errno = EILSEQ;
            return -1;
        }

        uint32_t previous = link - previous_offset;
        if (previous <= directory || previous >= item) {
            errno = EILSEQ;
            return -1;
        }
        item = previous;
    }

    errno = EILSEQ;
    return -1;
}

static int hp39_item_is_catalog_entry(
    const struct hpclang_rpl_reader *reader,
    uint32_t item_address,
    uint32_t name_length,
    int *selected)
{
    if (selected == NULL)
        return 0;
    *selected = 0;
    if (name_length == 0u)
        return 1;

    uint8_t prefix = 0u;
    if (!hp39_read_byte(reader, item_address + 2u, &prefix))
        return 0;
    *selected = prefix == HP39_CATALOG_PREFIX;
    return 1;
}

static int hp39_catalog_plan_visit(
    const struct hpclang_rpl_reader *reader,
    uint32_t item_address,
    uint32_t name_length,
    uint32_t object_address,
    void *context)
{
    (void)object_address;
    struct hp39_catalog_plan *plan = (struct hp39_catalog_plan *)context;
    int selected = 0;
    if (!hp39_item_is_catalog_entry(
            reader, item_address, name_length, &selected)) {
        errno = EIO;
        return -1;
    }
    if (!selected)
        return 0;

    uint32_t base_length = name_length - 1u;
    uint32_t entry_nibbles = 7u + 2u * base_length;
    if (entry_nibbles > HP39_SATURN_LIMIT - plan->nibble_count) {
        errno = EOVERFLOW;
        return -1;
    }
    plan->nibble_count += entry_nibbles;
    return 0;
}

static int hp39_buffer_put_nibble(
    struct hp39_catalog_writer *writer,
    uint8_t value)
{
    if (writer == NULL || value > 0x0fu ||
        writer->offset >= writer->capacity_nibbles)
        return 0;

    uint8_t *packed = &writer->bytes[writer->offset >> 1];
    if ((writer->offset & 1u) != 0u)
        *packed = (uint8_t)((*packed & 0x0fu) | (uint8_t)(value << 4));
    else
        *packed = (uint8_t)((*packed & 0xf0u) | value);
    ++writer->offset;
    return 1;
}

static int hp39_buffer_put_word(
    struct hp39_catalog_writer *writer,
    uint32_t value,
    uint32_t nibbles)
{
    for (uint32_t i = 0u; i < nibbles; ++i) {
        if (!hp39_buffer_put_nibble(
                writer, (uint8_t)((value >> (i * 4u)) & 0x0fu)))
            return 0;
    }
    return 1;
}

static int hp39_catalog_write_visit(
    const struct hpclang_rpl_reader *reader,
    uint32_t item_address,
    uint32_t name_length,
    uint32_t object_address,
    void *context)
{
    (void)object_address;
    struct hp39_catalog_writer *writer =
        (struct hp39_catalog_writer *)context;
    int selected = 0;
    if (!hp39_item_is_catalog_entry(
            reader, item_address, name_length, &selected)) {
        errno = EIO;
        return -1;
    }
    if (!selected)
        return 0;

    uint32_t base_length = name_length - 1u;
    if (!hp39_buffer_put_word(writer, HP39_RPL_DOIDNT, 5u) ||
        !hp39_buffer_put_word(writer, base_length, 2u)) {
        errno = EILSEQ;
        return -1;
    }

    for (uint32_t i = 0u; i < base_length; ++i) {
        uint8_t byte = 0u;
        if (!hp39_read_byte(
                reader, item_address + 4u + i * 2u, &byte)) {
            errno = EIO;
            return -1;
        }
        if (!hp39_buffer_put_word(writer, byte, 2u)) {
            errno = EILSEQ;
            return -1;
        }
    }
    return 0;
}

static int hp39_buffer_read_nibble(
    void *context,
    uint32_t nibble_address,
    uint8_t *value)
{
    struct hp39_buffer_reader *buffer =
        (struct hp39_buffer_reader *)context;
    if (buffer == NULL || value == NULL ||
        nibble_address >= buffer->nibble_count)
        return 0;

    uint8_t packed = buffer->bytes[nibble_address >> 1];
    *value = (nibble_address & 1u) != 0u
        ? (uint8_t)(packed >> 4)
        : (uint8_t)(packed & 0x0fu);
    return 1;
}

static int hp39_store_errno(int result)
{
    switch (result) {
    case HPCLANG_RPL_STORE_NOT_FOUND:
        errno = ENOENT;
        break;
    case HPCLANG_RPL_STORE_LIMIT:
        errno = EOVERFLOW;
        break;
    case HPCLANG_RPL_STORE_UNAVAILABLE:
        errno = EIO;
        break;
    case HPCLANG_RPL_STORE_NOT_DIRECTORY:
    case HPCLANG_RPL_STORE_MALFORMED:
    default:
        errno = EILSEQ;
        break;
    }
    return -1;
}

static int hp39_empty_list_snapshot(struct hpclang_object **object)
{
    uint8_t encoded[5] = { 0u, 0u, 0u, 0u, 0u };
    struct hp39_catalog_writer writer = {
        .bytes = encoded,
        .capacity_nibbles = 10u,
        .offset = 0u,
    };
    if (!hp39_buffer_put_word(&writer, HP39_RPL_DOLIST, 5u) ||
        !hp39_buffer_put_word(&writer, HP39_RPL_SEMI, 5u) ||
        writer.offset != 10u) {
        errno = EILSEQ;
        return -1;
    }

    struct hp39_buffer_reader buffer = {
        .bytes = encoded,
        .nibble_count = 10u,
    };
    struct hpclang_rpl_reader encoded_reader = {
        .read_nibble = hp39_buffer_read_nibble,
        .context = &buffer,
    };
    return hpclang_object_snapshot_copy(
        &encoded_reader, 0u, 10u, object);
}

int hpclang_hp39_native_aplet_catalog_snapshot(
    const struct hpclang_rpl_reader *reader,
    struct hpclang_object **object)
{
    static const uint8_t aplet_list_name[] = {
        '\'', 'A', 'p', 'D', 'i', 'r', 'L', 'i', 's', 't'
    };

    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t home = 0u;
    int result = hpclang_rpl_store_home_object(reader, &home);
    if (result != HPCLANG_RPL_STORE_OK)
        return hp39_store_errno(result);

    /*
     * HP39 HOME carries one null-named Hidden directory. The Stock
     * GetApDirList word enters that directory and reads the private
     * apostrophe-prefixed ApDirList variable. Reproduce the same lookup
     * read-only instead of re-entering the RPL evaluator while native pages
     * occupy the ordinary temporary-object arena.
     */
    struct hpclang_rpl_directory_entry_view hidden;
    result = hpclang_rpl_directory_find(
        reader, home, NULL, 0u, &hidden);
    if (result != HPCLANG_RPL_STORE_OK)
        return hp39_store_errno(result);
    if (hidden.prologue != HP39_RPL_DORRP) {
        errno = EILSEQ;
        return -1;
    }

    struct hpclang_rpl_directory_entry_view aplet_list;
    result = hpclang_rpl_directory_find(
        reader,
        hidden.object_address,
        aplet_list_name,
        (uint32_t)sizeof(aplet_list_name),
        &aplet_list);
    if (result == HPCLANG_RPL_STORE_NOT_FOUND)
        return hp39_empty_list_snapshot(object);
    if (result != HPCLANG_RPL_STORE_OK)
        return hp39_store_errno(result);
    if (aplet_list.prologue != HP39_RPL_DOLIST) {
        errno = EILSEQ;
        return -1;
    }

    if (hpclang_object_snapshot_copy(
            reader,
            aplet_list.object_address,
            HP39_SATURN_LIMIT - aplet_list.object_address,
            object) != 0)
        return -1;
    if (*object == NULL ||
        hpclang_object_kind(*object) != HPCLANG_OBJECT_LIST) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

int hpclang_hp39_native_aplet_snapshot(
    const struct hpclang_rpl_reader *reader,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    static const uint8_t aplet_directory_name[] = {
        '\'', 'a', 'p', 'l', 'e', 't', 'd', 'i', 'r'
    };

    if (object == NULL || name == NULL || name_length == 0u ||
        name_length > HPCLANG_RPL_STORE_NAME_CAPACITY - 2u) {
        if (object != NULL)
            *object = NULL;
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t home = 0u;
    int result = hpclang_rpl_store_home_object(reader, &home);
    if (result != HPCLANG_RPL_STORE_OK)
        return hp39_store_errno(result);

    struct hpclang_rpl_directory_entry_view aplet_directory;
    result = hpclang_rpl_directory_find(
        reader,
        home,
        aplet_directory_name,
        (uint32_t)sizeof(aplet_directory_name),
        &aplet_directory);
    if (result != HPCLANG_RPL_STORE_OK)
        return hp39_store_errno(result);
    if (aplet_directory.prologue != HP39_RPL_DORRP) {
        errno = EILSEQ;
        return -1;
    }

    uint8_t stored_name[HPCLANG_RPL_STORE_NAME_CAPACITY];
    stored_name[0] = HP39_CATALOG_PREFIX;
    for (size_t i = 0u; i < name_length; ++i)
        stored_name[i + 1u] = name[i];

    struct hpclang_rpl_directory_entry_view entry;
    result = hpclang_rpl_directory_find(
        reader,
        aplet_directory.object_address,
        stored_name,
        (uint32_t)name_length + 1u,
        &entry);
    if (result != HPCLANG_RPL_STORE_OK)
        return hp39_store_errno(result);
    if (entry.prologue != HP39_RPL_DORRP) {
        errno = EILSEQ;
        return -1;
    }

    if (hpclang_object_snapshot_copy(
            reader,
            entry.object_address,
            HP39_SATURN_LIMIT - entry.object_address,
            object) != 0)
        return -1;
    if (*object == NULL ||
        hpclang_object_kind(*object) != HPCLANG_OBJECT_DIRECTORY) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

int hpclang_hp39_native_catalog_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t context_pointer_address,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    struct hp39_catalog_plan plan = { .nibble_count = 10u };
    if (hp39_walk_directory(
            reader,
            context_pointer_address,
            hp39_catalog_plan_visit,
            &plan) != 0)
        return -1;

    size_t bytes = ((size_t)plan.nibble_count + 1u) / 2u;
    uint8_t *encoded = (uint8_t *)calloc(bytes, 1u);
    if (encoded == NULL) {
        errno = ENOMEM;
        return -1;
    }

    struct hp39_catalog_writer writer = {
        .bytes = encoded,
        .capacity_nibbles = plan.nibble_count,
        .offset = 0u,
    };
    if (!hp39_buffer_put_word(&writer, HP39_RPL_DOLIST, 5u)) {
        free(encoded);
        errno = EILSEQ;
        return -1;
    }
    if (hp39_walk_directory(
            reader,
            context_pointer_address,
            hp39_catalog_write_visit,
            &writer) != 0) {
        free(encoded);
        return -1;
    }
    if (!hp39_buffer_put_word(&writer, HP39_RPL_SEMI, 5u) ||
        writer.offset != plan.nibble_count) {
        free(encoded);
        errno = EILSEQ;
        return -1;
    }

    struct hp39_buffer_reader buffer = {
        .bytes = encoded,
        .nibble_count = plan.nibble_count,
    };
    struct hpclang_rpl_reader encoded_reader = {
        .read_nibble = hp39_buffer_read_nibble,
        .context = &buffer,
    };
    int result = hpclang_object_snapshot_copy(
        &encoded_reader, 0u, plan.nibble_count, object);
    free(encoded);
    if (result != 0)
        return -1;
    if (*object == NULL ||
        hpclang_object_kind(*object) != HPCLANG_OBJECT_LIST) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

static int hp39_body_search_visit(
    const struct hpclang_rpl_reader *reader,
    uint32_t item_address,
    uint32_t name_length,
    uint32_t object_address,
    void *context)
{
    struct hp39_body_search *search = (struct hp39_body_search *)context;
    int selected = 0;
    if (!hp39_item_is_catalog_entry(
            reader, item_address, name_length, &selected)) {
        errno = EIO;
        return -1;
    }
    if (!selected || name_length - 1u != search->name_length)
        return 0;

    for (size_t i = 0u; i < search->name_length; ++i) {
        uint8_t byte = 0u;
        if (!hp39_read_byte(
                reader,
                item_address + 4u + (uint32_t)i * 2u,
                &byte)) {
            errno = EIO;
            return -1;
        }
        if (byte != search->name[i])
            return 0;
    }

    uint32_t prologue = 0u;
    if (!hp39_read_word(reader, object_address, 5u, &prologue)) {
        errno = EIO;
        return -1;
    }
    if (prologue != HP39_RPL_DOCSTR) {
        errno = EILSEQ;
        return -1;
    }

    if (hpclang_object_snapshot_copy(
            reader,
            object_address,
            HP39_SATURN_LIMIT - object_address,
            search->object) != 0)
        return -1;
    if (*search->object == NULL ||
        hpclang_object_kind(*search->object) != HPCLANG_OBJECT_STRING) {
        hpclang_object_release(*search->object);
        *search->object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 1;
}

int hpclang_hp39_native_body_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t context_pointer_address,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    if (object == NULL || name == NULL || name_length == 0u ||
        name_length > UINT8_MAX - 1u) {
        if (object != NULL)
            *object = NULL;
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    struct hp39_body_search search = {
        .name = name,
        .name_length = name_length,
        .object = object,
    };
    int result = hp39_walk_directory(
        reader,
        context_pointer_address,
        hp39_body_search_visit,
        &search);
    if (result == 1)
        return 0;
    if (result < 0)
        return -1;

    errno = ENOENT;
    return -1;
}
