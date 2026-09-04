#ifndef HPCLANG_BOOTSTRAP_H
#define HPCLANG_BOOTSTRAP_H

#include <stdint.h>
#include <hpclang/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HPCLANG_L1_DATA_INDEX       (HPCLANG_BOOT_BASE >> 20)
#define HPCLANG_L1_CODE_INDEX       (HPCLANG_CODE_BASE >> 20)

#define HPCLANG_L2_ENTRIES          256u
#define HPCLANG_L2_BYTES            (HPCLANG_L2_ENTRIES * sizeof(uint32_t))
#define HPCLANG_L2_WORKSPACE_BYTES  HPCLANG_PAGE_SIZE

#define HPCLANG_DATA_L2_FIRST       0u
#define HPCLANG_CODE_L2_FIRST       ((HPCLANG_CODE_BASE & 0x000fffffu) >> 12)

#define HPCLANG_RW_PAGE_CAPACITY    HPCLANG_L2_ENTRIES
#define HPCLANG_CODE_PAGE_CAPACITY  (HPCLANG_L2_ENTRIES - HPCLANG_CODE_L2_FIRST)

/*
 * Platform shims prepare physical pages. The common bootstrap only assigns
 * those pages to the fixed HPC1 virtual layout.
 *
 * rw_pages[0] backs HPCLANG_BOOT_BASE. The remaining pages back data, BSS,
 * heap and stack.
 *
 * code_pages[0] backs HPCLANG_CODE_BASE and contains the first page of the
 * HPC1 payload (the 16-byte HPC1 container header is not mapped).
 *
 * l2_workspace points to one writable 4 KiB physical page. Its first 1 KiB is
 * the data coarse table; its second 1 KiB is the code coarse table.
 */
struct hpclang_mmu_layout {
    volatile uint32_t *l1;
    volatile uint32_t *l2_workspace;
    uint32_t l2_workspace_phys;

    const uint32_t *rw_pages;
    uint32_t rw_page_count;

    const uint32_t *code_pages;
    uint32_t code_page_count;
};

struct hpclang_mmu_saved {
    uint32_t data_l1;
    uint32_t code_l1;
};

void hpclang_mmu_prepare(const struct hpclang_mmu_layout *layout);
void hpclang_mmu_install(
    const struct hpclang_mmu_layout *layout,
    struct hpclang_mmu_saved *saved);
void hpclang_mmu_restore(
    const struct hpclang_mmu_layout *layout,
    const struct hpclang_mmu_saved *saved);

#ifdef __cplusplus
}
#endif

#endif
