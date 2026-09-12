#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/bootstrap.h>

int main(void)
{
    uint32_t l1[4096];
    uint32_t workspace[HPCLANG_PAGE_SIZE / sizeof(uint32_t)];

    const uint32_t rw_pages[] = {
        0x08040000u,
        0x08041000u,
        0x0807f000u,
    };
    const uint32_t code_pages[] = {
        0x08080000u,
        0x08081000u,
    };

    memset(l1, 0, sizeof(l1));
    memset(workspace, 0xa5, sizeof(workspace));

    assert(HPCLANG_L1_DATA_INDEX == 0x11u);
    assert(HPCLANG_L1_CODE_INDEX == 0x12u);
    assert(HPCLANG_FLASH_ALIAS_FIRST_L1_INDEX == 0xa0u);
    assert(HPCLANG_FLASH_ALIAS_LAST_L1_INDEX == 0xffu);
    assert(HPCLANG_FLASH_ALIAS_MIN_BASE == 0x0a000000u);
    assert(HPCLANG_FLASH_ALIAS_MAX_BASE == 0x0ff00000u);
    assert(HPCLANG_CODE_L2_FIRST == 0x20u);
    assert(HPCLANG_CODE_PAGE_CAPACITY == 224u);

    /* Selection is live: prefer A0 when free and move on when stock owns it. */
    assert(hpclang_mmu_find_free_section_alias(NULL) == 0u);
    assert(hpclang_mmu_find_free_section_alias(l1) == 0xa0u);
    l1[0xa0] = 0xaaa66c4au;
    assert(hpclang_mmu_find_free_section_alias(l1) == 0xa1u);

    l1[HPCLANG_L1_DATA_INDEX] = 0x12345012u;
    l1[HPCLANG_L1_CODE_INDEX] = 0x6789a012u;

    const struct hpclang_mmu_layout layout = {
        .l1 = l1,
        .l2_workspace = workspace,
        .l2_workspace_phys = 0x0803f000u,
        .rw_pages = rw_pages,
        .rw_page_count = 3,
        .code_pages = code_pages,
        .code_page_count = 2,
        .section_alias_l1_index = 0xa1u,
        .section_alias_phys = 0u,
    };

    hpclang_mmu_prepare(&layout);

    uint32_t *data = workspace;
    uint32_t *code = workspace + HPCLANG_L2_ENTRIES;

    assert(data[0] == 0x08040ffeu);
    assert(data[1] == 0x08041ffeu);
    assert(data[2] == 0x0807fffeu);
    assert(data[3] == 0);

    assert(code[0] == 0);
    assert(code[31] == 0);
    assert(code[32] == 0x08080ffeu);
    assert(code[33] == 0x08081ffeu);
    assert(code[34] == 0);

    struct hpclang_mmu_saved saved;
    assert(hpclang_mmu_install(&layout, &saved) == 0);

    assert(saved.data_l1 == 0x12345012u);
    assert(saved.code_l1 == 0x6789a012u);
    assert(saved.section_alias_l1 == 0u);
    assert(l1[0xa0] == 0xaaa66c4au);
    assert(l1[0xa1] == 0x00000c12u);
    assert(l1[0x11] == 0x0803f011u);
    assert(l1[0x12] == 0x0803f411u);

    hpclang_mmu_restore(&layout, &saved);
    assert(l1[0xa0] == 0xaaa66c4au);
    assert(l1[0xa1] == 0u);
    assert(l1[0x11] == 0x12345012u);
    assert(l1[0x12] == 0x6789a012u);

    /*
     * Simulate the selected slot being claimed after selection but before the
     * IRQ-masked install boundary. Installation must fail without touching any
     * hpclang or stock descriptor.
     */
    l1[0xa1] = 0xdead0012u;
    assert(hpclang_mmu_install(&layout, &saved) != 0);
    assert(l1[0xa1] == 0xdead0012u);
    assert(l1[0x11] == 0x12345012u);
    assert(l1[0x12] == 0x6789a012u);

    const struct hpclang_mmu_layout no_alias = {
        .l1 = l1,
        .l2_workspace = workspace,
        .l2_workspace_phys = 0x0803f000u,
        .rw_pages = rw_pages,
        .rw_page_count = 3,
        .code_pages = code_pages,
        .code_page_count = 2,
        .section_alias_l1_index = 0u,
        .section_alias_phys = 0u,
    };
    l1[0xa0] = 0x13579012u;
    assert(hpclang_mmu_install(&no_alias, &saved) == 0);
    assert(saved.section_alias_l1 == 0u);
    assert(l1[0xa0] == 0x13579012u);
    hpclang_mmu_restore(&no_alias, &saved);
    assert(l1[0xa0] == 0x13579012u);

    /* Exhausting the private high window fails closed. */
    for (uint32_t i = HPCLANG_FLASH_ALIAS_FIRST_L1_INDEX;
         i <= HPCLANG_FLASH_ALIAS_LAST_L1_INDEX;
         ++i)
        l1[i] = 0x00000c12u;
    assert(hpclang_mmu_find_free_section_alias(l1) == 0u);

    return 0;
}
