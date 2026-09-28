#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/object.h>
#include <hpclang/target.h>

#include "object-snapshot.h"
#include "rpl-mutation.h"
#include "rpl-stack-view.h"
#include "saturn-view.h"

enum
{
    SATURN_SPACE_NIBBLES = 0x00100000u
};

static int stack_family_available(void)
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
    if (io == NULL || !stack_family_available())
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
    return 1;
}

static int set_mutation_errno(int result)
{
    switch (result) {
    case HPCLANG_RPL_MUTATION_NOSPACE:
        errno = ENOSPC;
        break;
    case HPCLANG_RPL_MUTATION_LEVEL:
        errno = EINVAL;
        break;
    case HPCLANG_RPL_MUTATION_MALFORMED:
        errno = EILSEQ;
        break;
    case HPCLANG_RPL_MUTATION_IO:
        errno = EIO;
        break;
    case HPCLANG_RPL_MUTATION_UNAVAILABLE:
    default:
        errno = ENOSYS;
        break;
    }
    return -1;
}

static const struct hpclang_rpl_reader stock_reader = {
    .read_nibble = stock_read_nibble,
    .context = NULL,
};

static int set_stack_errno(int result)
{
    switch (result) {
    case HPCLANG_RPL_STACK_LEVEL:
        errno = EINVAL;
        break;
    case HPCLANG_RPL_STACK_UNAVAILABLE:
        errno = EIO;
        break;
    case HPCLANG_RPL_STACK_MALFORMED:
    default:
        errno = EILSEQ;
        break;
    }
    return -1;
}

int hpclang_stack_depth(uint32_t *depth)
{
    if (depth == NULL) {
        errno = EFAULT;
        return -1;
    }
    *depth = 0u;

    if (!stack_family_available()) {
        errno = ENOSYS;
        return -1;
    }

    int result = hpclang_rpl_stack_depth(&stock_reader, depth);
    return result == HPCLANG_RPL_STACK_OK ? 0 : set_stack_errno(result);
}

int hpclang_stack_peek(uint32_t level, struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    if (!stack_family_available()) {
        errno = ENOSYS;
        return -1;
    }

    uint32_t address = 0u;
    int result = hpclang_rpl_stack_object_address(&stock_reader, level, &address);
    if (result != HPCLANG_RPL_STACK_OK)
        return set_stack_errno(result);
    if (address >= SATURN_SPACE_NIBBLES) {
        errno = EILSEQ;
        return -1;
    }

    return hpclang_object_snapshot_copy(&stock_reader, address, SATURN_SPACE_NIBBLES - address,
                                        object);
}

int hpclang_stack_drop(uint32_t count)
{
    struct hpclang_rpl_mutation_io io;
    if (!stock_mutation_io(&io)) {
        errno = ENOSYS;
        return -1;
    }

    int result = hpclang_rpl_mutation_drop(&io, count);
    return result == HPCLANG_RPL_MUTATION_OK ? 0 : set_mutation_errno(result);
}

int hpclang_stack_pop(struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    struct hpclang_rpl_mutation_io io;
    if (!stock_mutation_io(&io)) {
        errno = ENOSYS;
        return -1;
    }

    struct hpclang_object *snapshot = NULL;
    if (hpclang_stack_peek(1u, &snapshot) != 0)
        return -1;

    int result = hpclang_rpl_mutation_drop(&io, 1u);
    if (result != HPCLANG_RPL_MUTATION_OK) {
        hpclang_object_release(snapshot);
        return set_mutation_errno(result);
    }

    *object = snapshot;
    return 0;
}

int hpclang_stack_push(const struct hpclang_object *object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }

    struct hpclang_rpl_mutation_io io;
    if (!stock_mutation_io(&io)) {
        errno = ENOSYS;
        return -1;
    }

    int result = hpclang_rpl_mutation_push_snapshot(&io, object);
    return result == HPCLANG_RPL_MUTATION_OK ? 0 : set_mutation_errno(result);
}

int hpclang_stack_replace(uint32_t level, const struct hpclang_object *object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }

    struct hpclang_rpl_mutation_io io;
    if (!stock_mutation_io(&io)) {
        errno = ENOSYS;
        return -1;
    }

    int result = hpclang_rpl_mutation_replace(&io, level, object);
    return result == HPCLANG_RPL_MUTATION_OK ? 0 : set_mutation_errno(result);
}
