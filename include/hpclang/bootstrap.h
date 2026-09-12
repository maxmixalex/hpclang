#ifndef HPCLANG_BOOTSTRAP_H
#define HPCLANG_BOOTSTRAP_H

#include <stdint.h>
#include <hpclang/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HPCLANG_L1_DATA_INDEX       (HPCLANG_BOOT_BASE >> 20)
#define HPCLANG_L1_CODE_INDEX       (HPCLANG_CODE_BASE >> 20)

/*
 * Private bootstrap-only section alias used by HP39/40 runtime services that
 * must address a fixed physical NOR asset rather than the stock Saturn bank
 * view. This address is not part of the application ABI.
 *
 * 0x0a000000 is the same high section proved safe by the retained read-only
 * HP39g+ NOR dumper. The active stock L1 descriptor is saved and restored with
 * the ordinary HPC1 mappings, so no alias survives application return.
 */
#define HPCLANG_FLASH_ALIAS_BASE    0x0a000000u
#define HPCLANG_L1_FLASH_ALIAS_INDEX (HPCLANG_FLASH_ALIAS_BASE >> 20)

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
 *
 * section_alias_l1_index == 0 disables the optional section alias. When it is
 * nonzero, section_alias_phys is the 1 MiB-aligned physical section mapped at
 * that L1 index for the lifetime of HPC1. This is a private launcher/runtime
 * facility, not an application-visible memory contract.
 */
struct hpclang_mmu_layout {
    volatile uint32_t *l1;
    volatile uint32_t *l2_workspace;
    uint32_t l2_workspace_phys;

    const uint32_t *rw_pages;
    uint32_t rw_page_count;

    const uint32_t *code_pages;
    uint32_t code_page_count;

    uint32_t section_alias_l1_index;
    uint32_t section_alias_phys;
};

struct hpclang_mmu_saved {
    uint32_t data_l1;
    uint32_t code_l1;
    uint32_t section_alias_l1;
};

/*
 * Family shims fill this after allocating/staging physical pages. No legacy
 * launcher memory flags or calculator-specific allocator state crosses this
 * boundary.
 */
struct hpclang_launch_context {
    struct hpclang_mmu_layout mmu;

    uint32_t model;
    uint32_t entry_offset;
    uintptr_t saturn_state;
    uintptr_t platform_cookie;
};

void hpclang_mmu_prepare(const struct hpclang_mmu_layout *layout);
void hpclang_mmu_install(
    const struct hpclang_mmu_layout *layout,
    struct hpclang_mmu_saved *saved);
void hpclang_mmu_restore(
    const struct hpclang_mmu_layout *layout,
    const struct hpclang_mmu_saved *saved);

int hpclang_bootstrap_run(const struct hpclang_launch_context *launch);

#ifdef __cplusplus
}
#endif

#endif
