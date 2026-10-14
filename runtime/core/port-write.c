#include <errno.h>
#include <stdlib.h>
#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/port.h>
#include <hpclang/target.h>
#include "object-snapshot.h"
#include "rpl-backup.h"
#include "rpl-port-mutation.h"
#include "rpl-stock-memory.h"
#include "rpl-flash-record.h"
#include "stock-flash.h"

static int snapshot_read(void *context, uint32_t offset, uint8_t *value)
{
    const struct hpclang_object *const *object = context;
    return hpclang_object_snapshot_read_nibble(*object, offset, value);
}

static int mutation_result(int result);

static int writable_port(uint32_t port, int allow_create)
{
    if (port > 2) {
        errno = EINVAL;
        return -1;
    }
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!device || !boot || !boot->saturn_state ||
        (port > 0 && (!allow_create || device->family != HPCLANG_FAMILY_HP4950)) ||
        (device->family != HPCLANG_FAMILY_HP48 && device->family != HPCLANG_FAMILY_HP4950)) {
        errno = ENOSYS;
        return -1;
    }
    return 0;
}

struct packed_backup
{
    const uint8_t *data;
    uint32_t span;
};

static int packed_read(void *context, uint32_t offset, uint8_t *value)
{
    const struct packed_backup *source = context;
    if (offset >= source->span)
        return 0;
    *value = (source->data[offset / 2u] >> ((offset & 1u) * 4u)) & 15u;
    return 1;
}

static int create_flash_backup(const uint8_t *packed, uint32_t span)
{
    struct packed_backup source = {packed, span};
    const struct hpclang_rpl_reader reader = {packed_read, &source};
    uint8_t *record = NULL;
    uint32_t bytes;
    int result = hpclang_rpl_flash_backup_record(&reader, 0, span, &record, &bytes);
    if (result) {
        errno = result == HPCLANG_RPL_BACKUP_NOMEM  ? ENOMEM
                : result == HPCLANG_RPL_SPAN_BOUNDS ? ENOSPC
                                                    : EILSEQ;
        return -1;
    }
    result = hpclang_stock_flash_create_backup(record, bytes);
    free(record);
    switch (result) {
    case HPCLANG_RPL_FLASH_WRITTEN:
        return 0;
    case HPCLANG_RPL_FLASH_EXISTS:
        errno = EEXIST;
        break;
    case HPCLANG_RPL_FLASH_NOSPACE:
        errno = ENOSPC;
        break;
    case HPCLANG_RPL_FLASH_UNSUPPORTED:
        errno = ENOSYS;
        break;
    case HPCLANG_RPL_FLASH_FAILED:
        errno = EIO;
        break;
    default:
        errno = EILSEQ;
        break;
    }
    return -1;
}

static int write_backup(uint32_t port, const uint8_t *name, uint32_t name_length,
                        const struct hpclang_object *value, int replace)
{
    if ((!name && name_length) || name_length > 255 || !value) {
        errno = EINVAL;
        return -1;
    }
    if (writable_port(port, !replace))
        return -1;
    const struct hpclang_rpl_reader source = {snapshot_read, &value};
    uint8_t *packed = NULL;
    uint32_t span;
    int result = hpclang_rpl_backup_encode(&source, 0, value->nibble_count, name, name_length,
                                           &packed, &span);
    if (result) {
        errno = result == HPCLANG_RPL_BACKUP_NOMEM ? ENOMEM : EILSEQ;
        return -1;
    }
    if (port == 2) {
        result = create_flash_backup(packed, span);
        free(packed);
        return result;
    }
    struct hpclang_rpl_stock_memory memory;
    struct hpclang_rpl_mutation_io io;
    uint32_t limit;
    int available = port == 1 ? hpclang_rpl_stock_port1_mutation_io(&io, &limit)
                              : hpclang_rpl_stock_mutation_io(&io, &memory);
    if (!available) {
        free(packed);
        errno = ENOSYS;
        return -1;
    }
    result = port == 1 ? hpclang_rpl_port1_create_backup(&io, limit, packed, span)
             : replace ? hpclang_rpl_port0_replace_backup(&io, packed, span)
                       : hpclang_rpl_port0_create_backup(&io, packed, span);
    free(packed);
    return mutation_result(result);
}

int hpclang_port_backup_create(uint32_t port, const uint8_t *name, uint32_t name_length,
                               const struct hpclang_object *value)
{
    return write_backup(port, name, name_length, value, 0);
}

int hpclang_port_backup_replace(uint32_t port, const uint8_t *name, uint32_t name_length,
                                const struct hpclang_object *value)
{
    return write_backup(port, name, name_length, value, 1);
}

int hpclang_port_backup_remove(uint32_t port, const uint8_t *name, uint32_t name_length)
{
    if ((!name && name_length) || name_length > 255) {
        errno = EINVAL;
        return -1;
    }
    if (writable_port(port, 0))
        return -1;
    struct hpclang_rpl_stock_memory memory;
    struct hpclang_rpl_mutation_io io;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
        errno = ENOSYS;
        return -1;
    }
    return mutation_result(hpclang_rpl_port0_remove_backup(&io, name, name_length));
}

static int mutation_result(int result)
{
    switch (result) {
    case HPCLANG_RPL_MUTATION_OK:
        return 0;
    case HPCLANG_RPL_MUTATION_EXISTS:
        errno = EEXIST;
        break;
    case HPCLANG_RPL_MUTATION_NOT_FOUND:
        errno = ENOENT;
        break;
    case HPCLANG_RPL_MUTATION_NOSPACE:
        errno = ENOSPC;
        break;
    case HPCLANG_RPL_MUTATION_IO:
        errno = EIO;
        break;
    case HPCLANG_RPL_MUTATION_UNAVAILABLE:
        errno = ENOSYS;
        break;
    case HPCLANG_RPL_MUTATION_UNSUPPORTED:
        errno = ENOTSUP;
        break;
    case HPCLANG_RPL_MUTATION_BUSY:
        errno = EBUSY;
        break;
    default:
        errno = EILSEQ;
        break;
    }
    return -1;
}
