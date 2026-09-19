#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object.h>
#include <hpclang/target.h>

#include "object-snapshot.h"
#include "rpl-stack-view.h"
#include "saturn-view.h"

enum {
    SATURN_SPACE_NIBBLES = 0x00100000u
};

static int stack_family_available(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL ||
        (device->family != HPCLANG_FAMILY_HP48 &&
         device->family != HPCLANG_FAMILY_HP4950))
        return 0;

    const struct hpclang_boot_info *boot = hpclang_boot_info();
    return boot != NULL && boot->saturn_state != 0u;
}

static int stock_read_nibble(
    void *context,
    uint32_t nibble_address,
    uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(
        hpclang_saturn_address_make(nibble_address), value);
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

int hpclang_stack_peek(
    uint32_t level,
    struct hpclang_object **object)
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
    int result =
        hpclang_rpl_stack_object_address(&stock_reader, level, &address);
    if (result != HPCLANG_RPL_STACK_OK)
        return set_stack_errno(result);
    if (address >= SATURN_SPACE_NIBBLES) {
        errno = EILSEQ;
        return -1;
    }

    return hpclang_object_snapshot_copy(
        &stock_reader,
        address,
        SATURN_SPACE_NIBBLES - address,
        object);
}
