#ifndef HPCLANG_NATIVE_ALLOCATION_H
#define HPCLANG_NATIVE_ALLOCATION_H

/* Test-only observer for the audited Stock allocator layout. All reads stay
 * inside the model's SRAM interval. A size-shaped word alone is insufficient:
 * require allocated flags, matching footer and reciprocal allocation links.
 * This describes one block, not native heap totals or transfer-time peaks. */
typedef int (*native_read32_t)(void *, uint32_t, uint32_t *);
struct native_allocation
{
    uint32_t pointer, usable_bytes, flags, next, previous;
    uint32_t metadata_bytes, mapped_pages, unmapped_usable_bytes;
};

static int native_header_address(uint32_t address, uint32_t begin, uint32_t end)
{
    return !(address & 3u) && address >= begin && address <= end - 16u;
}

static int native_allocation_read(native_read32_t read32, void *context, uint32_t pointer,
                                  uint32_t begin, uint32_t end, const uint32_t *pages,
                                  uint32_t count, struct native_allocation *output)
{
    struct native_allocation value = {0};
    if (!read32 || !output || !pages || count > 256u || begin >= end || end - begin < 20u ||
        !pointer || pointer < 16u || !native_header_address(pointer - 16u, begin, end) ||
        pointer > end - 4u)
        return -1;
    uint32_t header = pointer - 16u, footer;
    value.pointer = pointer;
    if (read32(context, header, &value.usable_bytes) ||
        read32(context, header + 4u, &value.flags) || read32(context, header + 8u, &value.next) ||
        read32(context, header + 12u, &value.previous) || (value.usable_bytes & 3u) ||
        value.usable_bytes > end - pointer - 4u ||
        ((value.flags & 0xf000u) != 0x1000u && (value.flags & 0xf000u) != 0x2000u) ||
        read32(context, pointer + value.usable_bytes, &footer) || footer != value.usable_bytes)
        return -1;
    const uint32_t links[2] = {value.next, value.previous};
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t backlink;
        if (links[i] &&
            (links[i] == header || !native_header_address(links[i], begin, end) ||
             read32(context, links[i] + (i ? 8u : 12u), &backlink) || backlink != header))
            return -1;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if ((pages[i] & 4095u) || pages[i] < begin || end - begin < 4096u || pages[i] > end - 4096u)
            return -1;
        for (uint32_t j = 0; j < i; ++j)
            if (pages[i] == pages[j])
                return -1;
        if (pages[i] < pointer + value.usable_bytes && pages[i] + 4096u > pointer) {
            if (pages[i] < pointer || pages[i] + 4096u > pointer + value.usable_bytes)
                return -1;
            ++value.mapped_pages;
        }
    }
    value.metadata_bytes = 20u;
    value.unmapped_usable_bytes = value.usable_bytes - value.mapped_pages * 4096u;
    *output = value;
    return 0;
}
#endif
