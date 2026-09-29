#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <hpclang/target.h>

#include "object-snapshot.h"
#include "rpl-mutation.h"
#include "rpl-object-store.h"
#include "saturn-view.h"

enum
{
    RPL_DORRP = 0x02a96u,
    RPL_SPACE_NIBBLES = 0x00100000u
};

struct hpclang_object_store
{
    uint32_t component_count;
    uint32_t encoded_size;
    uint8_t encoded_path[];
};

struct object_store_iterator_entry
{
    uint32_t kind;
    uint32_t flags;
    uint32_t name_length;
    uint8_t name[HPCLANG_OBJECT_STORE_NAME_CAPACITY];
};

struct hpclang_object_store_iterator
{
    uint32_t count;
    uint32_t index;
    struct object_store_iterator_entry entries[];
};

static int stock_store_available(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL ||
        (device->family != HPCLANG_FAMILY_HP48 && device->family != HPCLANG_FAMILY_HP4950))
        return 0;

    const struct hpclang_boot_info *boot = hpclang_boot_info();
    return boot != NULL && boot->saturn_state != 0u;
}

static int stock_read_nibble(void *context, uint32_t nibble_address, uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(hpclang_saturn_address_make(nibble_address), value);
}

static int stock_write_nibble(void *context, uint32_t nibble_address, uint8_t value)
{
    (void)context;
    return hpclang_saturn_write_nibble(hpclang_saturn_address_make(nibble_address), value);
}

static int stock_mutation_io(struct hpclang_rpl_mutation_io *io)
{
    if (io == NULL || !stock_store_available())
        return 0;

    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL || (boot->platform_cookie & HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG) == 0u)
        return 0;

    uintptr_t raw = boot->platform_cookie & ~(uintptr_t)HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    const struct hpclang_rpl_memory_bounds *bounds = (const struct hpclang_rpl_memory_bounds *)raw;
    if (bounds == NULL || bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC ||
        bounds->temp_limit <= UINT32_C(0x00080000) || bounds->temp_limit > UINT32_C(0x00100000) ||
        bounds->user_limit < UINT32_C(0x00080000) || bounds->user_limit >= UINT32_C(0x00100000) ||
        bounds->stack_reserve_nibbles > UINT32_C(0x00100000) - bounds->user_limit ||
        (bounds->stack_reserve_nibbles % 5u) != 0u)
        return 0;

    io->read_nibble = stock_read_nibble;
    io->write_nibble = stock_write_nibble;
    io->context = NULL;
    io->temp_limit = bounds->temp_limit;
    io->user_limit = bounds->user_limit;
    io->stack_reserve_nibbles = bounds->stack_reserve_nibbles;
    io->native_roots = bounds->native_roots;
    io->native_root_count = bounds->native_root_count;
    return 1;
}

static const struct hpclang_rpl_reader stock_reader = {
    .read_nibble = stock_read_nibble,
    .context = NULL,
};

static int set_store_errno(int result)
{
    switch (result) {
    case HPCLANG_RPL_STORE_NOT_FOUND:
        errno = ENOENT;
        break;
    case HPCLANG_RPL_STORE_NOT_DIRECTORY:
        errno = ENOTDIR;
        break;
    case HPCLANG_RPL_STORE_LIMIT:
        errno = ENAMETOOLONG;
        break;
    case HPCLANG_RPL_STORE_UNAVAILABLE:
        errno = EIO;
        break;
    case HPCLANG_RPL_STORE_MALFORMED:
    default:
        errno = EILSEQ;
        break;
    }
    return -1;
}

static struct hpclang_object_store *alloc_store(const uint8_t *encoded_path, uint32_t encoded_size,
                                                uint32_t component_count)
{
    if (encoded_size > HPCLANG_RPL_STORE_PATH_CAPACITY) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    struct hpclang_object_store *store = malloc(sizeof(*store) + (size_t)encoded_size);
    if (store == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    store->component_count = component_count;
    store->encoded_size = encoded_size;
    if (encoded_size != 0u)
        memcpy(store->encoded_path, encoded_path, encoded_size);
    return store;
}

static int validate_name(const uint8_t *name, uint32_t name_length)
{
    /*
     * A zero-length name is a real classic-RPL name: the Hidden directory and
     * null-named variables use it. NULL is therefore valid exactly when the
     * explicit length is zero.
     */
    if ((name == NULL && name_length != 0u) || name_length >= HPCLANG_OBJECT_STORE_NAME_CAPACITY) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

static int resolve_store(const struct hpclang_object_store *store, uint32_t *directory)
{
    if (store == NULL || directory == NULL) {
        errno = EFAULT;
        return -1;
    }

    uint32_t current = 0u;
    int result = hpclang_rpl_store_home_object(&stock_reader, &current);
    if (result != HPCLANG_RPL_STORE_OK)
        return set_store_errno(result);

    uint32_t offset = 0u;
    uint32_t components = 0u;
    while (offset < store->encoded_size) {
        uint32_t length = store->encoded_path[offset++];
        if (length > store->encoded_size - offset) {
            errno = EILSEQ;
            return -1;
        }

        struct hpclang_rpl_directory_entry_view entry;
        result = hpclang_rpl_directory_find(&stock_reader, current, store->encoded_path + offset,
                                            length, &entry);
        if (result != HPCLANG_RPL_STORE_OK)
            return set_store_errno(result);
        if (entry.prologue != RPL_DORRP) {
            errno = ENOTDIR;
            return -1;
        }

        current = entry.object_address;
        offset += length;
        ++components;
    }

    if (offset != store->encoded_size || components != store->component_count) {
        errno = EILSEQ;
        return -1;
    }

    *directory = current;
    return 0;
}

struct hpclang_object_store *hpclang_object_store_home(void)
{
    if (!stock_store_available()) {
        errno = ENOSYS;
        return NULL;
    }

    uint32_t ignored = 0u;
    int result = hpclang_rpl_store_home_object(&stock_reader, &ignored);
    if (result != HPCLANG_RPL_STORE_OK) {
        set_store_errno(result);
        return NULL;
    }

    return alloc_store(NULL, 0u, 0u);
}

struct hpclang_object_store *hpclang_object_store_current(void)
{
    if (!stock_store_available()) {
        errno = ENOSYS;
        return NULL;
    }

    uint32_t current = 0u;
    int result = hpclang_rpl_store_current_object(&stock_reader, &current);
    if (result != HPCLANG_RPL_STORE_OK) {
        set_store_errno(result);
        return NULL;
    }

    uint8_t *path = malloc(HPCLANG_RPL_STORE_PATH_CAPACITY);
    if (path == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    uint32_t encoded_size = 0u;
    uint32_t component_count = 0u;
    result =
        hpclang_rpl_directory_path_to(&stock_reader, current, path, HPCLANG_RPL_STORE_PATH_CAPACITY,
                                      &encoded_size, &component_count);
    if (result != HPCLANG_RPL_STORE_OK) {
        free(path);
        set_store_errno(result);
        return NULL;
    }

    struct hpclang_object_store *store = alloc_store(path, encoded_size, component_count);
    free(path);
    return store;
}

struct hpclang_object_store *
hpclang_object_store_open_child(const struct hpclang_object_store *parent, const uint8_t *name,
                                uint32_t name_length)
{
    if (!stock_store_available()) {
        errno = ENOSYS;
        return NULL;
    }
    if (parent == NULL) {
        errno = EFAULT;
        return NULL;
    }
    if (!validate_name(name, name_length))
        return NULL;

    uint32_t directory = 0u;
    if (resolve_store(parent, &directory) != 0)
        return NULL;

    struct hpclang_rpl_directory_entry_view entry;
    int result = hpclang_rpl_directory_find(&stock_reader, directory, name, name_length, &entry);
    if (result != HPCLANG_RPL_STORE_OK) {
        set_store_errno(result);
        return NULL;
    }
    if (entry.prologue != RPL_DORRP) {
        errno = ENOTDIR;
        return NULL;
    }

    uint32_t extra = 1u + name_length;
    if (extra > UINT32_MAX - parent->encoded_size) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    uint32_t encoded_size = parent->encoded_size + extra;
    if (encoded_size > HPCLANG_RPL_STORE_PATH_CAPACITY) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    uint8_t *path = malloc(encoded_size);
    if (path == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    if (parent->encoded_size != 0u)
        memcpy(path, parent->encoded_path, parent->encoded_size);
    path[parent->encoded_size] = (uint8_t)name_length;
    if (name_length != 0u) {
        memcpy(path + parent->encoded_size + 1u, name, name_length);
    }

    struct hpclang_object_store *child =
        alloc_store(path, encoded_size, parent->component_count + 1u);
    free(path);
    return child;
}

void hpclang_object_store_close(struct hpclang_object_store *store)
{
    free(store);
}

int hpclang_object_store_get(const struct hpclang_object_store *store, const uint8_t *name,
                             uint32_t name_length, struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    if (!stock_store_available()) {
        errno = ENOSYS;
        return -1;
    }
    if (!validate_name(name, name_length))
        return -1;

    uint32_t directory = 0u;
    if (resolve_store(store, &directory) != 0)
        return -1;

    struct hpclang_rpl_directory_entry_view entry;
    int result = hpclang_rpl_directory_find(&stock_reader, directory, name, name_length, &entry);
    if (result != HPCLANG_RPL_STORE_OK)
        return set_store_errno(result);
    if (entry.prologue == RPL_DORRP) {
        errno = EISDIR;
        return -1;
    }
    if (entry.object_address >= RPL_SPACE_NIBBLES) {
        errno = EILSEQ;
        return -1;
    }

    return hpclang_object_snapshot_copy(&stock_reader, entry.object_address,
                                        RPL_SPACE_NIBBLES - entry.object_address, object);
}

static int set_mutation_errno(int result)
{
    switch (result) {
    case HPCLANG_RPL_MUTATION_OK:
        return 0;
    case HPCLANG_RPL_MUTATION_BUSY:
        errno = EBUSY;
        break;
    case HPCLANG_RPL_MUTATION_NOT_FOUND:
        errno = ENOENT;
        break;
    case HPCLANG_RPL_MUTATION_DIRECTORY:
        errno = EISDIR;
        break;
    case HPCLANG_RPL_MUTATION_UNSUPPORTED:
        errno = ENOTSUP;
        break;
    case HPCLANG_RPL_MUTATION_EXISTS:
        errno = EEXIST;
        break;
    case HPCLANG_RPL_MUTATION_NOSPACE:
        errno = ENOSPC;
        break;
    case HPCLANG_RPL_MUTATION_MALFORMED:
        errno = EILSEQ;
        break;
    case HPCLANG_RPL_MUTATION_IO:
        errno = EIO;
        break;
    case HPCLANG_RPL_MUTATION_LEVEL:
        errno = EINVAL;
        break;
    case HPCLANG_RPL_MUTATION_UNAVAILABLE:
    default:
        errno = ENOSYS;
        break;
    }
    return -1;
}

static int mutation_prepare(const struct hpclang_object_store *store, const uint8_t *name,
                            uint32_t name_length, uint32_t *directory,
                            struct hpclang_rpl_mutation_io *io)
{
    if (store == NULL || directory == NULL || io == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (!stock_store_available()) {
        errno = ENOSYS;
        return -1;
    }
    if (!validate_name(name, name_length))
        return -1;
    if (resolve_store(store, directory) != 0)
        return -1;
    if (!stock_mutation_io(io)) {
        errno = ENOSYS;
        return -1;
    }
    return 0;
}

/*
 * Private M7.3f bridges used by Stock E2E while the public put/remove contract
 * is still being completed. They expose no Saturn address and accept only an
 * owned snapshot. Each call re-resolves the logical store handle, so USEROB
 * movement cannot leave a stale directory address in the caller.
 */
int hpclang_object_store_replace_existing(const struct hpclang_object_store *store,
                                          const uint8_t *name, uint32_t name_length,
                                          const struct hpclang_object *object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }

    uint32_t directory = 0u;
    struct hpclang_rpl_mutation_io io;
    if (mutation_prepare(store, name, name_length, &directory, &io) != 0)
        return -1;

    return set_mutation_errno(
        hpclang_rpl_mutation_store_replace(&io, directory, name, name_length, object));
}

int hpclang_object_store_replace_existing_same_size(const struct hpclang_object_store *store,
                                                    const uint8_t *name, uint32_t name_length,
                                                    const struct hpclang_object *object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }

    uint32_t directory = 0u;
    struct hpclang_rpl_mutation_io io;
    if (mutation_prepare(store, name, name_length, &directory, &io) != 0)
        return -1;

    return set_mutation_errno(
        hpclang_rpl_mutation_store_replace_same_size(&io, directory, name, name_length, object));
}

int hpclang_object_store_create_new(const struct hpclang_object_store *store, const uint8_t *name,
                                    uint32_t name_length, const struct hpclang_object *object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }

    uint32_t directory = 0u;
    struct hpclang_rpl_mutation_io io;
    if (mutation_prepare(store, name, name_length, &directory, &io) != 0)
        return -1;

    return set_mutation_errno(
        hpclang_rpl_mutation_store_create(&io, directory, name, name_length, object));
}

struct hpclang_object_store_iterator *
hpclang_object_store_iterator_open(const struct hpclang_object_store *store)
{
    if (!stock_store_available()) {
        errno = ENOSYS;
        return NULL;
    }

    uint32_t directory = 0u;
    if (resolve_store(store, &directory) != 0)
        return NULL;

    struct hpclang_rpl_directory_cursor cursor;
    int result = hpclang_rpl_directory_cursor_open(&stock_reader, directory, &cursor);
    if (result != HPCLANG_RPL_STORE_OK) {
        set_store_errno(result);
        return NULL;
    }

    uint32_t count = 0u;
    struct hpclang_rpl_directory_entry_view view;
    while ((result = hpclang_rpl_directory_cursor_next(&stock_reader, &cursor, &view)) > 0) {
        if (count == UINT32_MAX) {
            errno = ENOMEM;
            return NULL;
        }
        ++count;
    }
    if (result < 0) {
        set_store_errno(result);
        return NULL;
    }

    /*
     * The private cursor rejects more than RPL_DIRECTORY_ENTRY_LIMIT (4096)
     * entries before this point, so this fixed-size metadata snapshot is
     * bounded independently of host size_t width.
     */
    size_t entry_size = sizeof(struct object_store_iterator_entry);
    struct hpclang_object_store_iterator *iterator =
        malloc(sizeof(*iterator) + (size_t)count * entry_size);
    if (iterator == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    iterator->count = count;
    iterator->index = 0u;

    result = hpclang_rpl_directory_cursor_open(&stock_reader, directory, &cursor);
    if (result != HPCLANG_RPL_STORE_OK) {
        free(iterator);
        set_store_errno(result);
        return NULL;
    }

    uint32_t index = 0u;
    while ((result = hpclang_rpl_directory_cursor_next(&stock_reader, &cursor, &view)) > 0) {
        if (index >= count) {
            free(iterator);
            errno = EBUSY;
            return NULL;
        }
        struct object_store_iterator_entry *dst = &iterator->entries[index++];
        memset(dst, 0, sizeof(*dst));
        dst->kind = (uint32_t)hpclang_object_kind_from_native_prologue(view.prologue);
        dst->flags = view.prologue == RPL_DORRP ? HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY : 0u;
        dst->name_length = view.name_length;
        memcpy(dst->name, view.name, view.name_length);
    }
    if (result < 0 || index != count) {
        free(iterator);
        if (result < 0)
            set_store_errno(result);
        else
            errno = EBUSY;
        return NULL;
    }

    return iterator;
}

int hpclang_object_store_iterator_read(struct hpclang_object_store_iterator *iterator,
                                       struct hpclang_object_store_entry *entry)
{
    if (iterator == NULL || entry == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (entry->size < sizeof(*entry) || entry->version != HPCLANG_OBJECT_STORE_ENTRY_VERSION) {
        errno = EINVAL;
        return -1;
    }
    if (iterator->index >= iterator->count)
        return 0;

    const struct object_store_iterator_entry *src = &iterator->entries[iterator->index++];
    uint32_t size = entry->size;
    uint32_t version = entry->version;
    memset(entry, 0, sizeof(*entry));
    entry->size = size;
    entry->version = version;
    entry->kind = src->kind;
    entry->flags = src->flags;
    entry->name_length = src->name_length;
    memcpy(entry->name, src->name, src->name_length);
    return 1;
}

void hpclang_object_store_iterator_close(struct hpclang_object_store_iterator *iterator)
{
    free(iterator);
}
