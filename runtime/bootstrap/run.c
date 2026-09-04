#include <stdint.h>

#include <hpclang/bootstrap.h>

void hpclang_mmu_sync(
    volatile uint32_t *l2_workspace,
    volatile uint32_t *l1);
int hpclang_enter(uintptr_t entry, uintptr_t stack_top);

int hpclang_bootstrap_run(const struct hpclang_launch_context *launch)
{
    struct hpclang_mmu_saved saved;

    hpclang_mmu_prepare(&launch->mmu);
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

    return result;
}
