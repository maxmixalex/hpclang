#include "rpl-bank-effects.h"

static uint32_t cpu_word(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void cpu_put(uint8_t *bytes, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(value >> (8u * i));
}

int hpclang_rpl_bank_cpu_predict(const uint8_t state[0x90], uint32_t configuration,
                                 struct hpclang_rpl_bank_cpu_effects *output)
{
    if (!state || !output)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (configuration > 0xfffffu || cpu_word(state + 0x48) > 0xfffffu ||
        cpu_word(state + 0x4c) > 0xfffffu || cpu_word(state + 0x50) > 1 ||
        cpu_word(state + 0x64) > 1 || cpu_word(state + 0x8c) >= 8)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned cursor = (cpu_word(state + 0x8c) + 7u) & 7u;
    uint32_t continuation = cpu_word(state + 0x6c + 4u * cursor);
    if (continuation > 0xfffffu)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_bank_cpu_effects next;
    for (unsigned i = 0; i < sizeof next.state; ++i)
        next.state[i] = state[i];
    next.continuation = continuation;
    cpu_put(next.state + 0x10, (cpu_word(state + 0x10) & ~0xfffffu) | configuration);
    cpu_put(next.state + 0x48, configuration);
    cpu_put(next.state + 0x50, 0);
    cpu_put(next.state + 0x64, 0);
    cpu_put(next.state + 0x6c + 4u * cursor, 0);
    cpu_put(next.state + 0x8c, cursor);
    *output = next;
    return 0;
}

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

static int capture_word(const struct hpclang_rpl_reader *reader, uint32_t byte_offset,
                        uint32_t *word, hpclang_rpl_reference_dependency_fn dependency,
                        void *context)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        uint32_t at = byte_offset * 2u + i;
        uint8_t value;
        if (!reader->read_nibble(reader->context, at, &value))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (value > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        int status = dependency(context, at, value);
        if (status)
            return status < 0 ? status : HPCLANG_RPL_SPAN_MALFORMED;
        result |= (uint32_t)value << (4u * i);
    }
    *word = result;
    return 0;
}

int hpclang_rpl_bank_cpu_capture_predict(const struct hpclang_rpl_reader *reader,
                                         uint32_t configuration, unsigned expected_p,
                                         struct hpclang_rpl_bank_cpu_effects *output,
                                         hpclang_rpl_reference_dependency_fn dependency,
                                         void *context)
{
    if (!reader || !reader->read_nibble || !output || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (expected_p > 1 || configuration > 0xfffffu)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t pending;
    int status = capture_word(reader, 0xc24u, &pending, dependency, context);
    if (status)
        return status;
    if (pending)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint8_t state[0x90];
    for (unsigned i = 0; i < sizeof state; i += 4) {
        uint32_t value;
        status = capture_word(reader, 0x90cu + i, &value, dependency, context);
        if (status)
            return status;
        cpu_put(state + i, value);
    }
    if (cpu_word(state + 0x50) != expected_p)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    return hpclang_rpl_bank_cpu_predict(state, configuration, output);
}

int hpclang_rpl_bank_effects_capture(const struct hpclang_rpl_reader *reader,
                                     struct hpclang_rpl_bank_effects *state,
                                     hpclang_rpl_reference_dependency_fn dependency, void *context)
{
    if (!reader || !reader->read_nibble || !state || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct hpclang_rpl_bank_effects next;
    int status = hpclang_rpl_stock_bank_map_capture(reader, &next.map, dependency, context);
    if (status)
        return status;
    status = capture_word(reader, 0x400u, &next.sentinel, dependency, context);
    if (status)
        return status;
    for (unsigned i = 0; i < 256; ++i) {
        status = capture_word(reader, 0x404u + i * 4u, &next.writes[i], dependency, context);
        if (status)
            return status;
    }
    for (unsigned i = 0; i < 4; ++i) {
        status = capture_word(reader, 0xc4cu + i * 4u, &next.cache[i], dependency, context);
        if (status)
            return status;
    }
    *state = next;
    return 0;
}
