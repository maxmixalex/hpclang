#include <stdint.h>

#include <hpclang/bootstrap.h>

void hpclang_mmu_sync(
    volatile uint32_t *l2_workspace,
    volatile uint32_t *l1,
    uint32_t section_alias_l1_index);
uint32_t hpclang_irq_save_disable(void);
void hpclang_irq_restore(uint32_t cpsr);
int hpclang_enter(uintptr_t entry, uintptr_t stack_top);

int hpclang_bootstrap_run(const struct hpclang_launch_context *launch)
{
    struct hpclang_mmu_saved saved;

    hpclang_mmu_prepare(&launch->mmu);

    /*
     * The stock OS interrupt handlers run in the stock virtual address space.
     * They must not preempt while HPC1's temporary L1 entries are installed.
     * Save the complete CPSR so the caller's original IRQ/FIQ state and mode
     * are restored only after the stock mappings are back.
     */
    uint32_t saved_cpsr = hpclang_irq_save_disable();

    /*
     * A launcher may have selected an empty section-alias slot before this
     * critical section. hpclang_mmu_install() re-checks that descriptor at the
     * actual write boundary; on a collision it changes no L1 entry.
     */
    if (hpclang_mmu_install(&launch->mmu, &saved) != 0) {
        hpclang_irq_restore(saved_cpsr);
        return -5;
    }
    hpclang_mmu_sync(
        launch->mmu.l2_workspace,
        launch->mmu.l1,
        launch->mmu.section_alias_l1_index);

    struct hpclang_boot_info *boot =
        (struct hpclang_boot_info *)(uintptr_t)HPCLANG_BOOT_BASE;

    boot->model = launch->model;
    boot->rw_bytes = launch->mmu.rw_page_count * HPCLANG_PAGE_SIZE;
    boot->saturn_state = launch->saturn_state;
    boot->platform_cookie = launch->platform_cookie;

    /*
     * TEMPORARY E2E DIAGNOSTIC: the rest of the boot page is private bootstrap
     * storage. The host already reports the first five words at +0x20, so pack
     * the complete pre-entry alias probe into those words and avoid rebuilding
     * the diagnostic x50ng frontend:
     *
     *   [0] "ALI3"
     *   [1] selected L1 descriptor after hpclang_mmu_sync()
     *   [2] stage: "PRB0" -> "PRBA" -> "DONE"
     *   [3] byte read at mini-font table offset 0x4335a
     *   [4] byte read at the mini-font 'A' advance offset 0x434c0
     *
     * If either volatile byte load aborts, [2] identifies exactly which load
     * faulted. Remove this block after the HP39 physical-NOR path is closed.
     */
    if (launch->mmu.section_alias_l1_index != 0u) {
        volatile uint32_t *diag =
            (volatile uint32_t *)(uintptr_t)(HPCLANG_BOOT_BASE + 0x20u);
        uintptr_t alias = launch->platform_cookie;
        const volatile uint8_t *nor0 =
            (const volatile uint8_t *)(alias + UINT32_C(0x0004335a));
        const volatile uint8_t *nor_a =
            (const volatile uint8_t *)(alias + UINT32_C(0x000434c0));

        diag[0] = UINT32_C(0x414c4933); /* "ALI3" */
        diag[1] = launch->mmu.l1[launch->mmu.section_alias_l1_index];
        diag[2] = UINT32_C(0x50524230); /* "PRB0": first read about to start */
        diag[3] = 0u;
        diag[4] = 0u;
        diag[3] = (uint32_t)*nor0;
        diag[2] = UINT32_C(0x50524241); /* "PRBA": 'A' read about to start */
        diag[4] = (uint32_t)*nor_a;
        diag[2] = UINT32_C(0x444f4e45); /* "DONE" */
    }

    uintptr_t entry = HPCLANG_CODE_BASE + launch->entry_offset;
    uintptr_t stack_top =
        HPCLANG_BOOT_BASE +
        launch->mmu.rw_page_count * HPCLANG_PAGE_SIZE;

    int result = hpclang_enter(entry, stack_top);

    hpclang_mmu_restore(&launch->mmu, &saved);
    hpclang_mmu_sync(
        launch->mmu.l2_workspace,
        launch->mmu.l1,
        launch->mmu.section_alias_l1_index);

    hpclang_irq_restore(saved_cpsr);
    return result;
}
