typedef unsigned int u32;
static u32 read5(const u32 *map, u32 a)
{
    u32 v = 0;
    for (unsigned i = 0; i < 5; i++) {
        u32 n = a + i, phys = map[n >> 12];
        if (phys < 0x08000000 || phys > 0x0807f800)
            return 0;
        v |= ((*(volatile unsigned char *)(phys + (n % 4096) / 2) >> (4 * (n % 2))) & 15u)
             << (4 * i);
    }
    return v;
}
void exercise(u32 *out, volatile u32 *ctl, const u32 *map)
{
    __asm__ volatile("mrs %0,cpsr" : "=r"(ctl[1]));
    out[0] = 0x47524f42;
    out[1] = 1;
    out[2] = (u32)map;
    if ((u32)map < 0x08000000 || (u32)map > 0x0807fc00)
        return;
    u32 top = read5(map, 0x806f8);
    out[3] = top;
    if (top < 0x80000 || top > 0xffffa)
        return;
    u32 obj = read5(map, top);
    out[4] = obj;
    if (obj < 0x80000 || obj > 0xffff0)
        return;
    u32 type = read5(map, obj), span = read5(map, obj + 5);
    out[5] = type;
    out[6] = span;
    if (type != 0x2b1e || span < 229376 || span > 0x100000 - obj - 5)
        return;
    u32 first = (obj + 20 + 8191) & ~8191u, end = (obj + 5 + span) & ~8191u, count = 0;
    for (u32 a = first; a < end; a += 8192) {
        u32 page = map[a >> 12];
        if (count >= 32 || page < 0x08001000 || page > 0x0807f000 || (page & 4095) ||
            map[(a >> 12) + 1] != page + 2048)
            return;
        for (u32 j = 0; j < count; j++)
            if (out[12 + j] == page)
                return;
        out[12 + count++] = page;
    }
    out[7] = count;
    out[8] = first;
    out[9] = end;
#ifdef HPCLANG_PROBE_NOTEBOOK_STAGE
    enum
    {
        notebook_bytes = 45456,
        code_capacity = 12 * 4096
    };
    if (count < 27 || top > 0xffff6)
        return;
    /* The source string sits below the bitmap on the native data stack. */
    u32 source = read5(map, top + 5);
    if (source < 0x80000 || source > 0xfffe0 || read5(map, source) != 0x2a2c)
        return;
    u32 source_span = read5(map, source + 5);
    if (source_span != 5 + 2 * (16 + notebook_bytes) || source_span > 0x100000 - source - 5)
        return;
    u32 payload = source + 10;
    for (u32 j = payload >> 12; j <= (source + 4 + source_span) >> 12; j++)
        if (map[j] < 0x08000000 || map[j] > 0x0807f800)
            return;
    if ((read5(map, payload) & 65535) != 0x5048 || (read5(map, payload + 4) & 65535) != 0x3143)
        return;
    payload += 32;          /* Skip the 16-byte HPC1 header in nibble address space. */
    u32 hash = 2166136261u; /* FNV-1a over bytes read back from the physical pages. */
    for (u32 i = 0; i < code_capacity; i++) {
        u32 value = 0;
        if (i < notebook_bytes) {
            u32 nibble = payload + 2 * i;
            u32 low = *(volatile unsigned char *)(map[nibble >> 12] + ((nibble & 4095) >> 1));
            value = low;
            if (nibble & 1) {
                u32 next_nibble = nibble + 1;
                u32 high = *(volatile unsigned char *)(map[next_nibble >> 12] +
                                                       ((next_nibble & 4095) >> 1));
                value = (low >> 4) | ((high & 15) << 4);
            }
        }
        *(volatile unsigned char *)(out[12 + (i >> 12)] + (i & 4095)) = value;
    }
    for (u32 i = 0; i < code_capacity; i++) {
        u32 value = *(volatile unsigned char *)(out[12 + (i >> 12)] + (i & 4095));
        if (i < notebook_bytes)
            hash = (hash ^ value) * 16777619u;
        else if (value)
            return;
    }
    out[44] = hash;
    out[45] = notebook_bytes;
#endif
    out[1] = 0;
}
