#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include "rpl-bank-effects.h"

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return 0;
}

int main(void)
{
    struct hpclang_rpl_rom_bank banks[16];
    for (unsigned i = 0; i < 16; ++i)
        banks[i] = (struct hpclang_rpl_rom_bank){i * 0x20000u, 0x20000u};
    struct hpclang_rpl_bank_effects state = {0}, output, sentinel;
    for (unsigned i = 0; i < 256; ++i) {
        state.map.pages[i] = 0x08000000u + i * 0x800u;
        state.writes[i] = 0xdead0000u + i;
    }
    state.sentinel = 0x12345678;
    for (unsigned i = 0; i < 4; ++i)
        state.cache[i] = i + 9;
    memset(&sentinel, 0xa5, sizeof sentinel);
    output = sentinel;
    assert(hpclang_rpl_bank_effects_select(0, banks, 0, 0, &output) < 0);
    assert(memcmp(&output, &sentinel, sizeof output) == 0);
    assert(hpclang_rpl_bank_effects_select(&state, 0, 0, 0, &output) < 0);
    assert(hpclang_rpl_bank_effects_select(&state, banks, 0, 0, 0) < 0);
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t saved = state.map.pages[i];
        state.map.pages[i] = 1;
        assert(hpclang_rpl_bank_effects_select(&state, banks, 1, 1, &output) < 0);
        assert(memcmp(&output, &sentinel, sizeof output) == 0);
        state.map.pages[i] = saved;
    }
    state.map.overlay_page = 255;
    assert(hpclang_rpl_bank_effects_select(&state, banks, 1, 1, &output) < 0);
    assert(memcmp(&output, &sentinel, sizeof output) == 0);
    state.map.overlay_page = 0;
    assert(hpclang_rpl_bank_effects_select(&state, banks, 0, 0, &output) == 0);
    assert(memcmp(&output, &state, sizeof output) == 0);
    state.map.overlay_page = 254;
    state.map.overlay_physical = 0x08060000;
    assert(hpclang_rpl_bank_effects_select(&state, banks, 1, 2, &output) == 0);
    assert(output.sentinel == 0x20000);
    assert(output.cache[0] == 0x40000 + 62 * 0x800);
    assert(output.cache[1] == 0x40000 + 63 * 0x800);
    for (unsigned i = 0; i < 2; ++i) {
        assert(output.writes[254 + i] == 0x08060000u + i * 0x800u);
        assert(output.cache[2 + i] == state.writes[254 + i]);
    }
    assert(hpclang_rpl_bank_effects_select(&state, banks, 1, 2, &state) == 0);
    assert(memcmp(&output, &state, sizeof output) == 0);
    return 0;
}
