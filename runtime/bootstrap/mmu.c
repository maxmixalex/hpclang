#include <hpclang/bootstrap.h>

/*
 * ARMv4 short-descriptor format used by the Samsung S3C2410-class MMU:
 *
 *   L1 coarse-table descriptor: physical table base | 0x11
 *   L2 4 KiB small-page RAM descriptor: physical page base | 0xffe
 *   L2 4 KiB small-page uncached descriptor: physical page base | 0xff2
 *
 * Both small-page forms use full-access AP bits. Normal HPC1 RAM/code keeps
 * the historical cacheable/buffered 0xffe descriptor. Bootstrap-private ROM
 * windows use 0xff2 so physical NOR is never made cacheable merely to expose a
 * fixed asset such as the HP39 native font tables.
 */
#define COARSE_L1_FLAGS          0x0011u
#define SMALL_L2_FLAGS           0x0ffeu
#define SMALL_L2_UNCACHED_FLAGS  0x0ff2u

static volatile uint32_t *data_l2(const struct hpclang_mmu_layout *layout)
{
    return layout->l2_workspace;
}

static volatile uint32_t *code_l2(const struct hpclang_mmu_layout *layout)
{
    return layout->l2_workspace + HPCLANG_L2_ENTRIES;
}

void hpclang_mmu_prepare(const struct hpclang_mmu_layout *layout)
{
    volatile uint32_t *data = data_l2(layout);
    volatile uint32_t *code = code_l2(layout);

    for (uint32_t i = 0; i < HPCLANG_L2_ENTRIES; ++i) {
        data[i] = 0;
        code[i] = 0;
    }

    for (uint32_t i = 0; i < layout->rw_page_count; ++i)
        data[HPCLANG_DATA_L2_FIRST + i] = layout->rw_pages[i] | SMALL_L2_FLAGS;

    if (layout->code_window_pages != 0 &&
        layout->code_window_page_count <= HPCLANG_CODE_WINDOW_PAGE_CAPACITY) {
        for (uint32_t i = 0; i < layout->code_window_page_count; ++i) {
            code[i] =
                (layout->code_window_pages[i] & 0xfffff000u) |
                SMALL_L2_UNCACHED_FLAGS;
        }
    }

    for (uint32_t i = 0; i < layout->code_page_count; ++i)
        code[HPCLANG_CODE_L2_FIRST + i] = layout->code_pages[i] | SMALL_L2_FLAGS;
}

void hpclang_mmu_install(
    const struct hpclang_mmu_layout *layout,
    struct hpclang_mmu_saved *saved)
{
    saved->data_l1 = layout->l1[HPCLANG_L1_DATA_INDEX];
    saved->code_l1 = layout->l1[HPCLANG_L1_CODE_INDEX];

    layout->l1[HPCLANG_L1_DATA_INDEX] =
        layout->l2_workspace_phys | COARSE_L1_FLAGS;
    layout->l1[HPCLANG_L1_CODE_INDEX] =
        (layout->l2_workspace_phys + HPCLANG_L2_BYTES) | COARSE_L1_FLAGS;
}

void hpclang_mmu_restore(
    const struct hpclang_mmu_layout *layout,
    const struct hpclang_mmu_saved *saved)
{
    layout->l1[HPCLANG_L1_DATA_INDEX] = saved->data_l1;
    layout->l1[HPCLANG_L1_CODE_INDEX] = saved->code_l1;
}
