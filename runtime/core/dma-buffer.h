#ifndef HPCLANG_DMA_BUFFER_H
#define HPCLANG_DMA_BUFFER_H

#include <stddef.h>
#include <stdint.h>

struct hpclang_dma_allocator
{
    void *(*allocate)(void *context, size_t bytes);
    void (*release)(void *context, void *allocation);
    /* Validate every page of the span. ENOTSUP means fragmented physical
     * backing; other errors stop acquisition without a larger allocation. */
    int (*translate)(void *context, const void *pixels, uint32_t bytes, uint32_t *physical);
    void *context;
};

/* Zero-initialize before first use. The buffer owns its backing allocation;
 * pixels may point inside it. Stop DMA before release. Cache publication and
 * device-specific address constraints belong to the consuming driver. */
struct hpclang_dma_buffer
{
    void *allocation;
    uint8_t *pixels;
    uint32_t physical, bytes;
    void (*release)(void *context, void *allocation);
    void *context;
};

/* Acquire white/zero-filled storage on demand. First try only alignment slack.
 * Fragmentation permits one larger allocation with at most a page of slack,
 * allowing a page-boundary start. Never retain failed candidates or reserve
 * memory at process startup. Alignment must be a power of two <= page size.
 * On failure, buffer is unchanged and no allocation remains owned. */
int hpclang_dma_buffer_acquire(const struct hpclang_dma_allocator *allocator, uint32_t bytes,
                               uint32_t alignment, struct hpclang_dma_buffer *buffer);
void hpclang_dma_buffer_release(struct hpclang_dma_buffer *buffer);

#endif
