#include <hpclang/bootstrap.h>

/*
 * ARMv4 short-descriptor format used by the Samsung S3C2410-class MMU:
 *
 *   L1 coarse-table descriptor: physical table base | 0x11
 *   L1 1 MiB section descriptor: physical section base | 0xc12
 *   L2 4 KiB small-page descriptor: physical page base | 0xffe
 *
 * 0xc12 matches the read-only physical-flash alias already proven by the
 * retained HP39g+ NOR dumper. 0xffe is the same full-access/cacheable
 * small-page descriptor used by the historical launchers. hpclang deliberately
 * uses coarse tables for code and writable memory; the optional section alias
 * is bootstrap-private and exists only while HPC1 is running.
 */
#define COARSE_L1_FLAGS  0x0011u
#define SECTION_L1_FLAGS 0x0c12u
#define SMALL_L2_FLAGS   0x0ffeu

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
    saved->section_alias_l1 = 0u;

    layout->l1[HPCLANG_L1_DATA_INDEX] =
        layout->l2_workspace_phys | COARSE_L1_FLAGS;
    layout->l1[HPCLANG_L1_CODE_INDEX] =
        (layout->l2_workspace_phys + HPCLANG_L2_BYTES) | COARSE_L1_FLAGS;

    if (layout->section_alias_l1_index != 0u) {
        saved->section_alias_l1 =
            layout->l1[layout->section_alias_l1_index];
        layout->l1[layout->section_alias_l1_index] =
            (layout->section_alias_phys & 0xfff00000u) | SECTION_L1_FLAGS;
    }
}

void hpclang_mmu_restore(
    const struct hpclang_mmu_layout *layout,
    const struct hpclang_mmu_saved *saved)
{
    if (layout->section_alias_l1_index != 0u)
        layout->l1[layout->section_alias_l1_index] =
            saved->section_alias_l1;

    layout->l1[HPCLANG_L1_DATA_INDEX] = saved->data_l1;
    layout->l1[HPCLANG_L1_CODE_INDEX] = saved->code_l1;
}
