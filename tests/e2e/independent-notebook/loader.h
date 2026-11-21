typedef unsigned int u32;
u32 hpclang_irq_save_disable(void);
void hpclang_irq_restore(u32);
u32 stock_call(u32, u32, u32);
__asm__(".arm\n.global stock_call\nstock_call:\n"
        "stmdb sp!, {r4-r12,lr}\nadr lr, 1f\nswi 0\n"
        "1: ldmia sp!, {r4-r12,lr}\nbx lr\n");

static u32 desc(u32 p)
{
    return ((volatile u32 *)0x07f00000)[p >> 20];
}
static u32 mark(u32 p, unsigned j)
{
    return 0x935c612bu ^ p ^ (0x1234567u * j);
}
static int owner(u32 p, u32 bytes)
{
    if (p < 0x08000010 || p > 0x08080000 - bytes)
        return 0;
    volatile u32 *q = (volatile u32 *)p;
    u32 size = q[-4], flags = q[-3] & 0xf000;
    if (size < bytes || size > 0x08080000 - p - 4 || (size & 3) ||
        (flags != 0x1000 && flags != 0x2000) || q[size / 4] != size)
        return 0;
    for (unsigned i = 0; i < 2; i++) {
        u32 l = q[(int)i - 2];
        if (l &&
            ((l & 3) || l < 0x08000000 || l > 0x0807fff0 || ((volatile u32 *)l)[3 - i] != p - 16))
            return 0;
    }
    return 1;
}

#include "../../../platform/armtoolbox/stage-stack.c"
__attribute__((used, noinline)) static int rd(void *ctx, u32 addr, unsigned width, u32 *v)
{
    const u32 *m = ctx;
    *v = 0;
    if (addr > 0x100000 - width || width > 5)
        return 0;
    for (unsigned j = 0; j < width; j++) {
        u32 a = addr + j, b = m[a >> 12];
        if (b & 2047u || (b >= 0x200000 && (b < 0x08000000 || b > 0x0807f800)))
            return 0;
        *v |= ((((volatile unsigned char *)b)[(a & 4095) >> 1] >> (4 * (a & 1))) & 15) << (4 * j);
    }
    return 1;
}
void hpclang_mmu_sync(volatile u32 *, volatile u32 *);
void hpclang_cache_clean(const void *, u32);
static u32 split(volatile u32 *rec, unsigned second, const u32 *map, u32 *frame)
{
    u32 pg[21], cnt = 0;
    for (unsigned i = 0; i < rec[1]; i++)
        for (unsigned k = 0; k < rec[5 + i * 2]; k++) {
            if (cnt == 21)
                for (;;) {
                }
            pg[cnt++] = ((rec[4 + i * 2] + 4095) & ~4095u) + k * 4096;
        }
    if (cnt != 21)
        for (;;) {
        }
    unsigned irq = hpclang_irq_save_disable();
    volatile u32 *l1 = (void *)0x07f00000, *l2 = (void *)pg[20];
    u32 oldrw = l1[17], oldcode = l1[18];
    unsigned hash = 2166136261u;
    for (unsigned j = 0; j < IMAGE_BYTES; j++)
        hash = (hash ^ ((volatile unsigned char *)0x08021000u)[j]) * 16777619u;
    if (hash != IMAGE_HASH)
        for (;;) {
        }
    for (unsigned i = 0; i < 512; i++)
        l2[i] = 0;
    for (unsigned i = 0; i < 20; i++) {
        u32 ph = i < 14 ? 0x08021000 + i * 4096 : pg[i - 14];
        if (i >= 14) {
            volatile u32 *dst = (void *)ph, *src = (void *)(0x08021000 + i * 4096);
            for (unsigned j = 0; j < 1024; j++)
                dst[j] = src[j];
            hpclang_cache_clean((void *)ph, 4096);
        }
        l2[256 + 32 + i] = ph | 0xaa2; /* AP=2 throughout, user read-only */
    }
    for (unsigned i = 0; i < 8; i++)
        l2[i] = pg[6 + i] | 0xff2;
    l2[256 + 31] = pg[20] | 0xaa2;
    l1[17] = pg[20] | 0x11;
    l1[18] = (pg[20] + 1024) | 0x11;
    hpclang_mmu_sync(l2, l1);
    hash = 2166136261u;
    for (unsigned j = 0; j < IMAGE_BYTES; j++)
        hash = (hash ^ ((volatile unsigned char *)0x01220000u)[j]) * 16777619u;
    for (unsigned i = 0; i < 8; i++) {
        volatile u32 *dst = (void *)(0x01100000 + i * 4096), *phys = (void *)pg[6 + i];
        dst[0] = 0xc5170000u + i + second;
        if (phys[0] != dst[0])
            for (;;) {
            }
    }

    if (!second) {
        volatile u32 *boot = (void *)0x01100000;
        boot[0] = NOTEBOOK_MODEL;
        boot[1] = 32768;
        boot[2] = (u32)map;
        u32 lo, hi;
        if (!rd((void *)map, 0x806f3, 5, &lo) || !rd((void *)map, 0x806f8, 5, &hi) ||
            lo < 0x80000 || hi <= lo)
            for (;;) {
            }
        /* The fixture requires the full Saturn main arena maps
           to upper SRAM; KOS blocks and ERAM records are entirely below it. */
        for (unsigned i = 128; i < 256; i++)
            if (map[i] != 0x08040000 + (i - 128) * 2048 || map[257 + i] != map[i])
                for (;;) {
                }
        for (unsigned i = 0; i < 21; i++)
            if (pg[i] >= 0x08040000)
                for (;;) {
                }
        struct hpclang_stage_stack_io io = {(void *)map, 0, 0};
        __asm__ volatile(
            "adr %0,1f\nldr r12,[%0]\nadd %0,%0,r12\nb 2f\n.align 2\n1: .word rd-1b\n2:"
            : "=&r"(io.read)::"r12");
        u32 *roots[12], masks[12];
        roots[0] = frame;
        masks[0] = ~0u;
        u32 *cpu = (u32 *)map;
        unsigned n = 1 + hpclang_stage_execution_roots(
                             &io, *frame, cpu + 0x978 / 4, cpu[0x998 / 4],
                             cpu[0x970 / 4] <= 1 ? (int)cpu[0x970 / 4] : -1, cpu + 0x90c / 4,
                             cpu + 0x91c / 4, roots + 1, masks + 1);
        /* No hidden RPL stack slot; deny Port 1 edits for this owner. */
        u32 bounds[] = {0x354c5052, hi, lo, 0, (u32)roots, n, 0, (u32)masks, (u32)frame};
        boot[3] = (u32)bounds | 1;

        extern int nb_enter(u32, u32);
        rec[40] = nb_enter(0x01220000 + ENTRY_OFFSET, 0x01108000);
        rec[41] = 0x4e424558;
    } else if (rec[41] != 0x4e424558 || rec[40])
        for (;;) {
        }
    l1[17] = oldrw;
    l1[18] = oldcode;
    hpclang_mmu_sync(l2, l1);
    if (hash != IMAGE_HASH || l1[17] != oldrw || l1[18] != oldcode)
        for (;;) {
        }
    /* Restore all previously marked allocation contents for the independent
       GC gate. Writable data here is a mapping test, not Notebook state. */
    for (unsigned i = 0; i < 21; i++)
        for (unsigned j = 0; j < 1024; j++)
            ((volatile u32 *)pg[i])[j] = mark(pg[i], j);
    hpclang_irq_restore(irq);
    return hash;
}

__asm__(".arm\n.global nb_enter\nnb_enter:\n"
        "stmdb sp!,{r4-r11,lr}\nmov r4,sp\nmov sp,r1\nmov lr,pc\nbx r0\nmov sp,r4\nldmia "
        "sp!,{r4-r11,lr}\nbx lr\n");
