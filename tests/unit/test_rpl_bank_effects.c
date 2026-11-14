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

static void test_cpu(void)
{
    uint8_t input[0x90] = {0};
    input[0x13] = 0xab; /* Preserve high C digits. */
    input[0x88] = 0x30;
    input[0x89] = 0x31;
    input[0x50] = input[0x64] = 1;
    struct hpclang_rpl_bank_cpu_effects output, expected, sentinel;
    assert(!hpclang_rpl_bank_cpu_predict(input, 0x7f024, &output));
    assert(output.continuation == 0x3130 && output.state[0x8c] == 7);
    assert(output.state[0x88] == 0 && output.state[0x89] == 0);
    assert(output.state[0x10] == 0x24 && output.state[0x11] == 0xf0 && output.state[0x12] == 7 &&
           output.state[0x13] == 0xab);
    assert(!output.state[0x50] && !output.state[0x64]);
    expected = output;
    memcpy(output.state, input, sizeof input);
    assert(!hpclang_rpl_bank_cpu_predict(output.state, 0x7f024, &output));
    assert(!memcmp(&output, &expected, sizeof output));
    memset(&sentinel, 0xa5, sizeof sentinel);
    const unsigned offsets[] = {0x4a, 0x4e, 0x50, 0x64, 0x8c, 0x8a};
    const uint8_t invalid[] = {16, 16, 2, 2, 8, 16};
    for (unsigned i = 0; i < sizeof offsets / sizeof *offsets; ++i) {
        unsigned at = offsets[i];
        uint8_t saved = input[at];
        input[at] = invalid[i];
        output = sentinel;
        assert(hpclang_rpl_bank_cpu_predict(input, 0x7f024, &output) < 0);
        assert(!memcmp(&output, &sentinel, sizeof output));
        input[at] = saved;
    }
    assert(hpclang_rpl_bank_cpu_predict(input, 0x100000, &output) < 0);
    assert(!memcmp(&output, &sentinel, sizeof output));
    assert(hpclang_rpl_bank_cpu_predict(0, 0, &output) < 0);
    assert(hpclang_rpl_bank_cpu_predict(input, 0, 0) < 0);
}

static void test_cpu_capture(void)
{
    struct fixture f = {0};
    f.bytes[0x994] = 0x30;
    f.bytes[0x995] = 0x31;
    struct hpclang_rpl_reader reader = {read_nibble, &f};
    struct hpclang_rpl_bank_cpu_effects output, expected, sentinel;
    memset(&sentinel, 0xa5, sizeof sentinel);
    assert(!hpclang_rpl_bank_cpu_predict(f.bytes + 0x90c, 0x7f024, &expected));
    assert(!hpclang_rpl_bank_cpu_capture_predict(&reader, 0x7f024, 0, &output, observe, &f));
    assert(f.reads == 296 && f.observed == 296);
    assert(!memcmp(&output, &expected, sizeof output));
    for (unsigned mode = 1; mode <= 4; ++mode) {
        for (unsigned stop = 1; stop <= 296; ++stop) {
            f.mode = mode;
            f.stop = stop;
            f.reads = f.observed = 0;
            memset(f.seen, 0, sizeof f.seen);
            output = sentinel;
            int status =
                hpclang_rpl_bank_cpu_capture_predict(&reader, 0x7f024, 0, &output, observe, &f);
            assert(status == (mode == 1   ? HPCLANG_RPL_SPAN_UNAVAILABLE
                              : mode == 3 ? -73
                                          : HPCLANG_RPL_SPAN_MALFORMED));
            assert(f.reads == stop && !memcmp(&output, &sentinel, sizeof output));
        }
    }
    f.mode = f.stop = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        memset(f.bytes + 0xc24, 0, 4);
        f.bytes[0xc24 + bit / 8] = (uint8_t)(1u << (bit % 8));
        f.reads = f.observed = 0;
        memset(f.seen, 0, sizeof f.seen);
        output = sentinel;
        assert(hpclang_rpl_bank_cpu_capture_predict(&reader, 0x7f024, 0, &output, observe, &f) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(f.reads == 8 && !memcmp(&output, &sentinel, sizeof output));
    }
    memset(f.bytes + 0xc24, 0, 4);
    memset(f.seen, 0, sizeof f.seen);
    f.reads = f.observed = 0;
    assert(hpclang_rpl_bank_cpu_capture_predict(&reader, 0x7f024, 1, &output, observe, &f) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&output, &sentinel, sizeof output));
}

static void set_word(struct fixture *f, unsigned at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        f->bytes[at + i] = (uint8_t)(value >> (8 * i));
}

static void test_config(void)
{
    struct fixture f = {0}, output = {0}, expected, original;
    set_word(&f, 0xb9c, 3);
    expected = f;
    set_word(&expected, 0xb9c, 35);
    assert(!hpclang_rpl_config_predict(f.bytes, 0xdeadbeef, output.bytes));
    assert(!memcmp(output.bytes, expected.bytes, sizeof f.bytes));
    set_word(&f, 0xb9c, 0);
    assert(!hpclang_rpl_config_predict(f.bytes, 0xfffabcde, output.bytes));
    assert(word(&output, 0xb9c) == 1 && word(&output, 0xba0) == (0xabcdeu >> 6));
    for (unsigned i = 0; i < 256; ++i) {
        set_word(&f, 4 * i, 0x08000000 + 0x800 * i);
        set_word(&f, 0x404 + 4 * i, 0x09000000 + 0x800 * i);
    }
    f.bytes[0x808 + 64] = 5;
    set_word(&f, 0xb9c, 33);
    set_word(&f, 0xbac, 16);
    set_word(&f, 0xbd4, 0x08030000);
    set_word(&f, 0xc48, 64);
    set_word(&f, 0xc5c, 0x08030000);
    set_word(&f, 0xc50, 0xa5);
    set_word(&f, 0xc58, 0x5a);
    assert(!hpclang_rpl_config_predict(f.bytes, 65u << 12, output.bytes));
    assert(word(&output, 0xb9c) == 3 && word(&output, 0xba8) == 65);
    assert(word(&output, 64 * 4) == 0x08030000 && word(&output, 65 * 4) == 0x08030800);
    assert(word(&output, 0xc4c) == word(&f, 64 * 4));
    assert(word(&output, 0xc54) == word(&f, 0x404 + 64 * 4));
    assert(word(&output, 0xc50) == 0xa5 && word(&output, 0xc58) == 0x5a);
    assert(output.bytes[0x808 + 64] == 5 && output.bytes[0x808 + 65] == 5);
    assert(word(&output, 0x400) == word(&f, 0) && word(&output, 0x804) == word(&f, 0x404));
    original = f;
    assert(!hpclang_rpl_config_predict(f.bytes, 65u << 12, f.bytes));
    assert(!memcmp(f.bytes, output.bytes, sizeof f.bytes));
    f = original;
    memset(output.bytes, 0xa5, sizeof output.bytes);
    expected = output;
    for (unsigned bad = 0; bad < 3; ++bad) {
        f = original;
        uint32_t c = 65u << 12;
        if (bad == 0)
            set_word(&f, 0xbac, 257);
        if (bad == 1) {
            set_word(&f, 0xbac, 7);
            c = 255u << 12;
        }
        if (bad == 2)
            set_word(&f, 0xc48, 255);
        assert(hpclang_rpl_config_predict(f.bytes, c, output.bytes) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(output.bytes, expected.bytes, sizeof output.bytes));
        struct fixture before = f;
        assert(hpclang_rpl_config_predict(f.bytes, c, f.bytes) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(f.bytes, before.bytes, sizeof f.bytes));
    }
    f = original;
    set_word(&f, 0xbac, 0);
    set_word(&f, 0xc48, UINT32_MAX);
    assert(!hpclang_rpl_config_predict(f.bytes, 65u << 12, output.bytes));
    assert(word(&output, 0x400) == word(&f, 0) && word(&output, 0xc4c) == word(&f, 0xc4c));
    for (unsigned flags = 62; flags < 66; ++flags) {
        set_word(&f, 0xb9c, flags);
        assert(!hpclang_rpl_config_predict(f.bytes, 0xffffffff, output.bytes));
        assert(!memcmp(f.bytes, output.bytes, sizeof f.bytes));
    }
    assert(hpclang_rpl_config_predict(NULL, 0, output.bytes) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_config_predict(f.bytes, 0, NULL) == HPCLANG_RPL_SPAN_UNAVAILABLE);
}

static void test_bank_write(void)
{
    struct fixture f = {0};
    set_word(&f, 0xba0, UINT32_MAX);
    set_word(&f, 0xbb0, 0x7f);
    for (unsigned offset = 0; offset < 4096; ++offset) {
        unsigned lower = (offset >> 5) & 3, upper = (offset >> 1) & 15;
        set_word(&f, 0xbcc, lower);
        set_word(&f, 0xbd0, upper);
        struct hpclang_rpl_bank_write result;
        assert(!hpclang_rpl_bank_write_inspect(f.bytes, 0x7f000 + offset, &result));
        assert(result.lower == lower && result.upper == upper && result.unchanged);
        set_word(&f, 0xbd0, upper ^ 1);
        assert(!hpclang_rpl_bank_write_inspect(f.bytes, 0x7f000 + offset, &result));
        assert(!result.unchanged);
        set_word(&f, 0xbd0, upper);
        set_word(&f, 0xbcc, lower ^ 1);
        assert(!hpclang_rpl_bank_write_inspect(f.bytes, 0x7f000 + offset, &result));
        assert(!result.unchanged);
    }
    const struct hpclang_rpl_bank_write saved = {91, 92, 93};
    struct hpclang_rpl_bank_write result = saved;
    set_word(&f, 0xba0, 0x7f024 >> 6);
    assert(hpclang_rpl_bank_write_inspect(f.bytes, 0x7f024, &result) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&saved, &result, sizeof result));
    set_word(&f, 0xba0, UINT32_MAX);
    assert(hpclang_rpl_bank_write_inspect(f.bytes, 0x80000, &result) < 0);
    assert(hpclang_rpl_bank_write_inspect(f.bytes, 0x100000, &result) < 0);
    assert(hpclang_rpl_bank_write_inspect(NULL, 0x7f024, &result) < 0);
    assert(hpclang_rpl_bank_write_inspect(f.bytes, 0x7f024, NULL) < 0);
    assert(!memcmp(&saved, &result, sizeof result));
}

static void test_unconfig(void)
{
    struct fixture f = {0};
    set_word(&f, 0xba0, 0x7f000 >> 6);
    for (unsigned offset = 0; offset < 4096; ++offset) {
        set_word(&f, 0xba8, 0x7f);
        set_word(&f, 0xbb8, 0x7f);
        set_word(&f, 0xbc4, 0x7f);
        set_word(&f, 0xbb0, 0x7f);
        const unsigned flags[] = {31, 29, 21, 5, 0xffffffff};
        const unsigned kinds[] = {HPCLANG_RPL_UNCONFIG_RAM, HPCLANG_RPL_UNCONFIG_ERAM,
                                  HPCLANG_RPL_UNCONFIG_PORT, HPCLANG_RPL_UNCONFIG_BANK,
                                  HPCLANG_RPL_UNCONFIG_RAM};
        for (unsigned i = 0; i < 5; ++i) {
            set_word(&f, 0xb9c, flags[i]);
            struct hpclang_rpl_unconfig effect;
            assert(!hpclang_rpl_unconfig_inspect(f.bytes, 0xabf7f000 + offset, &effect));
            assert(effect.kind == (offset >= 64 ? kinds[i] : HPCLANG_RPL_UNCONFIG_IO));
            assert(effect.rebuild == (offset >= 64 && i != 3));
            assert(effect.clear_field == (offset >= 64 ? i == 3 ? 0xbb0u : 0u : 0xba0u));
        }
    }
    set_word(&f, 0xb9c, 0xffffffc0);
    struct hpclang_rpl_unconfig effect;
    assert(!hpclang_rpl_unconfig_inspect(f.bytes, 0x80000, &effect));
    assert(!effect.kind && effect.flags == 0xffffffc0 && !effect.clear_field && !effect.rebuild);
    struct hpclang_rpl_unconfig saved = effect;
    assert(hpclang_rpl_unconfig_inspect(NULL, 0, &effect) < 0);
    assert(!memcmp(&effect, &saved, sizeof effect));
    assert(hpclang_rpl_unconfig_inspect(f.bytes, 0, NULL) < 0);
}

int main(void)
{
    test_bank_write();
    test_unconfig();
    test_config();
    test_cpu_capture();
    test_cpu();
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
