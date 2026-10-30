#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include "rpl-execution-map.h"
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return 0;
}
int main(void)
{
    uint32_t before[256], after[256];
    for (unsigned i = 0; i < 256; ++i)
        before[i] = 0x08000000u + (255u - i) * 0x800u;
    memcpy(after, before, sizeof before);
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 0, 0x100000) == 0);
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 0xfffff, 1) == 0);
    for (unsigned i = 0; i < 256; ++i) {
        after[i] = 0;
        assert(hpclang_rpl_execution_mapping_preserved(before, after, 0, 0x100000) < 0);
        assert(hpclang_rpl_execution_mapping_preserved(before, after, i * 4096, 4096) < 0);
        after[i] = before[i];
    }
    after[1] = before[0];
    after[0] = before[1];
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 4095, 2) < 0);
    memcpy(after, before, sizeof before);
    after[1] = 1;
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 0, 4096) == 0);
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 4095, 2) < 0);
    before[1] = 1;
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 4096, 1) < 0);
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 0, 0) < 0);
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 0xfffff, 2) < 0);
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 0x100000, 1) < 0);
    assert(hpclang_rpl_execution_mapping_preserved(before, after, 0, UINT32_MAX) < 0);
    assert(hpclang_rpl_execution_mapping_preserved(0, after, 0, 1) < 0);
    assert(hpclang_rpl_execution_mapping_preserved(before, 0, 0, 1) < 0);
    return 0;
}
