#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include "rpl-execution-map.h"
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return 0;
}
struct fixture
{
    uint8_t code[4];
    unsigned reads, observations, fail_read, fail_observation;
    int error;
};
static int read_code(void *context, uint32_t address, uint8_t *value)
{
    struct fixture *f = context;
    assert(address >= 0xffe && address < 0x1002);
    ++f->reads;
    if (f->reads == f->fail_read)
        return 0;
    *value = f->code[address - 0xffe];
    return 1;
}
static int observe_code(void *context, uint32_t address, uint8_t value)
{
    struct fixture *f = context;
    assert(address == 0xffe + f->observations && value == f->code[f->observations]);
    ++f->observations;
    return f->observations == f->fail_observation ? f->error : 0;
}
static void instructions(uint32_t before[256], uint32_t after[256])
{
    struct fixture f = {.code = {0, 14, 0, 0}};
    const struct hpclang_rpl_reader reader = {read_code, &f};
    uint32_t length = 99;
    assert(hpclang_rpl_execution_instruction_preserved(&reader, before, after, 0xffe, 4, &length,
                                                       observe_code, &f) == 0);
    assert(length == 4 && f.reads == 4 && f.observations == 4);
    after[1] = before[0];
    f.reads = f.observations = 0;
    length = 99;
    assert(hpclang_rpl_execution_instruction_preserved(&reader, before, after, 0xffe, 4, &length,
                                                       observe_code,
                                                       &f) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(length == 99 && f.reads == 2 && f.observations == 2);
    after[1] = before[1];
    for (unsigned i = 1; i <= 4; ++i) {
        f.reads = f.observations = 0;
        f.fail_read = i;
        assert(hpclang_rpl_execution_instruction_preserved(&reader, before, after, 0xffe, 4,
                                                           &length, observe_code,
                                                           &f) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(length == 99 && f.reads == i && f.observations == i - 1);
        f.fail_read = 0;
        f.fail_observation = i;
        for (unsigned positive = 0; positive < 2; ++positive) {
            f.reads = f.observations = 0;
            f.error = positive ? 1 : HPCLANG_RPL_SPAN_BOUNDS;
            assert(hpclang_rpl_execution_instruction_preserved(&reader, before, after, 0xffe, 4,
                                                               &length, observe_code, &f) ==
                   (positive ? HPCLANG_RPL_SPAN_MALFORMED : HPCLANG_RPL_SPAN_BOUNDS));
            assert(length == 99 && f.reads == i && f.observations == i);
        }
        f.fail_observation = 0;
    }
    f.reads = f.observations = 0;
    assert(hpclang_rpl_execution_instruction_preserved(&reader, before, after, 0xffe, 3, &length,
                                                       observe_code,
                                                       &f) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(length == 99 && f.reads == 2);
    f.reads = f.observations = 0;
    f.code[2] = 16;
    assert(hpclang_rpl_execution_instruction_preserved(&reader, before, after, 0xffe, 4, &length,
                                                       observe_code,
                                                       &f) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(length == 99 && f.observations == 2);
    assert(hpclang_rpl_execution_instruction_preserved(&reader, before, after, 0xffe, 4, &length, 0,
                                                       &f) == HPCLANG_RPL_SPAN_UNAVAILABLE);
}
int main(void)
{
    uint32_t before[256], after[256];
    for (unsigned i = 0; i < 256; ++i)
        before[i] = 0x08000000u + (255u - i) * 0x800u;
    memcpy(after, before, sizeof before);
    instructions(before, after);
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
