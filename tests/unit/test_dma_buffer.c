#include <assert.h>
#include <errno.h>
#include <string.h>
#include "dma-buffer.h"
#include "dma-span.h"

struct memory
{
    _Alignas(4096) uint8_t bytes[32768];
    uint32_t pages[8];
    size_t offset, reserved;
    unsigned allocations, releases;
    int owned, exhausted, map_error;
};

static void *allocate(void *context, size_t bytes)
{
    struct memory *memory = context;
    assert(!memory->owned);
    ++memory->allocations;
    if (memory->exhausted)
        return NULL;
    assert(bytes <= sizeof(memory->bytes) - memory->offset);
    memory->owned = 1;
    memory->reserved = bytes;
    return memory->bytes + memory->offset;
}

static void release(void *context, void *allocation)
{
    struct memory *memory = context;
    assert(memory->owned && allocation == memory->bytes + memory->offset);
    memory->owned = 0;
    ++memory->releases;
}

static int translate(void *context, const void *pixels, uint32_t bytes, uint32_t *physical)
{
    struct memory *memory = context;
    size_t offset = (const uint8_t *)pixels - memory->bytes;
    assert(memory->owned && offset >= memory->offset);
    assert(offset - memory->offset + bytes <= memory->reserved);
    if (memory->map_error) {
        errno = memory->map_error;
        return -1;
    }
    return hpclang_dma_span(memory->pages, 8, (uint32_t)offset, bytes, 4, physical);
}

int main(void)
{
    static struct memory memory;
    memset(memory.bytes, 0xcc, sizeof(memory.bytes));
    for (unsigned page = 0; page < 8; ++page)
        memory.pages[page] = 0x08000ffe + page * 4096;
    struct hpclang_dma_allocator allocator = {allocate, release, translate, &memory};
    struct hpclang_dma_buffer buffer = {0};
    memory.offset = 7;
    assert(hpclang_dma_buffer_acquire(&allocator, 6800, 4, &buffer) == 0);
    assert(memory.allocations == 1 && memory.reserved == 6803 && memory.releases == 0);
    assert(buffer.pixels == memory.bytes + 8 && buffer.physical == 0x08000008);
    for (unsigned i = 0; i < 6800; ++i)
        assert(buffer.pixels[i] == 0);
    assert(memory.bytes[7] == 0xcc && memory.bytes[6808] == 0xcc);
    assert(hpclang_dma_buffer_acquire(&allocator, 6800, 4, &buffer) == -1 && errno == EBUSY);
    assert(memory.allocations == 1);
    hpclang_dma_buffer_release(&buffer);
    hpclang_dma_buffer_release(&buffer);
    assert(memory.releases == 1 && !buffer.allocation && !buffer.pixels);

    /* The first page is detached; a larger owned block permits skipping it. */
    memory.offset = 4092;
    memory.pages[0] = 0x08007ffe;
    assert(hpclang_dma_buffer_acquire(&allocator, 6000, 4, &buffer) == 0);
    assert(memory.allocations == 3 && memory.releases == 2);
    assert(buffer.pixels == memory.bytes + 4096 && buffer.physical == 0x08001000);
    hpclang_dma_buffer_release(&buffer);
    assert(memory.releases == 3);

    memory.exhausted = 1;
    assert(hpclang_dma_buffer_acquire(&allocator, 6000, 4, &buffer) == -1 && errno == ENOMEM);
    assert(memory.allocations == 4 && memory.releases == 3 && !memory.owned);
    memory.exhausted = 0;
    memory.map_error = EFAULT;
    assert(hpclang_dma_buffer_acquire(&allocator, 6000, 4, &buffer) == -1 && errno == EFAULT);
    assert(memory.allocations == 5 && memory.releases == 4 && !memory.owned);
    memory.map_error = 0;
    for (unsigned page = 0; page < 8; ++page)
        memory.pages[page] = 0x08000ffe + page * 8192;
    assert(hpclang_dma_buffer_acquire(&allocator, 6000, 4, &buffer) == -1 && errno == ENOTSUP);
    assert(memory.allocations == 7 && memory.releases == 6 && !memory.owned);
    assert(!buffer.allocation && !buffer.pixels);
    assert(hpclang_dma_buffer_acquire(&allocator, UINT32_MAX, 4, &buffer) == -1 && errno == EINVAL);
    assert(hpclang_dma_buffer_acquire(&allocator, 4, 3, &buffer) == -1 && errno == EINVAL);
    assert(memory.allocations == 7);
    return 0;
}
