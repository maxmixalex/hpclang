#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/port.h>
#include <hpclang/target.h>

#include "object-snapshot.h"
#include "rpl-port.h"
#include "saturn-view.h"

struct hpclang_port_iterator
{
    uint32_t count;
    uint32_t index;
    struct hpclang_port_entry entries[];
};

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(hpclang_saturn_address_make(address), value);
}

static const struct hpclang_rpl_reader reader = {read_nibble, NULL};

static int port_error(int result)
{
    errno = result == HPCLANG_RPL_SPAN_UNAVAILABLE   ? EIO
            : result == HPCLANG_RPL_SPAN_UNSUPPORTED ? ENOTSUP
                                                     : EILSEQ;
    return -1;
}

static int open_cursor(uint32_t port, struct hpclang_rpl_port_cursor *cursor)
{
    if (port > 2) {
        errno = EINVAL;
        return -1;
    }
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (port != 0 || !device || !boot || !boot->saturn_state ||
        (device->family != HPCLANG_FAMILY_HP48 && device->family != HPCLANG_FAMILY_HP4950)) {
        errno = ENOSYS;
        return -1;
    }
    uint32_t start = 0;
    for (uint32_t i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!read_nibble(NULL, 0x80716 + i, &nibble))
            return port_error(HPCLANG_RPL_SPAN_UNAVAILABLE);
        if (nibble > 15)
            return port_error(HPCLANG_RPL_SPAN_MALFORMED);
        start |= (uint32_t)nibble << (4 * i);
    }
    /* Port 0 belongs to main RAM. Its terminating zero is model dependent. */
    if (start < 0x80000 || start > 0xffffb)
        return port_error(HPCLANG_RPL_SPAN_BOUNDS);
    *cursor = (struct hpclang_rpl_port_cursor){start, 0x100000, 0};
    return 0;
}

struct hpclang_port_iterator *hpclang_port_iterator_open(uint32_t port)
{
    struct hpclang_rpl_port_cursor initial;
    if (open_cursor(port, &initial))
        return NULL;
    struct hpclang_rpl_port_cursor cursor = initial;
    struct hpclang_rpl_port_entry native;
    size_t count = 0;
    int result;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &native)) > 0)
        ++count;
    if (result < 0) {
        port_error(result);
        return NULL;
    }
    if (count >
        (SIZE_MAX - sizeof(struct hpclang_port_iterator)) / sizeof(struct hpclang_port_entry)) {
        errno = ENOMEM;
        return NULL;
    }
    struct hpclang_port_iterator *iterator =
        malloc(sizeof(*iterator) + count * sizeof(iterator->entries[0]));
    if (!iterator) {
        errno = ENOMEM;
        return NULL;
    }
    /* Every entry consumes at least 29 nibbles in the 20-bit address space. */
    iterator->count = (uint32_t)count;
    iterator->index = 0;
    cursor = initial;
    uint32_t index = 0;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &native)) > 0) {
        if (index == count)
            break;
        struct hpclang_port_entry *entry = &iterator->entries[index++];
        *entry = (struct hpclang_port_entry){
            .size = sizeof(*entry),
            .version = HPCLANG_PORT_ENTRY_VERSION,
            .kind = native.prologue == 0x02b62 ? HPCLANG_PORT_BACKUP : HPCLANG_PORT_LIBRARY,
            .library_number = native.library_number,
            .name_length = native.name_length};
        memcpy(entry->name, native.name, native.name_length);
    }
    if (result != 0 || index != count) {
        free(iterator);
        if (result < 0)
            port_error(result);
        else
            errno = EBUSY;
        return NULL;
    }
    return iterator;
}

int hpclang_port_iterator_read(struct hpclang_port_iterator *iterator,
                               struct hpclang_port_entry *entry)
{
    if (!iterator || !entry || entry->size < sizeof(*entry) ||
        entry->version != HPCLANG_PORT_ENTRY_VERSION) {
        errno = EINVAL;
        return -1;
    }
    if (iterator->index == iterator->count)
        return 0;
    uint32_t size = entry->size;
    *entry = iterator->entries[iterator->index++];
    entry->size = size;
    return 1;
}

void hpclang_port_iterator_close(struct hpclang_port_iterator *iterator)
{
    free(iterator);
}

struct hpclang_object *hpclang_port_backup_get(uint32_t port, const uint8_t *name,
                                               uint32_t name_length)
{
    if ((!name && name_length) || name_length >= HPCLANG_PORT_NAME_CAPACITY) {
        errno = EINVAL;
        return NULL;
    }
    struct hpclang_rpl_port_cursor cursor;
    if (open_cursor(port, &cursor))
        return NULL;
    struct hpclang_rpl_port_entry entry;
    int result;
    while ((result = hpclang_rpl_port_next(&reader, &cursor, &entry)) > 0) {
        if (entry.prologue != 0x02b62 || entry.name_length != name_length ||
            (name_length && memcmp(entry.name, name, name_length)))
            continue;
        struct hpclang_object *object = NULL;
        if (hpclang_object_snapshot_copy(&reader, entry.value_address, entry.value_span, &object))
            return NULL;
        return object;
    }
    if (result < 0)
        port_error(result);
    else
        errno = ENOENT;
    return NULL;
}
