#include <errno.h>
#include <string.h>
#include <hpclang/abi.h>
#include "dma-buffer.h"

static int acquire(const struct hpclang_dma_allocator *allocator, uint32_t bytes,
                   uint32_t alignment, uint32_t slack, struct hpclang_dma_buffer *buffer)
{
    size_t reserved = (size_t)bytes + slack;
    void *allocation = allocator->allocate(allocator->context, reserved);
    if (!allocation) {
        errno = ENOMEM;
        return -1;
    }
    uintptr_t base = (uintptr_t)allocation;
    int error = ERANGE;
    if (reserved <= UINTPTR_MAX - base) {
        uintptr_t limit = base + reserved;
        uintptr_t candidate = (base + alignment - 1u) & ~(uintptr_t)(alignment - 1u);
        while (candidate <= limit && bytes <= limit - candidate) {
            uint32_t physical;
            if (allocator->translate(allocator->context, (const void *)candidate, bytes,
                                     &physical) == 0) {
                if ((physical & (alignment - 1u)) || bytes - 1u > UINT32_MAX - physical) {
                    error = ERANGE;
                    break;
                }
                memset((void *)candidate, 0, bytes);
                *buffer = (struct hpclang_dma_buffer){allocation,         (uint8_t *)candidate,
                                                      physical,           bytes,
                                                      allocator->release, allocator->context};
                return 0;
            }
            error = errno;
            if (error != ENOTSUP)
                break;
            /* Moving inside this page cannot bypass a later mapping gap.
             * Check the next page boundary, without wrapping uintptr_t. */
            uintptr_t end = candidate | (HPCLANG_PAGE_SIZE - 1u);
            if (end == UINTPTR_MAX)
                break;
            candidate = end + 1u;
        }
    }
    allocator->release(allocator->context, allocation);
    errno = error;
    return -1;
}

int hpclang_dma_buffer_acquire(const struct hpclang_dma_allocator *allocator, uint32_t bytes,
                               uint32_t alignment, struct hpclang_dma_buffer *buffer)
{
    if (!allocator || !allocator->allocate || !allocator->release || !allocator->translate ||
        !buffer || !bytes || bytes > UINT32_MAX - (HPCLANG_PAGE_SIZE - 1u) || !alignment ||
        alignment > HPCLANG_PAGE_SIZE || (alignment & (alignment - 1u))) {
        errno = EINVAL;
        return -1;
    }
    if (buffer->allocation) {
        errno = EBUSY;
        return -1;
    }
    if (acquire(allocator, bytes, alignment, alignment - 1u, buffer) == 0)
        return 0;
    if (errno != ENOTSUP || alignment == HPCLANG_PAGE_SIZE)
        return -1;
    return acquire(allocator, bytes, alignment, HPCLANG_PAGE_SIZE - 1u, buffer);
}

void hpclang_dma_buffer_release(struct hpclang_dma_buffer *buffer)
{
    if (!buffer || !buffer->allocation)
        return;
    buffer->release(buffer->context, buffer->allocation);
    *buffer = (struct hpclang_dma_buffer){0};
}
