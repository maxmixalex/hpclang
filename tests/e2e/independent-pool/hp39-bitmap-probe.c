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
    out[1] = 0;
}
