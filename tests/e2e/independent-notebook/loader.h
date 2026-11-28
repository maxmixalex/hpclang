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
enum
{
    RW_PAGES = NOTEBOOK_RW_BYTES / 4096,
    POOL_PAGES = RW_PAGES + 1
};
_Static_assert(NOTEBOOK_RW_BYTES % 4096 == 0, "Notebook RW mapping requires whole pages");
/* Stock stores Port 1 records consecutively in the 128 KiB ERAM bank. */
static u32 eram_word(u32 at, unsigned width)
{
    const volatile unsigned char *ram = (const void *)0x08020000;
    u32 value = 0;
    for (unsigned i = 0; i < width; i++, at++)
        value |= ((ram[at / 2] >> (4 * (at & 1))) & 15u) << (4 * i);
    return value;
}

static u32 notebook_code(void)
{
    for (u32 at = 0; at <= 0x40000 - 10;) {
        u32 kind = eram_word(at, 5);
        if (kind != 0x2b62 && kind != 0x2b40)
            return 0;
        u32 end = at + 5 + eram_word(at + 5, 5);
        if (end > 0x40000 || end < at + 29)
            return 0;
        if (kind == 0x2b62 && end >= at + 44 && eram_word(at + 10, 2) == 5 &&
            eram_word(at + 12, 6) == 0x4c424e && eram_word(at + 18, 4) == 0x4553) {
            u32 payload = at + 34, page = (payload + 8191) & ~8191u;
            u32 pages = (IMAGE_BYTES + 4095) / 4096;
            if (eram_word(at + 22, 2) != 5 || eram_word(at + 24, 5) != 0x2a2c ||
                at + 29 + eram_word(at + 29, 5) != end - 10 || eram_word(end - 10, 5) != 0x2911 ||
                page + pages * 8192 > end - 10)
                return 0;
            u32 code = 0x08020000 + page / 2;
            u32 hash = 2166136261u;
            for (unsigned i = 0; i < IMAGE_BYTES; i++)
                hash = (hash ^ ((const volatile unsigned char *)code)[i]) * 16777619u;
            return hash == IMAGE_HASH ? code : 0;
        }
        at = end;
    }
    return 0;
}

static u32 pool_digest(const u32 *pg)
{
    u32 digest = 2166136261u;
    for (unsigned i = 0; i < POOL_PAGES; i++)
        for (unsigned j = 0; j < 1024; j++)
            digest = (digest ^ ((volatile u32 *)pg[i])[j]) * 16777619u;
    return digest;
}

static u32 split(volatile u32 *rec, unsigned second, const u32 *map, u32 *frame, u32 code)
{
    u32 pg[POOL_PAGES], cnt = 0;
    for (unsigned i = 0; i < rec[1]; i++)
        for (unsigned k = 0; k < rec[5 + i * 2]; k++) {
            if (cnt == POOL_PAGES)
                for (;;) {
                }
            pg[cnt++] = ((rec[4 + i * 2] + 4095) & ~4095u) + k * 4096;
        }
    if (cnt != POOL_PAGES)
        for (;;) {
        }
    unsigned irq = hpclang_irq_save_disable();
    volatile u32 *l1 = (void *)0x07f00000, *l2 = (void *)pg[RW_PAGES];
    u32 oldrw = l1[17], oldcode = l1[18];
    if (NOTEBOOK_MAPPED_GC && second) {
        if (oldrw != (pg[RW_PAGES] | 0x11) || oldcode != ((pg[RW_PAGES] + 1024) | 0x11))
            for (;;) {
            }
        if (pool_digest(pg) != rec[30])
            for (;;) {
            }
        oldrw = rec[28];
        oldcode = rec[29];
    } else {
        for (unsigned i = 0; i < 512; i++)
            l2[i] = 0;
        /* The entire staged image is page aligned in the leased Port 1 record. */
        for (unsigned i = 0; i < (IMAGE_BYTES + 4095) / 4096; i++)
            l2[256 + 32 + i] = (code + i * 4096) | 0xaa2;
        for (unsigned i = 0; i < RW_PAGES; i++)
            l2[i] = pg[i] | 0xff2;
        l2[256 + 31] = pg[RW_PAGES] | 0xaa2;
        l1[17] = pg[RW_PAGES] | 0x11;
        l1[18] = (pg[RW_PAGES] + 1024) | 0x11;
        hpclang_mmu_sync(l2, l1);
    }
    unsigned hash = 2166136261u;
    for (unsigned j = 0; j < IMAGE_BYTES; j++)
        hash = (hash ^ ((volatile unsigned char *)0x01220000u)[j]) * 16777619u;
    if (!(NOTEBOOK_MAPPED_GC && second))
        for (unsigned i = 0; i < RW_PAGES; i++) {
            volatile u32 *dst = (void *)(0x01100000 + i * 4096), *phys = (void *)pg[i];
            dst[0] = 0xc5170000u + i + second;
            if (phys[0] != dst[0])
                for (;;) {
                }
        }

    if (second == NOTEBOOK_ENTER_AFTER_GC) {
        volatile u32 *boot = (void *)0x01100000;
        boot[0] = NOTEBOOK_MODEL;
        boot[1] = NOTEBOOK_RW_BYTES;
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
        for (unsigned i = 0; i < POOL_PAGES; i++)
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
        rec[40] = nb_enter(0x01220000 + ENTRY_OFFSET, 0x01100000 + NOTEBOOK_RW_BYTES);
        rec[41] = 0x4e424558;
    } else if (second && rec[41] != 0x4e424558)
        for (;;) {
        }
    if (NOTEBOOK_MAPPED_GC) {
        /* Words 4..27 hold up to twelve KOS pointer/count pairs. */
        if (!second) {
            rec[28] = oldrw;
            rec[29] = oldcode;
            rec[30] = pool_digest(pg);
            hpclang_irq_restore(irq);
            return hash;
        }
    }
    l1[17] = oldrw;
    l1[18] = oldcode;
    hpclang_mmu_sync(l2, l1);
    if (hash != IMAGE_HASH || l1[17] != oldrw || l1[18] != oldcode)
        for (;;) {
        }
    /* Restore all previously marked allocation contents for the independent
       GC gate. Writable data here is a mapping test, not Notebook state. */
    for (unsigned i = 0; i < POOL_PAGES; i++)
        for (unsigned j = 0; j < 1024; j++)
            ((volatile u32 *)pg[i])[j] = mark(pg[i], j);
    hpclang_irq_restore(irq);
    return hash;
}

__asm__(".arm\n.global nb_enter\nnb_enter:\n"
        "stmdb sp!,{r4-r11,lr}\nmov r4,sp\nmov sp,r1\nmov lr,pc\nbx r0\nmov sp,r4\nldmia "
        "sp!,{r4-r11,lr}\nbx lr\n");
