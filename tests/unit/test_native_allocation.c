#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "native-allocation.h"

#define BASE 0x08000000u
static uint32_t memory[16384];
static unsigned reads;
static int unreadable;
static int read32(void *context, uint32_t address, uint32_t *value)
{
    assert(context == memory);
    assert(!(address & 3u) && address >= BASE && address < BASE + sizeof memory);
    ++reads;
    if (unreadable)
        return -1;
    *value = memory[(address - BASE) / 4u];
    return 0;
}

int main(void)
{
    const uint32_t pointer = BASE + 0x1010u, end = BASE + sizeof memory;
    uint32_t pages[2] = {BASE + 0x2000u, BASE + 0x3000u};
    memory[0x1000 / 4] = 0x2ff0u;
    memory[0x1004 / 4] = 0x2102u;
    memory[0x1008 / 4] = BASE + 0x5000u;
    memory[0x100c / 4] = BASE + 0x7000u;
    memory[0x4000 / 4] = 0x2ff0u;
    memory[0x500c / 4] = BASE + 0x1000u;
    memory[0x7008 / 4] = BASE + 0x1000u;
    struct native_allocation result;
    assert(!native_allocation_read(read32, memory, pointer, BASE, end, pages, 2, &result));
    assert(result.pointer == pointer && result.usable_bytes == 0x2ff0u);
    assert(result.metadata_bytes == 20 && result.mapped_pages == 2);
    assert(result.unmapped_usable_bytes == 0xff0u);
    struct native_allocation retained = result;
    const unsigned offsets[] = {0x1000, 0x1004, 0x1008, 0x100c, 0x4000, 0x500c, 0x7008};
    const uint32_t bad[] = {0xfffffffcu, 0x102u, 0xfffffffcu, BASE + 0x1000u, 4u, 0u, 0u};
    for (unsigned i = 0; i < sizeof offsets / sizeof *offsets; ++i) {
        uint32_t saved = memory[offsets[i] / 4];
        memory[offsets[i] / 4] = bad[i];
        assert(native_allocation_read(read32, memory, pointer, BASE, end, pages, 2, &result) == -1);
        assert(!memcmp(&result, &retained, sizeof result));
        memory[offsets[i] / 4] = saved;
    }
    pages[0] = BASE + 0x1000u; /* mapped page overlaps allocator header */
    assert(native_allocation_read(read32, memory, pointer, BASE, end, pages, 2, &result) == -1);
    pages[0] = pages[1]; /* duplicate page must not double-count */
    assert(native_allocation_read(read32, memory, pointer, BASE, end, pages, 2, &result) == -1);
    pages[0] = end;
    assert(native_allocation_read(read32, memory, pointer, BASE, end, pages, 2, &result) == -1);
    pages[0] = BASE + 0x8000u;
    pages[1] = BASE + 0x9000u;
    assert(!native_allocation_read(read32, memory, pointer, BASE, end, pages, 2, &result));
    assert(!result.mapped_pages && result.unmapped_usable_bytes == 0x2ff0u);
    unsigned before = reads;
    assert(native_allocation_read(read32, memory, 0, BASE, end, pages, 2, &result) == -1);
    assert(native_allocation_read(read32, memory, BASE + 4, BASE, end, pages, 2, &result) == -1);
    assert(native_allocation_read(read32, memory, end, BASE, end, pages, 2, &result) == -1);
    assert(native_allocation_read(read32, memory, pointer, end, BASE, pages, 2, &result) == -1);
    assert(native_allocation_read(read32, memory, pointer, BASE, end, pages, 257, &result) == -1);
    assert(reads == before);
    unreadable = 1;
    assert(native_allocation_read(read32, memory, pointer, BASE, end, pages, 2, &result) == -1);
    return 0;
}
