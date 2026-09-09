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
    };

    hpclang_mmu_prepare(&layout);

    uint32_t *data = workspace;
    uint32_t *code = workspace + HPCLANG_L2_ENTRIES;

    assert(HPCLANG_L1_DATA_INDEX == 0x11u);
    assert(HPCLANG_L1_CODE_INDEX == 0x12u);
    assert(HPCLANG_CODE_L2_FIRST == 0x20u);
    assert(HPCLANG_CODE_PAGE_CAPACITY == 224u);

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
    hpclang_mmu_install(&layout, &saved);

    assert(saved.data_l1 == 0x12345012u);
    assert(saved.code_l1 == 0x6789a012u);
    assert(l1[0x11] == 0x0803f011u);
    assert(l1[0x12] == 0x0803f411u);

    hpclang_mmu_restore(&layout, &saved);
    assert(l1[0x11] == 0x12345012u);
    assert(l1[0x12] == 0x6789a012u);

    return 0;
}
