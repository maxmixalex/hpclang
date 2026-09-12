#include <stdint.h>

#include <hpclang/bootstrap.h>

void hpclang_mmu_sync(
    volatile uint32_t *l2_workspace,
    volatile uint32_t *l1);
uint32_t hpclang_irq_save_disable(void);
void hpclang_irq_restore(uint32_t cpsr);
int hpclang_enter(uintptr_t entry, uintptr_t stack_top);

int hpclang_bootstrap_run(const struct hpclang_launch_context *launch)
{
    struct hpclang_mmu_saved saved;

    /*
     * The optional section alias borrows a high virtual section only for the
     * lifetime of HPC1. Match the retained HP39g+ read-only NOR dumper's
     * ownership rule: never overwrite a mapping already owned by stock code.
     */
    if (launch->mmu.section_alias_l1_index != 0u &&
        launch->mmu.l1[launch->mmu.section_alias_l1_index] != 0u)
        return -5;

    hpclang_mmu_prepare(&launch->mmu);

    /*
     * The stock OS interrupt handlers run in the stock virtual address space.
     * They must not preempt while HPC1's temporary L1 entries are installed.
     * Save the complete CPSR so the caller's original IRQ/FIQ state and mode
     * are restored only after the stock mappings are back.
     */
    uint32_t saved_cpsr = hpclang_irq_save_disable();

    hpclang_mmu_install(&launch->mmu, &saved);
    hpclang_mmu_sync(launch->mmu.l2_workspace, launch->mmu.l1);

    struct hpclang_boot_info *boot =
        (struct hpclang_boot_info *)(uintptr_t)HPCLANG_BOOT_BASE;

    boot->model = launch->model;
    boot->rw_bytes = launch->mmu.rw_page_count * HPCLANG_PAGE_SIZE;
    boot->saturn_state = launch->saturn_state;
    boot->platform_cookie = launch->platform_cookie;

    uintptr_t entry = HPCLANG_CODE_BASE + launch->entry_offset;
    uintptr_t stack_top =
        HPCLANG_BOOT_BASE +
        launch->mmu.rw_page_count * HPCLANG_PAGE_SIZE;

    int result = hpclang_enter(entry, stack_top);

    hpclang_mmu_restore(&launch->mmu, &saved);
    hpclang_mmu_sync(launch->mmu.l2_workspace, launch->mmu.l1);

    hpclang_irq_restore(saved_cpsr);
    return result;
}
