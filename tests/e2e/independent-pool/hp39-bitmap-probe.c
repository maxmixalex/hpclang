#ifdef HPCLANG_PROBE_NOTEBOOK_EDITOR
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
void hpclang_cache_clean(const void *, unsigned);
#endif
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
#ifndef NOTEBOOK_BYTES
#define NOTEBOOK_BYTES 45456
#define NOTEBOOK_FNV1A 0x8b0d5f8fu
#endif
    enum
    {
        notebook_bytes = NOTEBOOK_BYTES,
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
#ifdef HPCLANG_PROBE_NOTEBOOK_EDITOR
    /* Verify the staged image before switching to application mappings. */
    if (hash != NOTEBOOK_FNV1A)
        return;
    volatile u32 *l1 = (void *)0x07f00000u;
    if ((l1[0x80] & 0xfff00003u) != 0x08000002u)
        return;
    u32 fonts[2];
    fonts[0] = NOTEBOOK_MODEL == HPCLANG_MODEL_HP39GPLUS ? 0x6e000 : 0x7e000;
    fonts[1] = fonts[0] + 4096;
    struct hpclang_launch_context launch;
    launch.mmu.l1 = l1;
    launch.mmu.l2_workspace = (void *)out[24];
    launch.mmu.l2_workspace_phys = out[24];
    launch.mmu.code_pages = out + 12;
    launch.mmu.code_page_count = 12;
    launch.mmu.rw_pages = out + 25;
    launch.mmu.rw_page_count = count - 13;
    launch.mmu.code_window_pages = fonts;
    launch.mmu.code_window_page_count = 2;
    launch.model = NOTEBOOK_MODEL;
    launch.entry_offset = 8;
    launch.saturn_state = (u32)map;
    launch.platform_cookie = 0;
#ifdef HPCLANG_PROBE_APLET_WRITEBACK
    if (top > 0xffff1u)
        return;
    struct hpclang_hp39_aplet_list_context list = {HPCLANG_HP39_APLET_LIST_MAGIC,
                                                   read5(map, top + 10u)};
    if (list.list_address < 0x80000u || list.list_address > 0xffff0u ||
        read5(map, list.list_address) != 0x2a74u)
        return;
    launch.platform_cookie = (u32)&list | HPCLANG_PLATFORM_COOKIE_HP39_TAG;
#endif
    for (u32 j = 0; j < count; j++) {
        if (j >= 12) {
            volatile u32 *page = (void *)out[12 + j];
            for (u32 k = 0; k < HPCLANG_PAGE_SIZE / sizeof(*page); k++)
                page[k] = 0;
        }
        hpclang_cache_clean((void *)out[12 + j], HPCLANG_PAGE_SIZE);
    }
    out[46] = l1[HPCLANG_L1_DATA_INDEX];
    out[47] = l1[HPCLANG_L1_CODE_INDEX];
    out[48] = hpclang_bootstrap_run(&launch);
    out[49] = l1[HPCLANG_L1_DATA_INDEX];
    out[50] = l1[HPCLANG_L1_CODE_INDEX];
    if (out[46] != out[49] || out[47] != out[50] || out[48])
        return;
#endif
#endif
    out[1] = 0;
}
