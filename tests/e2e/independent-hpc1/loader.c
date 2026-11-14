#include <hpclang/bootstrap.h>
#include "stage-image.h"
void hpclang_cache_clean(const void *, unsigned);
static unsigned cpsr(void)
{
    unsigned v;
    __asm__ volatile("mrs %0, cpsr" : "=r"(v));
    return v;
}
void exercise(unsigned *out, unsigned char *arena, const struct hpclang_image_header *image,
              unsigned bytes)
{
    for (unsigned i = 0; i < 134; i++)
        out[i] = 0;
    out[0] = 0x32435048u;
    out[1] = 1;
    if (!hpclang_stage_image_valid(image, bytes) || image->magic[3] != '1' ||
        image->image_size > 4096 || image->rw_static_size > 4096)
        return;
    unsigned start = ((unsigned)arena + 4095u) & ~4095u;
    if (start < 0x08000000u || start + 24576u > 0x08080000u ||
        start + 24576u > (unsigned)arena + 28672u)
        return;
    volatile unsigned *l1 = (void *)0x07f00000u;
    if ((l1[0x80] & 3u) != 2u || (l1[0x80] & 0xfff00000u) != 0x08000000u) {
        out[1] = 2;
        return;
    }
    unsigned *memory = (void *)start;
    for (unsigned i = 0; i < 24576 / 4; i++)
        memory[i] = 0;
    unsigned char *code = (void *)(start + 4096u);
    const unsigned char *from = (const void *)(image + 1);
    for (unsigned i = 0; i < image->image_size; i++)
        code[i] = from[i];
    unsigned code_page = start + 4096u, rw_pages[4];
    for (unsigned i = 0; i < 4; i++)
        rw_pages[i] = start + 8192u + i * 4096u;
    struct hpclang_launch_context launch;
    launch.mmu.l1 = l1;
    launch.mmu.l2_workspace = (void *)start;
    launch.mmu.l2_workspace_phys = start;
    launch.mmu.rw_pages = rw_pages;
    launch.mmu.rw_page_count = 4;
    launch.mmu.code_pages = &code_page;
    launch.mmu.code_page_count = 1;
    launch.mmu.code_window_pages = 0;
    launch.mmu.code_window_page_count = 0;
    launch.model = 6;
    launch.entry_offset = image->entry_offset;
    launch.saturn_state = 0;
    launch.platform_cookie = 0;
    out[3] = l1[0x11];
    out[4] = l1[0x12];
    out[7] = *(volatile unsigned *)0x08000008u;
    out[9] = start;
    out[10] = start + 24576;
    out[14] = cpsr();
    hpclang_cache_clean((const void *)start, 24576);
    out[2] = (unsigned)hpclang_bootstrap_run(&launch);
    out[15] = cpsr();
    out[5] = l1[0x11];
    out[6] = l1[0x12];
    out[8] = *(volatile unsigned *)0x08000008u;
    volatile unsigned *data = (void *)(start + 12288u);
    out[11] = data[0];
    out[12] = data[1];
    out[13] = data[2];
    out[1] = out[2] == 42 && out[3] == out[5] && out[4] == out[6] && out[7] == out[8] &&
                     out[11] == 6 && out[12] == 16384 && out[13] == 0x31435048u &&
                     (out[14] & 255u) == (out[15] & 255u)
                 ? 0
                 : 3;
}
