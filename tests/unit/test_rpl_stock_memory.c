#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
#include "rpl-stock-memory.h"

static const struct hpclang_boot_info *active_boot;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return active_boot;
}

struct span_walk
{
    const uint32_t *map;
    uint32_t next, end, previous_end, calls, stop;
};

static int inspect_span(void *context, uint32_t logical, uint32_t physical, uint32_t span)
{
    struct span_walk *walk = context;
    assert(logical == walk->next && span && span <= walk->end - logical);
    if (walk->calls)
        assert(physical != walk->previous_end); /* Maximal runs. */
    for (uint32_t i = 0; i < span; ++i) {
        uint32_t at = logical + i;
        assert(physical + i == walk->map[at / 4096] * 2u + at % 4096);
    }
    walk->previous_end = physical + span;
    walk->next += span;
    return ++walk->calls == walk->stop ? 73 : 0;
}

static void test_discontinuous_spans(void)
{
    uint32_t map[256];
    for (unsigned mode = 0; mode < 4; ++mode) {
        for (unsigned i = 0; i < 256; ++i)
            map[i] = mode == 0   ? i * 0x800u
                     : mode == 1 ? 0x08020000u
                     : mode == 2 ? 0x08000000u + (255u - i) * 0x800u
                                 : (i & 1u ? 0x08000000u : 0u) + (i / 2u) * 0x800u;
        struct span_walk walk = {.map = map, .end = 0x100000};
        assert(!hpclang_rpl_stock_visit_spans(map, 0, walk.end, inspect_span, &walk));
        assert(walk.next == walk.end && walk.calls == (mode ? 256u : 1u));
        /* Offsets, nibble parity, partial pages and cross-region boundaries. */
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t start = (i * 7919u) % 0x100000u;
            uint32_t length = 1u + (i * 2053u) % 65536u;
            if (length > 0x100000u - start)
                length = 0x100000u - start;
            walk = (struct span_walk){.map = map, .next = start, .end = start + length};
            assert(!hpclang_rpl_stock_visit_spans(map, start, length, inspect_span, &walk));
            assert(walk.next == walk.end && walk.calls);
        }
        if (mode) {
            for (uint32_t stop = 1; stop <= 256; ++stop) {
                walk = (struct span_walk){.map = map, .end = 0x100000, .stop = stop};
                assert(hpclang_rpl_stock_visit_spans(map, 0, walk.end, inspect_span, &walk) == 73);
                assert(walk.calls == stop && walk.next == stop * 4096u);
            }
        }
    }
    for (unsigned i = 0; i < 256; ++i)
        map[i] = i * 0x800u;
    const uint32_t invalid[] = {1, 0x200000, 0x08080000, UINT32_MAX};
    for (unsigned i = 0; i < 256; ++i) {
        for (unsigned j = 0; j < sizeof invalid / sizeof invalid[0]; ++j) {
            map[i] = invalid[j];
            struct span_walk walk = {.map = map, .end = 0x100000};
            assert(hpclang_rpl_stock_visit_spans(map, 0, walk.end, inspect_span, &walk) ==
                   HPCLANG_RPL_SPAN_UNSUPPORTED);
            assert(!walk.calls); /* Reject even a late bad page before callbacks. */
        }
        map[i] = i * 0x800u;
    }
    struct span_walk walk = {.map = map, .end = 0x100000};
    assert(hpclang_rpl_stock_visit_spans(map, 0, 0, inspect_span, &walk) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_visit_spans(map, 0xfffff, 2, inspect_span, &walk) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_visit_spans(NULL, 0, 1, inspect_span, &walk) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_stock_visit_spans(map, 0, 1, NULL, &walk) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!walk.calls);
}

static uint32_t mapped_nibble(const uint32_t map[256], uint32_t address)
{
    return map[address / 4096] * 2u + address % 4096;
}

static void test_mapping_overlap(void)
{
    uint32_t left[256], right[256];
    for (unsigned i = 0; i < 256; ++i) {
        left[i] = i * 0x800u;
        right[i] = 0x08000000u + i * 0x800u;
    }
    assert(!hpclang_rpl_stock_spans_overlap(left, 0, 0x100000, right, 0, 0x100000));
    right[128] = left[64];
    assert(hpclang_rpl_stock_spans_overlap(left, 0x40003, 1, right, 0x80003, 1) == 1);
    assert(!hpclang_rpl_stock_spans_overlap(left, 0x40002, 1, right, 0x80003, 1));
    assert(!hpclang_rpl_stock_spans_overlap(left, 0x40004, 1, right, 0x80003, 1));
    assert(!hpclang_rpl_stock_spans_overlap(left, 0x40000, 4096, right, 0x40000, 4096));
    /* A late invalid page must not be hidden by an earlier matching alias. */
    right[0] = left[0];
    right[255] = 1;
    assert(hpclang_rpl_stock_spans_overlap(left, 0, 0x100000, right, 0, 0x100000) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    right[255] = 0x0807f800;
    left[255] = 1;
    assert(hpclang_rpl_stock_spans_overlap(left, 0, 0x100000, right, 0, 0x100000) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    left[255] = 255 * 0x800u;
    assert(hpclang_rpl_stock_spans_overlap(left, 0, 1, NULL, 0, 1) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_stock_spans_overlap(NULL, 0, 1, right, 0, 1) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_stock_spans_overlap(left, 0, 1, right, 0, 0) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_spans_overlap(left, 0, 0, right, 0, 1) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_spans_overlap(left, 0, 1, right, 0xfffff, 2) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    unsigned positives = 0, negatives = 0;
    for (uint32_t trial = 0; trial < 256; ++trial) {
        for (uint32_t page = 0; page < 256; ++page)
            left[page] = ((page * 17u + trial) % 256u) * 0x800u;
        for (uint32_t page = 0; page < 256; ++page)
            right[page] = left[(page + trial) % 256u] + (trial & 1u ? 0x08000000u : 0u);
        uint32_t a = (trial % 255u) * 4096u + 4095u - trial % 16u;
        uint32_t b = ((trial % 255u + 256u - trial) % 256u) * 4096u + 4093u;
        uint32_t an = 1u + trial % 97u, bn = 1u + trial % 89u;
        if (bn > 0x100000u - b)
            bn = 0x100000u - b;
        int expected = 0;
        for (uint32_t i = 0; i < an; ++i)
            for (uint32_t j = 0; j < bn; ++j)
                expected |= mapped_nibble(left, a + i) == mapped_nibble(right, b + j);
        assert(hpclang_rpl_stock_spans_overlap(left, a, an, right, b, bn) == expected);
        assert(hpclang_rpl_stock_spans_overlap(right, b, bn, left, a, an) == expected);
        positives += expected != 0;
        negatives += expected == 0;
    }
    assert(positives && negatives);
}

static void test_physical_spans(void)
{
    uint32_t map[256];
    for (unsigned i = 0; i < 256; ++i)
        map[i] = i * 0x800u;
    uint32_t physical = 123;
    /* Every nibble, including both parities and every page boundary. */
    for (uint32_t at = 0; at < 0x100000u; ++at) {
        assert(!hpclang_rpl_stock_resolve_span(map, at, 1, &physical));
        assert(physical == at);
    }
    assert(!hpclang_rpl_stock_resolve_span(map, 0, 0x100000, &physical));
    assert(physical == 0);
    for (unsigned page = 0; page < 256; ++page) {
        uint32_t saved = map[page];
        map[page] = 0x08020000;
        assert(!hpclang_rpl_stock_resolve_span(map, page * 4096 + 7, 4089, &physical));
        assert(physical == 0x10040007);
        if (page) {
            physical = 123;
            assert(hpclang_rpl_stock_resolve_span(map, page * 4096 - 1, 2, &physical) ==
                   HPCLANG_RPL_SPAN_UNSUPPORTED);
            assert(physical == 123);
        }
        map[page] = saved;
    }
    /* Two logical pages alias ERAM; switching one to Flash separates them. */
    map[64] = map[128] = 0x08020000;
    uint32_t alias;
    assert(!hpclang_rpl_stock_resolve_span(map, 0x40005, 5, &physical));
    assert(!hpclang_rpl_stock_resolve_span(map, 0x80005, 5, &alias));
    assert(physical == alias);
    map[64] = 0x140000;
    assert(!hpclang_rpl_stock_resolve_span(map, 0x40005, 5, &physical));
    assert(physical == 0x280005 && physical != alias);
    const uint32_t bad[] = {1, 0x200000, 0x08080000, 0x49000000, UINT32_MAX};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        map[64] = bad[i];
        physical = 123;
        assert(hpclang_rpl_stock_resolve_span(map, 0x40000, 1, &physical) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(physical == 123);
    }
    map[255] = 0x0807f800;
    assert(!hpclang_rpl_stock_resolve_span(map, 0xfffff, 1, &physical));
    assert(physical == 0x100fffff);
    physical = 123;
    assert(hpclang_rpl_stock_resolve_span(map, 0xfffff, 2, &physical) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_resolve_span(map, 0, 0, &physical) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_resolve_span(map, UINT32_MAX, 1, &physical) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_resolve_span(map, 1, UINT32_MAX, &physical) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_stock_resolve_span(NULL, 0, 1, &physical) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_stock_resolve_span(map, 0, 1, NULL) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(physical == 123);
}

int main(void)
{
    test_discontinuous_spans();
    test_mapping_overlap();
    test_physical_spans();
    static uint8_t port1[HPCLANG_RPL_PORT1_NIBBLES / 2u];
    struct hpclang_rpl_reader port_reader = {0};
    assert(!hpclang_rpl_stock_port2_reader(0, &port_reader));
    assert(!hpclang_rpl_stock_port2_capture(&port_reader, port1, 0x00000c02u));
    assert(!hpclang_rpl_stock_port2_capture(&port_reader, port1, 0x00100402u));
    assert(!hpclang_rpl_stock_port2_capture(&port_reader, port1, 0x00100c01u));
    assert(port_reader.read_nibble == NULL);
    assert(hpclang_rpl_stock_port2_capture(&port_reader, port1, 0x00100802u));
    port1[0] = 0x71;
    port1[sizeof(port1) - 1] = 0xab;
    uint8_t flash_nibble;
    assert(port_reader.read_nibble(port_reader.context, 1, &flash_nibble) && flash_nibble == 7);
    assert(port_reader.read_nibble(port_reader.context, HPCLANG_RPL_FLASH_BANK_NIBBLES - 1,
                                   &flash_nibble) &&
           flash_nibble == 10);
    assert(!port_reader.read_nibble(port_reader.context, HPCLANG_RPL_FLASH_BANK_NIBBLES,
                                    &flash_nibble));
    assert(!port_reader.read_nibble(port_reader.context, 0, NULL));
    port_reader = (struct hpclang_rpl_reader){0};
    assert(!hpclang_rpl_stock_port1_reader(&port_reader));
    assert(!hpclang_rpl_stock_port1_capture(NULL, port1, 0x08000c1au));
    assert(!hpclang_rpl_stock_port1_capture(&port_reader, NULL, 0x08000c1au));
    assert(!hpclang_rpl_stock_port1_capture(&port_reader, port1, 0x00000c1au));
    assert(!hpclang_rpl_stock_port1_capture(&port_reader, port1, 0x0800081au));
    assert(port_reader.read_nibble == NULL);
    assert(hpclang_rpl_stock_port1_capture(&port_reader, port1, 0x08000c1au));
    port1[0] = 0xa3;
    port1[sizeof(port1) - 1] = 0xb4;
    uint8_t nibble;
    assert(port_reader.read_nibble(port_reader.context, 0, &nibble) && nibble == 3);
    assert(port_reader.read_nibble(port_reader.context, 1, &nibble) && nibble == 10);
    assert(port_reader.read_nibble(port_reader.context, HPCLANG_RPL_PORT1_NIBBLES - 1, &nibble) &&
           nibble == 11);
    assert(!port_reader.read_nibble(port_reader.context, HPCLANG_RPL_PORT1_NIBBLES, &nibble));
    assert(!port_reader.read_nibble(port_reader.context, 0, NULL));
    uint32_t map[256] = {0};
    struct hpclang_rpl_stock_memory memory;
    for (uint32_t i = 0u; i < 128u; ++i)
        map[128u + i] = 0x08040000u + i * 0x800u;
    assert(hpclang_rpl_stock_memory_capture(&memory, map, 0x08000c1au));
    uintptr_t last = memory.pages[127];
    map[255] = 0u;
    assert(memory.pages[127] == last); /* captured mapping no longer follows Read[] */
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x08000c1au));
    map[255] = (uint32_t)last;
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x0800081au)); /* read-only */
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x00000c1au)); /* ROM alias */
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x08000c19u)); /* coarse table */
    map[255] = map[128];
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x08000c1au)); /* duplicate RAM */
    map[255] = (uint32_t)last + 1u;
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x08000c1au)); /* unaligned */
    map[255] = 0x08080000u;
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x08000c1au)); /* past physical SRAM */
    map[255] = 0x0007e000u;
    assert(!hpclang_rpl_stock_memory_capture(&memory, map, 0x08000c1au)); /* ROM page */

    uint8_t bytes[4096];
    memset(bytes, 0xa5, sizeof(bytes));
    memory.pages[0] = (uintptr_t)bytes;
    memory.pages[1] = (uintptr_t)(bytes + 2048);
    uint8_t value = 0u;
    assert(hpclang_rpl_stock_memory_write(&memory, 0x80fffu, 3u));
    assert(bytes[2047] == 0x35u);
    assert(hpclang_rpl_stock_memory_write(&memory, 0x81000u, 7u));
    assert(bytes[2048] == 0xa7u);
    assert(hpclang_rpl_stock_memory_read(&memory, 0x80fffu, &value) && value == 3u);
    assert(hpclang_rpl_stock_memory_read(&memory, 0x81000u, &value) && value == 7u);
    assert(!hpclang_rpl_stock_memory_write(&memory, 0x7ffffu, 1u));
    assert(!hpclang_rpl_stock_memory_write(&memory, 0x100000u, 1u));
    assert(!hpclang_rpl_stock_memory_write(&memory, 0x81000u, 16u));
    assert(bytes[2048] == 0xa7u);
    assert(!hpclang_rpl_stock_mutation_io(NULL, &memory));
    struct hpclang_rpl_mutation_io io = {0};
    struct hpclang_boot_info boot = {.model = HPCLANG_MODEL_HP50G, .saturn_state = 1u};
    active_boot = &boot;
    assert(!hpclang_rpl_stock_mutation_io(&io, &memory));
    boot.platform_cookie = HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    assert(!hpclang_rpl_stock_mutation_io(&io, &memory));
    struct hpclang_rpl_memory_bounds bounds = {0};
    boot.platform_cookie = (uintptr_t)&bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    assert(!hpclang_rpl_stock_mutation_io(&io, &memory));
    assert(io.read_nibble == NULL && io.write_nibble == NULL);
    return 0;
}
