#include <errno.h>
#include <hpclang/bootstrap.h>
#include "dma-span.h"

int hpclang_dma_span(const volatile uint32_t *pages, uint32_t page_count, uint32_t offset,
                     uint32_t bytes, uint32_t alignment, uint32_t *physical)
{
    if (!pages || !physical || !page_count || page_count > HPCLANG_L2_ENTRIES || !bytes ||
        !alignment || (alignment & (alignment - 1))) {
        errno = EINVAL;
        return -1;
    }
    uint32_t capacity = page_count * HPCLANG_PAGE_SIZE;
    if (offset >= capacity || bytes > capacity - offset) {
        errno = EFAULT;
        return -1;
    }

    uint32_t first = offset / HPCLANG_PAGE_SIZE;
    uint32_t last = (offset + bytes - 1) / HPCLANG_PAGE_SIZE;
    uint32_t base = pages[first] & UINT32_C(0xfffff000);
    uint32_t start = base + offset % HPCLANG_PAGE_SIZE;
    if ((start & (alignment - 1)) || bytes - 1 > UINT32_MAX - start) {
        errno = ERANGE;
        return -1;
    }
    for (uint32_t page = first; page <= last; ++page) {
        uint32_t descriptor = pages[page];
        /* Ignore only the cache/buffer bits in the HPC1 descriptor flags. */
        if ((descriptor & UINT32_C(0xff3)) != UINT32_C(0xff2)) {
            errno = EFAULT;
            return -1;
        }
        uint32_t expected = base + (page - first) * HPCLANG_PAGE_SIZE;
        if ((descriptor & UINT32_C(0xfffff000)) != expected) {
            errno = ENOTSUP;
            return -1;
        }
    }
    *physical = start;
    return 0;
}
