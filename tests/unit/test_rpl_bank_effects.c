#include <assert.h>
#include <string.h>
#include <hpclang/bootstrap.h>
#include "rpl-bank-effects.h"

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return 0;
}

struct fixture
{
    uint8_t bytes[0xc60];
    unsigned reads, observed, stop, mode;
    uint8_t seen[0xc60 * 2];
};

static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    struct fixture *f = context;
    assert(at < sizeof f->bytes * 2);
    *value = (f->bytes[at / 2] >> (4 * (at & 1))) & 15;
    if (++f->reads == f->stop) {
        if (f->mode == 1)
            return 0;
        if (f->mode == 2)
            *value = 16;
    }
    return 1;
}

static int observe(void *context, uint32_t at, uint8_t value)
{
    struct fixture *f = context;
    assert(value == ((f->bytes[at / 2] >> (4 * (at & 1))) & 15));
    assert(!f->seen[at]++);
    if (++f->observed == f->stop && f->mode >= 3)
        return f->mode == 3 ? -73 : 73;
    return 0;
}

static uint32_t word(const struct fixture *f, unsigned at)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
        result |= (uint32_t)f->bytes[at + i] << (8 * i);
    return result;
}

static void test_capture(void)
{
    struct fixture f = {0};
    for (unsigned i = 0; i < sizeof f.bytes; ++i)
        f.bytes[i] = (uint8_t)(i * 13 + 7);
    struct hpclang_rpl_reader reader = {read_nibble, &f};
    struct hpclang_rpl_bank_effects state, expected, sentinel;
    memset(&expected, 0, sizeof expected);
    for (unsigned i = 0; i < 256; ++i) {
        expected.map.pages[i] = word(&f, i * 4);
        expected.map.owners[i] = f.bytes[0x808 + i];
        expected.writes[i] = word(&f, 0x404 + i * 4);
    }
    expected.map.lower = word(&f, 0xbcc);
    expected.map.upper = word(&f, 0xbd0);
    expected.map.overlay_page = word(&f, 0xc48);
    expected.map.overlay_physical = word(&f, 0xc5c);
    expected.sentinel = word(&f, 0x400);
    for (unsigned i = 0; i < 4; ++i)
        expected.cache[i] = word(&f, 0xc4c + i * 4);
    assert(!hpclang_rpl_bank_effects_capture(&reader, &state, observe, &f));
    assert(f.reads == 4680 && f.observed == 4680);
    assert(!memcmp(&state, &expected, sizeof state));
    memset(&sentinel, 0xa5, sizeof sentinel);
    for (unsigned mode = 1; mode <= 4; ++mode) {
        for (unsigned stop = 1; stop <= 4680; ++stop) {
            f.mode = mode;
            f.stop = stop;
            f.reads = f.observed = 0;
            memset(f.seen, 0, sizeof f.seen);
            state = sentinel;
            int status = hpclang_rpl_bank_effects_capture(&reader, &state, observe, &f);
            assert(status == (mode == 1   ? HPCLANG_RPL_SPAN_UNAVAILABLE
                              : mode == 3 ? -73
                                          : HPCLANG_RPL_SPAN_MALFORMED));
            assert(f.reads == stop && !memcmp(&state, &sentinel, sizeof state));
        }
    }
    assert(hpclang_rpl_bank_effects_capture(0, &state, observe, &f) < 0);
    assert(hpclang_rpl_bank_effects_capture(&reader, 0, observe, &f) < 0);
    assert(hpclang_rpl_bank_effects_capture(&reader, &state, 0, &f) < 0);
    reader.read_nibble = 0;
    assert(hpclang_rpl_bank_effects_capture(&reader, &state, observe, &f) < 0);
}

int main(void)
{
    test_capture();
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
