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
 * Private bootstrap-only section-alias window used by HP39gs/40gs runtime
 * services that must address fixed physical NOR assets rather than the stock
 * Saturn bank view. These addresses are not part of the application ABI.
 *
 * The retained read-only HP39g+ NOR dumper proved L1[0xa0] / VA 0x0a000000 as
 * a usable high section when that descriptor is empty. Production bootstrap
 * code does not assume that slot remains free: it scans this contiguous high
 * window and borrows only a currently empty descriptor, then re-checks
 * ownership immediately before installing the mapping.
 */
#define HPCLANG_L1_SECTION_SHIFT             20u
#define HPCLANG_FLASH_ALIAS_FIRST_L1_INDEX   0x0a0u
#define HPCLANG_FLASH_ALIAS_LAST_L1_INDEX    0x0ffu
#define HPCLANG_FLASH_ALIAS_MIN_BASE \
    (HPCLANG_FLASH_ALIAS_FIRST_L1_INDEX << HPCLANG_L1_SECTION_SHIFT)
#define HPCLANG_FLASH_ALIAS_MAX_BASE \
    (HPCLANG_FLASH_ALIAS_LAST_L1_INDEX << HPCLANG_L1_SECTION_SHIFT)

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
    /* Opaque model-specific token copied verbatim into hpclang_boot_info. */
    uintptr_t platform_cookie;
};

uint32_t hpclang_mmu_find_free_section_alias(const volatile uint32_t *l1);
void hpclang_mmu_prepare(const struct hpclang_mmu_layout *layout);
int hpclang_mmu_install(
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
