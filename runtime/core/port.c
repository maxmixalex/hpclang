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
#include "rpl-stock-memory.h"
#include "saturn-view.h"
#include "launch-services.h"

int hpclang_port_query(uint32_t port, struct hpclang_port_info *info)
{
    if (port > 2 || !info || info->size < sizeof(*info) ||
        info->version != HPCLANG_PORT_INFO_VERSION) {
        errno = EINVAL;
        return -1;
    }
    const struct hpclang_device_info *device = hpclang_device_info();
    if (!device || device->model < HPCLANG_MODEL_HP39GPLUS || device->model > HPCLANG_MODEL_HP50G) {
        errno = ENOSYS;
        return -1;
    }
    struct hpclang_port_info result = {
        .size = info->size, .version = HPCLANG_PORT_INFO_VERSION, .port = port};
    if (device->model >= HPCLANG_MODEL_HP48GII &&
        (port == 0 || device->model != HPCLANG_MODEL_HP48GII)) {
        result.medium = port == 0   ? HPCLANG_PORT_MAIN_RAM
                        : port == 1 ? HPCLANG_PORT_EXTENDED_RAM
                                    : HPCLANG_PORT_FLASH;
        uint32_t reads =
            HPCLANG_PORT_ENUMERATE | HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_READ_LIBRARY;
        result.implemented = reads;
        if (port == 0)
            result.implemented |= HPCLANG_PORT_CREATE_BACKUP | HPCLANG_PORT_REPLACE_BACKUP |
                                  HPCLANG_PORT_REMOVE_BACKUP;
        else
            result.implemented |= HPCLANG_PORT_CREATE_BACKUP;
        const struct hpclang_boot_info *boot = hpclang_boot_info();
        if (boot && boot->model == device->model && boot->saturn_state) {
            result.available = reads;
            const struct hpclang_rpl_memory_bounds *bounds =
                port < 2 ? hpclang_launch_rpl_bounds(boot) : NULL;
            if (port == 0 && bounds)
                result.available = result.implemented;
            else if (port == 1 && bounds && bounds->magic == HPCLANG_RPL_BOUNDS_MAGIC &&
                     bounds->port1_limit <= HPCLANG_RPL_PORT1_NIBBLES)
                result.available |= HPCLANG_PORT_CREATE_BACKUP;
            else if (port == 2)
                result.available |= HPCLANG_PORT_CREATE_BACKUP;
        }
    }
    *info = result;
    return 0;
}

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

static int port_error(int result)
{
    errno = result == HPCLANG_RPL_SPAN_UNAVAILABLE   ? EIO
            : result == HPCLANG_RPL_SPAN_UNSUPPORTED ? ENOTSUP
                                                     : EILSEQ;
    return -1;
}

static int open_cursor(uint32_t port, struct hpclang_rpl_reader *reader,
                       struct hpclang_rpl_port_cursor *cursor)
{
    if (port > 2) {
        errno = EINVAL;
        return -1;
    }
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!device || !boot || !boot->saturn_state ||
        (device->family != HPCLANG_FAMILY_HP48 && device->family != HPCLANG_FAMILY_HP4950)) {
        errno = ENOSYS;
        return -1;
    }
    if (port == 2) {
        if (device->family != HPCLANG_FAMILY_HP4950 || !hpclang_rpl_stock_port2_reader(0, reader)) {
            errno = ENOSYS;
            return -1;
        }
        *cursor = (struct hpclang_rpl_port_cursor){0x20au, HPCLANG_RPL_FLASH_BANK_NIBBLES, 0};
        return 0;
    }
    if (port == 1) {
        if (device->family != HPCLANG_FAMILY_HP4950 || !hpclang_rpl_stock_port1_reader(reader)) {
            errno = ENOSYS;
            return -1;
        }
        *cursor = (struct hpclang_rpl_port_cursor){0, HPCLANG_RPL_PORT1_NIBBLES, 0};
        return 0;
    }
    *reader = (struct hpclang_rpl_reader){read_nibble, NULL};
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

static int next_entry(uint32_t port, uint32_t *bank, struct hpclang_rpl_reader *reader,
                      struct hpclang_rpl_port_cursor *cursor, struct hpclang_rpl_port_entry *entry)
{
    if (port != 2)
        return hpclang_rpl_port_next(reader, cursor, entry);
    for (;;) {
        if (*bank >= HPCLANG_RPL_PORT2_BANKS)
            return 0;
        /* Check the native bank identity before interpreting any slots. */
        uint32_t header = 0;
        for (uint32_t i = 0; i < 4; ++i) {
            uint8_t nibble;
            if (!reader->read_nibble(reader->context, 0x200u + i, &nibble))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            if (nibble > 15u)
                return HPCLANG_RPL_SPAN_MALFORMED;
            header |= (uint32_t)nibble << (4u * i);
        }
        if (header != (0xf0u | ((*bank + 8u) << 8u)))
            return HPCLANG_RPL_SPAN_MALFORMED;
        int result = hpclang_rpl_flash_next(reader, cursor, entry);
        if (result)
            return result;
        if (++*bank == HPCLANG_RPL_PORT2_BANKS)
            return 0;
        if (!hpclang_rpl_stock_port2_reader(*bank, reader))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        *cursor = (struct hpclang_rpl_port_cursor){0x20au, HPCLANG_RPL_FLASH_BANK_NIBBLES, 0};
    }
}

struct hpclang_port_iterator *hpclang_port_iterator_open(uint32_t port)
{
    struct hpclang_rpl_port_cursor cursor;
    struct hpclang_rpl_reader reader;
    if (open_cursor(port, &reader, &cursor))
        return NULL;
    struct hpclang_rpl_port_entry native;
    size_t count = 0;
    uint32_t bank = 0;
    int result;
    while ((result = next_entry(port, &bank, &reader, &cursor, &native)) > 0)
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
    iterator->count = (uint32_t)count;
    iterator->index = 0;
    bank = 0;
    if (open_cursor(port, &reader, &cursor)) {
        free(iterator);
        return NULL;
    }
    uint32_t index = 0;
    while ((result = next_entry(port, &bank, &reader, &cursor, &native)) > 0) {
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
    struct hpclang_rpl_reader reader;
    if (open_cursor(port, &reader, &cursor))
        return NULL;
    struct hpclang_rpl_port_entry entry;
    int result;
    uint32_t bank = 0;
    while ((result = next_entry(port, &bank, &reader, &cursor, &entry)) > 0) {
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

struct hpclang_object *hpclang_port_library_get(uint32_t port, uint32_t library_number)
{
    if (library_number > 0xfff) {
        errno = EINVAL;
        return NULL;
    }
    struct hpclang_rpl_port_cursor cursor;
    struct hpclang_rpl_reader reader;
    if (open_cursor(port, &reader, &cursor))
        return NULL;
    struct hpclang_rpl_port_entry entry;
    int result;
    uint32_t bank = 0;
    while ((result = next_entry(port, &bank, &reader, &cursor, &entry)) > 0) {
        if (entry.prologue != 0x02b40 || entry.library_number != library_number)
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
