#include <hpclang/bootstrap.h>

/*
 * ARMv4 short-descriptor format used by the Samsung S3C2410-class MMU:
 *
 *   L1 coarse-table descriptor: physical table base | 0x11
 *   L2 4 KiB small-page descriptor: physical page base | 0xffe
 *
 * 0xffe is the same full-access/cacheable small-page descriptor used by the
 * historical launchers. hpclang deliberately uses coarse tables for both code
 * and writable memory; HPC1 has no need for the historical 1 KiB fine-page
 * code layout.
 */
#define COARSE_L1_FLAGS 0x11u
#define SMALL_L2_FLAGS  0x0ffeu

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
