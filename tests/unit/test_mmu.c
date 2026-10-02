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
    const uint32_t private_pages[] = {
        0x00042000u,
        0x00043000u,
    };

    memset(l1, 0, sizeof(l1));
    memset(workspace, 0xa5, sizeof(workspace));

    assert(HPCLANG_L1_DATA_INDEX == 0x11u);
    assert(HPCLANG_L1_CODE_INDEX == 0x12u);
    assert(HPCLANG_CODE_WINDOW_BASE == 0x01200000u);
    assert(HPCLANG_CODE_L2_FIRST == 0x20u);
    assert(HPCLANG_CODE_WINDOW_PAGE_CAPACITY == 32u);
    assert(HPCLANG_CODE_PAGE_CAPACITY == 224u);
    assert(HPCLANG_CODE_WINDOW_BASE + HPCLANG_CODE_WINDOW_PAGE_CAPACITY * HPCLANG_PAGE_SIZE ==
           HPCLANG_CODE_BASE);

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
        .code_window_pages = private_pages,
        .code_window_page_count = 2,
    };

    hpclang_mmu_prepare(&layout);

    uint32_t *data = workspace;
    uint32_t *code = workspace + HPCLANG_L2_ENTRIES;

    assert(data[0] == 0x08040ffeu);
    assert(data[1] == 0x08041ffeu);
    assert(data[2] == 0x0807fffeu);
    assert(data[3] == 0);

    /* Private physical NOR pages are uncached and precede application code. */
    assert(code[0] == 0x00042ff2u);
    assert(code[1] == 0x00043ff2u);
    assert(code[2] == 0u);
    assert(code[31] == 0u);
    assert(code[32] == 0x08080ffeu);
    assert(code[33] == 0x08081ffeu);
    assert(code[34] == 0u);

    struct hpclang_mmu_saved saved;
    hpclang_mmu_install(&layout, &saved);

    assert(saved.data_l1 == 0x12345012u);
    assert(saved.code_l1 == 0x6789a012u);
    assert(l1[0x11] == 0x0803f011u);
    assert(l1[0x12] == 0x0803f411u);

    hpclang_mmu_restore(&layout, &saved);
    assert(l1[0x11] == 0x12345012u);
    assert(l1[0x12] == 0x6789a012u);

    /* No private window leaves the complete pre-code prefix unmapped. */
    const struct hpclang_mmu_layout no_window = {
        .l1 = l1,
        .l2_workspace = workspace,
        .l2_workspace_phys = 0x0803f000u,
        .rw_pages = rw_pages,
        .rw_page_count = 3,
        .code_pages = code_pages,
        .code_page_count = 2,
        .code_window_pages = NULL,
        .code_window_page_count = 0,
    };
    hpclang_mmu_prepare(&no_window);
    code = workspace + HPCLANG_L2_ENTRIES;
    assert(code[0] == 0u);
    assert(code[31] == 0u);
    assert(code[32] == 0x08080ffeu);

    /* An invalid oversized private window fails closed instead of overlapping
       the application code entries. */
    struct hpclang_mmu_layout oversized = no_window;
    oversized.code_window_pages = private_pages;
    oversized.code_window_page_count = HPCLANG_CODE_WINDOW_PAGE_CAPACITY + 1u;
    hpclang_mmu_prepare(&oversized);
    code = workspace + HPCLANG_L2_ENTRIES;
    assert(code[0] == 0u);
    assert(code[31] == 0u);
    assert(code[32] == 0x08080ffeu);

    /* Stock return code may occupy the unused half of the workspace. Neither
     * table preparation nor install/restore may overwrite that live code. */
    for (size_t i = HPCLANG_L2_TABLES_BYTES / sizeof(uint32_t);
         i < sizeof(workspace) / sizeof(workspace[0]); ++i)
        assert(workspace[i] == UINT32_C(0xa5a5a5a5));

    return 0;
}
