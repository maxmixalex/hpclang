#include <stdint.h>
#include <stddef.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>

#define STAGE_WINDOW_BASE   0x09000000u
#define STAGE_CODE_OFFSET   0x00000400u
#define STAGE_STACK_PAGES   2u

#define ARMSYS_PAGE_COUNT   0x914u
#define ARMSYS_L2_PHYS      0x920u

void hpclang_cache_clean(const void *start, uint32_t size);

static uint32_t load32(const void *base, uint32_t offset)
{
    return *(const uint32_t *)((const uint8_t *)base + offset);
}

static int hpc1_header_ok(const struct hpclang_image_header *header)
{
    return header->magic[0] == 'H' &&
           header->magic[1] == 'P' &&
           header->magic[2] == 'C' &&
           header->magic[3] == '1';
}

/*
 * Fixed-address stage1 entered from the tiny position-independent ARMToolbox
 * trampoline.
 *
 * The stock launcher has already created a page pool and a coarse L2 table for
 * the temporary 0x09000000 window. We use that pool only as startup storage;
 * HPC1 itself sees the normal hpclang v1 fixed layout.
 */
int hpclang_armtoolbox_stage1(
    void *saturn_state,
    const struct hpclang_image_header *hpc1,
    uint32_t stage1_size,
    uint32_t model)
{
    if (!hpc1_header_ok(hpc1))
        return -1;

    uint32_t total_pages = load32(saturn_state, ARMSYS_PAGE_COUNT);
    uint32_t old_l2_phys = load32(saturn_state, ARMSYS_L2_PHYS);
    volatile uint32_t *old_l2 =
        (volatile uint32_t *)(uintptr_t)STAGE_WINDOW_BASE;

    uint32_t stage_pages =
        (STAGE_CODE_OFFSET + stage1_size + HPCLANG_PAGE_SIZE - 1u) /
        HPCLANG_PAGE_SIZE;
    uint32_t code_count =
        (hpc1->image_size + HPCLANG_PAGE_SIZE - 1u) / HPCLANG_PAGE_SIZE;

    uint32_t workspace_page = stage_pages;
    uint32_t code_first = workspace_page + 1u;
    uint32_t rw_first = code_first + code_count;

    if (code_count > HPCLANG_CODE_PAGE_CAPACITY)
        return -2;
    if (rw_first + STAGE_STACK_PAGES >= total_pages)
        return -3;

    uint32_t rw_count = total_pages - STAGE_STACK_PAGES - rw_first;
    uint32_t rw_needed =
        1u +
        (hpc1->rw_static_size + HPCLANG_PAGE_SIZE - 1u) / HPCLANG_PAGE_SIZE +
        HPCLANG_STACK_MIN / HPCLANG_PAGE_SIZE;

    if (rw_count < rw_needed || rw_count > HPCLANG_RW_PAGE_CAPACITY)
        return -4;

    uint32_t code_pages[HPCLANG_CODE_PAGE_CAPACITY];
    uint32_t rw_pages[HPCLANG_RW_PAGE_CAPACITY];

    for (uint32_t i = 0; i < code_count; ++i)
        code_pages[i] = old_l2[code_first + i] & 0xfffff000u;

    for (uint32_t i = 0; i < rw_count; ++i)
        rw_pages[i] = old_l2[rw_first + i] & 0xfffff000u;

    const uint8_t *src = (const uint8_t *)(hpc1 + 1);
    uint32_t left = hpc1->image_size;

    for (uint32_t i = 0; i < code_count; ++i) {
        volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)(
            STAGE_WINDOW_BASE + (code_first + i) * HPCLANG_PAGE_SIZE);
        uint32_t n = left < HPCLANG_PAGE_SIZE ? left : HPCLANG_PAGE_SIZE;

        for (uint32_t j = 0; j < n; ++j)
            dst[j] = src[j];
        for (uint32_t j = n; j < HPCLANG_PAGE_SIZE; ++j)
            dst[j] = 0;

        src += n;
        left -= n;
    }

    hpclang_cache_clean(
        (const void *)(uintptr_t)(
            STAGE_WINDOW_BASE + code_first * HPCLANG_PAGE_SIZE),
        code_count * HPCLANG_PAGE_SIZE);

    struct hpclang_launch_context launch;

    launch.mmu.l1 = (volatile uint32_t *)(uintptr_t)0x07f00000u;
    launch.mmu.l2_workspace = (volatile uint32_t *)(uintptr_t)(
        STAGE_WINDOW_BASE + workspace_page * HPCLANG_PAGE_SIZE);
    launch.mmu.l2_workspace_phys =
        old_l2[workspace_page] & 0xfffff000u;
    launch.mmu.rw_pages = rw_pages;
    launch.mmu.rw_page_count = rw_count;
    launch.mmu.code_pages = code_pages;
    launch.mmu.code_page_count = code_count;

    launch.model = model;
    launch.entry_offset = hpc1->entry_offset;
    launch.saturn_state = (uintptr_t)saturn_state;
    launch.platform_cookie = old_l2_phys;

    return hpclang_bootstrap_run(&launch);
}
