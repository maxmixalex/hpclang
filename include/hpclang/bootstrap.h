#ifndef HPCLANG_BOOTSTRAP_H
#define HPCLANG_BOOTSTRAP_H

#include <stdint.h>
#include <hpclang/abi.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HPCLANG_L1_DATA_INDEX (HPCLANG_BOOT_BASE >> 20)
#define HPCLANG_L1_CODE_INDEX (HPCLANG_CODE_BASE >> 20)

#define HPCLANG_L2_ENTRIES 256u
#define HPCLANG_L2_BYTES (HPCLANG_L2_ENTRIES * sizeof(uint32_t))
#define HPCLANG_L2_WORKSPACE_BYTES HPCLANG_PAGE_SIZE
/* Two tables occupy the first half; the remaining bytes belong to the launcher. */
#define HPCLANG_L2_TABLES_BYTES (2u * HPCLANG_L2_BYTES)

#define HPCLANG_DATA_L2_FIRST 0u
#define HPCLANG_CODE_L2_FIRST ((HPCLANG_CODE_BASE & 0x000fffffu) >> 12)

/*
 * The code coarse table owns the complete 0x01200000..0x012fffff section,
 * while the public application image starts at HPCLANG_CODE_BASE=0x01220000.
 * Entries before HPCLANG_CODE_L2_FIRST are therefore bootstrap-private and may
 * expose small platform assets without borrowing stock L1 slots.
 */
#define HPCLANG_CODE_WINDOW_BASE (HPCLANG_L1_CODE_INDEX << 20)
#define HPCLANG_CODE_WINDOW_PAGE_CAPACITY (HPCLANG_CODE_L2_FIRST - 1u)
/* Read-only alias of the active L2 workspace for runtime DMA validation. */
#define HPCLANG_PAGE_TABLE_BASE (HPCLANG_CODE_BASE - HPCLANG_PAGE_SIZE)

#define HPCLANG_RW_PAGE_CAPACITY HPCLANG_L2_ENTRIES
#define HPCLANG_CODE_PAGE_CAPACITY (HPCLANG_L2_ENTRIES - HPCLANG_CODE_L2_FIRST)

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
     * code_window_pages, when non-NULL, are mapped non-cacheable/non-buffered at
     * HPCLANG_CODE_WINDOW_BASE before the application code range. The window is a
     * private launcher/runtime facility; applications must not depend on it.
     * Its page count must not exceed HPCLANG_CODE_WINDOW_PAGE_CAPACITY.
     *
     * l2_workspace points to one writable 4 KiB physical page. Its first 1 KiB is
     * the data coarse table; its second 1 KiB is the code coarse table.
     */
    struct hpclang_mmu_layout
    {
        volatile uint32_t *l1;
        volatile uint32_t *l2_workspace;
        uint32_t l2_workspace_phys;

        const uint32_t *rw_pages;
        uint32_t rw_page_count;

        const uint32_t *code_pages;
        uint32_t code_page_count;

        const uint32_t *code_window_pages;
        uint32_t code_window_page_count;
    };

    struct hpclang_mmu_saved
    {
        uint32_t data_l1;
        uint32_t code_l1;
    };

/*
 * ARMToolbox-private RPL allocator fence.
 *
 * The Stock launcher has already removed a pool of physical main-RAM pages for
 * the native ARM invocation. RPL object/stack mutation must not grow back into
 * those pages merely because Stock AVMEM still describes the surrounding RPL
 * arena. The HP48/49/50 stage computes conservative Saturn-nibble fences from
 * the live Read[] map and publishes this short-lived structure through the
 * opaque platform_cookie. Applications must never inspect it.
 */
#define HPCLANG_RPL_BOUNDS_MAGIC UINT32_C(0x324c5052) /* "RPL2" */
#define HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG ((uintptr_t)1u)

    struct hpclang_rpl_memory_bounds
    {
        uint32_t magic;
        uint32_t temp_limit;            /* exclusive upper bound for RSKTOP/TEMPOB growth */
        uint32_t user_limit;            /* lower native-page fence for DSKTOP growth */
        uint32_t stack_reserve_nibbles; /* launcher-private cells below user D1 */
        uint32_t **native_roots;        /* private ARM-held Saturn addresses */
        uint32_t native_root_count;
    };

/*
 * ARMToolbox-private HP39 platform context prefix.
 *
 * The context stays in the temporary stage window throughout the HPC1 call.
 * Runtime code must
 * check the cookie tag and magic before decoding this prefix.
 * Applications must not infer the
 * platform_cookie representation.
 */
#define HPCLANG_HP39_PLATFORM_MAGIC UINT32_C(0x39335048) /* "HP39" */
#define HPCLANG_PLATFORM_COOKIE_HP39_TAG ((uintptr_t)2u)

    struct hpclang_hp39_platform_context
    {
        uint32_t magic;
        uintptr_t font_window_base;
        uintptr_t portal_entry;
    };

    /*
     * Family shims fill this after allocating/staging physical pages. No legacy
     * launcher memory flags or calculator-specific allocator state crosses this
     * boundary.
     */
    struct hpclang_launch_context
    {
        struct hpclang_mmu_layout mmu;

        uint32_t model;
        uint32_t entry_offset;
        uintptr_t saturn_state;
        /* Opaque model-specific token copied verbatim into hpclang_boot_info. */
        uintptr_t platform_cookie;
    };

    void hpclang_mmu_prepare(const struct hpclang_mmu_layout *layout);
    void hpclang_mmu_install(const struct hpclang_mmu_layout *layout,
                             struct hpclang_mmu_saved *saved);
    void hpclang_mmu_restore(const struct hpclang_mmu_layout *layout,
                             const struct hpclang_mmu_saved *saved);

    int hpclang_bootstrap_run(const struct hpclang_launch_context *launch);

#ifdef __cplusplus
}
#endif

#endif
