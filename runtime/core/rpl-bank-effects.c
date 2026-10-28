#include "rpl-bank-effects.h"

int hpclang_rpl_bank_effects_select(const struct hpclang_rpl_bank_effects *state,
                                    const struct hpclang_rpl_rom_bank banks[16], uint32_t lower,
                                    uint32_t upper, struct hpclang_rpl_bank_effects *selected)
{
    if (!state || !banks || !selected)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct hpclang_rpl_bank_effects result = *state;
    int status =
        hpclang_rpl_stock_bank_map_select(&state->map, banks, lower, upper, result.map.pages);
    if (status)
        return status;
    if (lower != state->map.lower || upper != state->map.upper) {
        result.map.lower = lower;
        result.map.upper = upper;
        result.sentinel = result.map.pages[0];
        if (state->map.overlay_page) {
            for (unsigned i = 0; i < 2; ++i) {
                uint32_t page = state->map.overlay_page + i;
                uint32_t prior =
                    state->map.owners[page]
                        ? state->map.pages[page]
                        : banks[page & 64u ? upper : lower].physical + (page % 64u) * 0x800u;
                uint32_t physical = state->map.overlay_physical + i * 0x800u;
                if (prior != physical) {
                    result.cache[i] = prior;
                    result.cache[2u + i] = state->writes[page];
                    result.writes[page] = physical;
                }
            }
        }
    }
    *selected = result;
    return 0;
}
