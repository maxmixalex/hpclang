/* Borrow free main-RPL pages only while Stock execution and IRQ/FIQ are paused.
   No native service is called while pages are borrowed. This is not a lease
   suitable for resuming Stock or enabling native object mutation. */
typedef unsigned int u32;

static u32 word5(const u32 *map, u32 address)
{
    u32 result = 0;
    for (u32 i = 0; i < 5; ++i) {
        u32 nibble = address + i;
        u32 base = map[nibble >> 12];
        if (base < 0x08000000u || base > 0x0807f800u)
            return 0;
        u32 byte = *(volatile unsigned char *)(base + ((nibble & 4095u) >> 1));
        result |= ((byte >> ((nibble & 1u) * 4)) & 15u) << (i * 4);
    }
    return result;
}

void exercise(u32 *out, volatile u32 *control, const u32 *map)
{
    for (u32 i = 0; i < 134; ++i)
        out[i] = 0;
    out[0] = 0x52504f4cu;
    out[1] = 1;
    __asm__ volatile("mrs %0, cpsr" : "=r"(out[2]));
    control[1] = out[2];
    out[3] = (u32)map;
    if ((out[2] & 0xffu) != 0xd0u || (u32)map < 0x08000000u || (u32)map > 0x0807fc00u)
        return;
    u32 lower = word5(map, 0x806f3u), upper = word5(map, 0x806f8u);
    out[4] = lower;
    out[5] = upper;
    if (lower < 0x80000u || upper > 0x100000u || lower >= upper || upper - lower < 8704u)
        return;
    /* Leave 4 KiB below and 256 bytes above the pool, even in this paused probe. */
    u32 first = (lower + 8192u + 8191u) & ~8191u;
    u32 end = (upper - 512u) & ~8191u;
    out[6] = first;
    out[7] = end;
    u32 count = 0;
    for (u32 address = first; address < end; address += 8192u) {
        u32 page = map[address >> 12];
        if (count >= 64u || page < 0x08001000u || page > 0x0807f000u || (page & 4095u) ||
            map[(address >> 12) + 1] != page + 2048u)
            return;
        for (u32 i = 0; i < count; ++i)
            if (out[12 + i] == page)
                return;
        out[12 + count++] = page;
    }
    out[8] = count;
    if (!count)
        return;
    /* All pages are validated before any write. Preserve their previous bytes. */
    for (u32 i = 0; i < count; ++i) {
        volatile u32 *page = (void *)out[12 + i];
        u32 low = page[0], high = page[1023];
        page[0] = 0x13570000u + i;
        page[1023] = 0x24680000u + i;
        if (page[0] != 0x13570000u + i || page[1023] != 0x24680000u + i)
            out[9]++;
        page[0] = low;
        page[1023] = high;
        if (page[0] != low || page[1023] != high)
            out[10]++;
    }
    out[11] = word5(map, 0x806f3u) == lower && word5(map, 0x806f8u) == upper;
    out[1] = 0;
}
