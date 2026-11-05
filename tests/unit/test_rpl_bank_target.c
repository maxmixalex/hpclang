#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include "rpl-bank-target.h"
#include "rpl-stock-flash-data.h"

static uint8_t ram[0x80000];
static uint8_t flash[0x800];
static unsigned flash_reads;
static struct hpclang_rpl_bank_plan selected;
static struct hpclang_rpl_bank_target_workspace workspace;
static struct hpclang_rpl_bank_target output, sentinel;
static unsigned calls, stop, payloads;
static int callback_error = -79;

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return 0;
}
uint16_t hpclang_flash_read16(void *context, uint32_t address)
{
    (void)context;
    ++flash_reads;
    if (address == 0x1ffffe)
        return 0xabcd;
    assert(!(address & 1) && address >= 0x140000 && address < 0x140800);
    return flash[address - 0x140000] | ((uint16_t)flash[address - 0x140000 + 1] << 8);
}
/* Isolate the composition contract; original-ROM planner behavior is covered
 * by the maintained two-model bank-switch regression. */
int hpclang_rpl_bank_plan_prepare(const struct hpclang_rpl_bank_sources *sources, uint32_t entry,
                                  struct hpclang_rpl_bank_workspace *scratch,
                                  struct hpclang_rpl_bank_plan *plan,
                                  hpclang_rpl_graph_dependency_fn dependency, void *context)
{
    (void)scratch;
    assert(entry == 0x12345);
    int result = dependency(context, sources->saturn_mapping, entry, 1);
    if (result)
        return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
    *plan = selected;
    return 0;
}
static int observe(void *context, uint32_t mapping, uint32_t address, uint8_t value)
{
    (void)context;
    assert(value <= 15);
    assert((mapping == 1 && address == 0x12345) ||
           (mapping == 4 && address >= 0x40000 && address < 0x4000a));
    payloads += mapping == 4;
    return ++calls == stop ? callback_error : 0;
}
int main(void)
{
    uint32_t section = 0x08000c1a, sections[] = {0xc1a, 0x100c1a};
    struct hpclang_rpl_stock_view view;
    assert(!hpclang_rpl_stock_view_capture(&view, ram, 0x08003344, &section, sections));
    const uint32_t outside[] = {0, 0x27ffff, 0x400000, UINT32_MAX};
    for (unsigned i = 0; i < sizeof outside / sizeof *outside; ++i) {
        uint8_t value = 99;
        assert(!hpclang_rpl_stock_flash_data_read(&view, outside[i], &value));
        assert(value == 99 && !flash_reads);
    }
    for (unsigned i = 0; i < 4; ++i) {
        uint8_t value = 99;
        assert(hpclang_rpl_stock_flash_data_read(&view, 0x3ffffc + i, &value));
        assert(value == ((0xabcdu >> (4 * i)) & 15));
    }
    const struct hpclang_rpl_reader saturn = {hpclang_rpl_stock_view_saturn, &view};
    const struct hpclang_rpl_reader native = {hpclang_rpl_stock_view_native, &view};
    const struct hpclang_rpl_reader rom = {hpclang_rpl_stock_view_rom, &view};
    struct hpclang_rpl_bank_sources sources = {&saturn, &native, &rom, 0,       1,
                                               2,       3,       0,    0x40000, 0x860cc};
    struct hpclang_rpl_reference edge = {HPCLANG_RPL_REFERENCE_ACPTR, 0x80005, 0x40000, 0, 0x12345};
    selected.geometry.bytes = 0x20000;
    selected.effects.map.pages[64] = 0x08020000;
    /* DOBINT: five-nibble prologue and five-nibble body. */
    ram[0x20000] = 0x11;
    ram[0x20001] = 0x29;
    memset(&sentinel, 0xa5, sizeof sentinel);
    assert(!hpclang_rpl_bank_target_prepare(&view, &sources, &edge, 4, &workspace, &output, observe,
                                            0));
    assert(output.node.mapping == 4 && output.node.object.address == 0x40000 &&
           output.node.object.limit == 10 && !output.node.object.body_prologue);
    assert(payloads && !memcmp(&output.plan, &selected, sizeof selected));
    unsigned total = calls;
    for (unsigned i = 1; i <= total; ++i) {
        calls = 0;
        stop = i;
        output = sentinel;
        assert(hpclang_rpl_bank_target_prepare(&view, &sources, &edge, 4, &workspace, &output,
                                               observe, 0) == -79);
        assert(!memcmp(&output, &sentinel, sizeof output));
    }
    calls = 0;
    stop = 2;
    callback_error = 1;
    assert(hpclang_rpl_bank_target_prepare(&view, &sources, &edge, 4, &workspace, &output, observe,
                                           0) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&output, &sentinel, sizeof output));
    stop = 0;
    edge.kind = HPCLANG_RPL_REFERENCE_EXECUTION;
    calls = 0;
    assert(hpclang_rpl_bank_target_prepare(&view, &sources, &edge, 4, &workspace, &output, observe,
                                           0) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!calls && !memcmp(&output, &sentinel, sizeof output));
    edge.kind = HPCLANG_RPL_REFERENCE_ACPTR;
    for (unsigned id = 1; id <= 3; ++id)
        assert(hpclang_rpl_bank_target_prepare(&view, &sources, &edge, id, &workspace, &output,
                                               observe, 0) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!calls && !memcmp(&output, &sentinel, sizeof output));
    selected.geometry.bytes = 2;
    assert(hpclang_rpl_bank_target_prepare(&view, &sources, &edge, 4, &workspace, &output, observe,
                                           0) < 0);
    assert(!memcmp(&output, &sentinel, sizeof output));
    selected.geometry.bytes = 0x20000;
    selected.effects.map.pages[64] = 0x140000;
    memcpy(flash, ram + 0x20000, 5);
    assert(!hpclang_rpl_bank_target_prepare(&view, &sources, &edge, 4, &workspace, &output, observe,
                                            0));
    assert(output.node.object.limit == 10);
    struct hpclang_rpl_bank_target_view target_view = {&view, &output};
    uint8_t value = 99;
    for (unsigned i = 0; i < 10; ++i) {
        assert(hpclang_rpl_bank_target_read(&target_view, 0x40000 + i, &value));
        assert(value == ((flash[i / 2] >> (4 * (i & 1))) & 15));
    }
    value = 99;
    assert(!hpclang_rpl_bank_target_read(&target_view, 0x3ffff, &value) && value == 99);
    assert(!hpclang_rpl_bank_target_read(&target_view, 0x4000a, &value) && value == 99);
    output = sentinel;
    sections[1] ^= 4;
    assert(hpclang_rpl_bank_target_prepare(&view, &sources, &edge, 4, &workspace, &output, observe,
                                           0) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&output, &sentinel, sizeof output));
    return 0;
}
