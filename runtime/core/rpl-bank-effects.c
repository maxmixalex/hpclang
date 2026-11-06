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

int hpclang_rpl_config_predict(const uint8_t state[0xc60], uint32_t c, uint8_t output[0xc60])
{
    if (!state || !output)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    uint32_t flags = cpu_word(state + 0xb9c), low = flags & 31u;
    uint32_t field = 0, value = 0, next_flags = flags;
    uint32_t count = 0, first = 0, physical = 0, priority = 0, overlay = 0;
    int map = 0;
    c &= 0xfffffu;
    if (flags <= 61) {
        if (flags < 32 && !(flags & 1)) {
            next_flags = flags | 1;
            field = 0xba0;
            value = c >> 6;
        } else if ((low & 3) == 1) {
            if (flags < 32) {
                next_flags = flags | 32;
                field = 0xbac;
                value = 256 - (c >> 12);
            } else {
                next_flags = flags - 30;
                field = 0xba8;
                priority = 5;
                map = 1;
                count = cpu_word(state + 0xbac);
                physical = cpu_word(state + 0xbd4);
            }
        } else if ((low & 7) == 3) {
            next_flags = flags < 32 ? flags | 32 : flags - 28;
            if (flags >= 32) {
                field = 0xbb0;
                value = c >> 12;
            }
        } else if ((low & 15) == 7) {
            if (flags < 32) {
                next_flags = flags | 32;
                field = 0xbbc;
                value = 256 - (c >> 12);
            } else {
                next_flags = flags - 24;
                field = 0xbb8;
                priority = 2;
                map = 1;
                count = cpu_word(state + 0xbbc);
                physical = cpu_word(state + 0xbd8);
            }
        } else if (low == 15) {
            if (flags < 32) {
                next_flags = 47;
                field = 0xbc8;
                value = 256 - (c >> 12);
            } else {
                next_flags = 31;
                field = 0xbc4;
                priority = 3;
                map = 1;
                count = cpu_word(state + 0xbc8);
                physical = cpu_word(state + 0xbdc);
            }
        }
    }
    if (map) {
        value = c >> 12;
        if (count > 256)
            return HPCLANG_RPL_SPAN_BOUNDS;
        if (count) {
            first = (value / count) * count;
            if (count > 256 - first)
                return HPCLANG_RPL_SPAN_BOUNDS;
            overlay = cpu_word(state + 0xc48);
            if (overlay > 254)
                return HPCLANG_RPL_SPAN_BOUNDS;
        }
    }
    for (unsigned i = 0; i < 0xc60; ++i)
        output[i] = state[i];
    cpu_put(output + 0xb9c, next_flags);
    if (field)
        cpu_put(output + field, value);
    if (map) {
        for (uint32_t page = first; page < first + count; ++page) {
            if (output[0x808 + page] < priority) {
                output[0x808 + page] = (uint8_t)priority;
                cpu_put(output + page * 4, physical);
                cpu_put(output + 0x404 + page * 4, physical);
            }
            physical += 0x800;
        }
        if (count && overlay)
            for (unsigned i = 0; i < 2; ++i) {
                unsigned at = (overlay + i) * 4;
                uint32_t replacement = cpu_word(output + 0xc5c) + i * 0x800u;
                uint32_t old = cpu_word(output + at);
                if (old != replacement) {
                    cpu_put(output + 0xc4c + 4 * i, old);
                    cpu_put(output + at, replacement);
                    cpu_put(output + 0xc54 + 4 * i, cpu_word(output + 0x404 + at));
                    cpu_put(output + 0x404 + at, replacement);
                }
            }
        output[0x908] = output[0x808];
        cpu_put(output + 0x400, cpu_word(output));
        cpu_put(output + 0x804, cpu_word(output + 0x404));
    }
    return 0;
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
