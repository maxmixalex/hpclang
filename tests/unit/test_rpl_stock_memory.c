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
