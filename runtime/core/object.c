#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object.h>
#include <hpclang/target.h>

#include "object-snapshot.h"
#include "rpl-mutation.h"
#include "rpl-stock-memory.h"
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
    struct hpclang_rpl_stock_memory memory;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
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
    struct hpclang_rpl_stock_memory memory;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
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
    struct hpclang_rpl_stock_memory memory;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
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
    struct hpclang_rpl_stock_memory memory;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
        errno = ENOSYS;
        return -1;
    }

    int result = hpclang_rpl_mutation_replace(&io, level, object);
    return result == HPCLANG_RPL_MUTATION_OK ? 0 : set_mutation_errno(result);
}

static int stack_reorder(int nip)
{
    struct hpclang_rpl_mutation_io io;
    struct hpclang_rpl_stock_memory memory;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
        errno = ENOSYS;
        return -1;
    }
    int result = hpclang_rpl_mutation_reorder(&io, nip);
    return result == HPCLANG_RPL_MUTATION_OK ? 0 : set_mutation_errno(result);
}

int hpclang_stack_swap(void)
{
    return stack_reorder(0);
}

int hpclang_stack_nip(void)
{
    return stack_reorder(1);
}

static int boolean_reference_valid(uint32_t address)
{
    uint32_t entry = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = 0u;
        if (!stock_read_nibble(NULL, address + i, &nibble) || nibble > 15u) {
            errno = EIO;
            return 0;
        }
        entry |= (uint32_t)nibble << (4u * i);
    }
    if (entry != address + 5u) {
        errno = EILSEQ;
        return 0;
    }
    return 1;
}

int hpclang_stack_push_boolean(int value)
{
    if (value != 0 && value != 1) {
        errno = EINVAL;
        return -1;
    }
    struct hpclang_rpl_mutation_io io;
    struct hpclang_rpl_stock_memory memory;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
        errno = ENOSYS;
        return -1;
    }
    if (!boolean_reference_valid(value ? 0x03a81u : 0x03ac0u))
        return -1;
    int result = hpclang_rpl_mutation_push_boolean(&io, value);
    return result == HPCLANG_RPL_MUTATION_OK ? 0 : set_mutation_errno(result);
}

int hpclang_stack_peek_boolean(uint32_t level, int *value)
{
    if (value == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (!stack_family_available()) {
        errno = ENOSYS;
        return -1;
    }
    uint32_t address = 0u;
    int result = hpclang_rpl_stack_object_address(&stock_reader, level, &address);
    if (result != HPCLANG_RPL_STACK_OK)
        return set_stack_errno(result);
    if (address != 0x03a81u && address != 0x03ac0u) {
        errno = EINVAL;
        return -1;
    }
    if (!boolean_reference_valid(address))
        return -1;
    *value = address == 0x03a81u;
    return 0;
}

int hpclang_stack_pop_boolean(int *value)
{
    if (value == NULL) {
        errno = EFAULT;
        return -1;
    }
    struct hpclang_rpl_mutation_io io;
    struct hpclang_rpl_stock_memory memory;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory)) {
        errno = ENOSYS;
        return -1;
    }
    int result_value = 0;
    if (hpclang_stack_peek_boolean(1u, &result_value) != 0)
        return -1;
    int result = hpclang_rpl_mutation_drop(&io, 1u);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return set_mutation_errno(result);
    *value = result_value;
    return 0;
}
