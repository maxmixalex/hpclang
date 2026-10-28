#include <stddef.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
#include "rpl-stock-memory.h"
#include "rpl-port.h"
#include "launch-services.h"

int hpclang_rpl_stock_bank_route_capture(const struct hpclang_rpl_reader *state,
                                         struct hpclang_rpl_stock_bank_route *route,
                                         hpclang_rpl_reference_dependency_fn dependency,
                                         void *context)
{
    if (!state || !state->read_nibble || !route || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    static const uint32_t offsets[] = {0xb9c, 0xba0, 0xba8, 0xbb8, 0xbc4};
    uint32_t words[5] = {0};
    for (unsigned i = 0; i < 5; ++i) {
        for (unsigned j = 0; j < 8; ++j) {
            uint32_t at = offsets[i] * 2u + j;
            uint8_t value;
            if (!state->read_nibble(state->context, at, &value))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            if (value > 15)
                return HPCLANG_RPL_SPAN_MALFORMED;
            int result = dependency(context, at, value);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            words[i] |= (uint32_t)value << (4u * j);
        }
    }
    *route =
        (struct hpclang_rpl_stock_bank_route){words[0], words[1], words[2], words[3], words[4]};
    return 0;
}

int hpclang_rpl_stock_bank_route_check(const struct hpclang_rpl_stock_bank_route *route,
                                       uint32_t selector, uint32_t configuration)
{
    if (!route)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (selector > 0xffffbu || configuration > 0xffffeu)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if ((route->flags & ~0x18u) != 3u || route->io > 0x3fffu || route->ram > 255u ||
        ((route->flags & 8u) && route->eram > 255u) || ((route->flags & 16u) && route->port > 255u))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t io_first = route->io << 6, page = configuration >> 12;
    if ((selector < io_first + 64u && io_first < selector + 5u) ||
        (configuration < io_first + 64u && io_first < configuration + 2u) || page == route->ram ||
        ((route->flags & 8u) && page == route->eram) ||
        ((route->flags & 16u) && page == route->port))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    return 0;
}

static int valid_page(uint32_t page)
{
    return !(page & 0x7ffu) &&
           (page <= 0x200000u - 0x800u || (page >= 0x08000000u && page <= 0x08080000u - 0x800u));
}

int hpclang_rpl_stock_bank_window_select(const uint32_t previous[64], const uint8_t owners[64],
                                         const struct hpclang_rpl_rom_bank *bank,
                                         uint32_t selected[64])
{
    if (!previous || !owners || !bank || !selected)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!bank->bytes || bank->bytes > 0x20000u || (bank->bytes & 1023u) ||
        (bank->physical & 0x7ffu) || bank->physical > 0x200000u - 0x20000u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t result[64];
    for (unsigned i = 0; i < 64; ++i) {
        result[i] = owners[i] ? previous[i] : bank->physical + i * 0x800u;
        if (!valid_page(result[i]))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    for (unsigned i = 0; i < 64; ++i)
        selected[i] = result[i];
    return 0;
}

int hpclang_rpl_stock_resolve_span(const uint32_t read_map[256], uint32_t address, uint32_t span,
                                   uint32_t *physical)
{
    if (!read_map || !physical)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!span || address >= 0x100000u || span > 0x100000u - address)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t first = address >> 12, last = (address + span - 1u) >> 12;
    uint32_t base = read_map[first];
    for (uint32_t i = first; i <= last; ++i) {
        uint32_t page = read_map[i];
        if (!valid_page(page))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (page != base + (i - first) * 0x800u)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    *physical = base * 2u + (address & 0xfffu);
    return 0;
}

int hpclang_rpl_stock_visit_spans(const uint32_t read_map[256], uint32_t address, uint32_t span,
                                  hpclang_rpl_stock_span_visit_fn visit, void *context)
{
    if (!read_map || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!span || address >= 0x100000u || span > 0x100000u - address)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t first = address >> 12, last = (address + span - 1u) >> 12;
    for (uint32_t page = first; page <= last; ++page)
        if (!valid_page(read_map[page]))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t end = address + span;
    while (address < end) {
        uint32_t page = address >> 12;
        uint32_t physical = read_map[page] * 2u + (address & 0xfffu);
        uint32_t stop = (page + 1u) << 12;
        while (stop < end && read_map[page + 1u] == read_map[page] + 0x800u) {
            ++page;
            stop += 0x1000u;
        }
        if (stop > end)
            stop = end;
        int result = visit(context, address, physical, stop - address);
        if (result)
            return result;
        address = stop;
    }
    return 0;
}

struct overlap_scan
{
    const uint32_t *right_map;
    uint32_t right_address, right_span, physical, span;
};

static int overlap_right(void *context, uint32_t logical, uint32_t physical, uint32_t span)
{
    (void)logical;
    const struct overlap_scan *scan = context;
    return physical < scan->physical + scan->span && scan->physical < physical + span;
}

static int overlap_left(void *context, uint32_t logical, uint32_t physical, uint32_t span)
{
    (void)logical;
    struct overlap_scan *scan = context;
    scan->physical = physical;
    scan->span = span;
    return hpclang_rpl_stock_visit_spans(scan->right_map, scan->right_address, scan->right_span,
                                         overlap_right, scan);
}

int hpclang_rpl_stock_spans_overlap(const uint32_t left_map[256], uint32_t left_address,
                                    uint32_t left_span, const uint32_t right_map[256],
                                    uint32_t right_address, uint32_t right_span)
{
    struct overlap_scan scan = {right_map, right_address, right_span, 0, 0};
    return hpclang_rpl_stock_visit_spans(left_map, left_address, left_span, overlap_left, &scan);
}

static int port1_read(void *context, uint32_t address, uint8_t *value)
{
    if (!context || !value || address >= HPCLANG_RPL_PORT1_NIBBLES)
        return 0;
    const volatile uint8_t *bytes = context;
    uint8_t packed = bytes[address / 2u];
    *value = (packed >> (4u * (address & 1u))) & 15u;
    return 1;
}

static int flash_read(void *context, uint32_t address, uint8_t *value)
{
    if (!context || !value || address >= HPCLANG_RPL_FLASH_BANK_NIBBLES)
        return 0;
    const volatile uint8_t *bytes = context;
    *value = (bytes[address / 2u] >> (4u * (address & 1u))) & 15u;
    return 1;
}

static int rom_bank_read(void *context, uint32_t address, uint8_t *value)
{
    const struct hpclang_rpl_stock_rom_bank *memory = context;
    if (!memory || !value || !memory->bytes || address < 0x40000u ||
        address - 0x40000u >= memory->size * 2u)
        return 0;
    uint32_t offset = address - 0x40000u;
    *value = (memory->bytes[offset / 2u] >> (4u * (offset & 1u))) & 15u;
    return 1;
}

int hpclang_rpl_stock_rom_bank_capture(struct hpclang_rpl_stock_rom_bank *memory,
                                       struct hpclang_rpl_reader *reader,
                                       const volatile uint8_t *bank_bytes,
                                       const struct hpclang_rpl_rom_bank *bank,
                                       const uint32_t flash_sections[2])
{
    if (!memory || !reader || !bank_bytes || !bank || !flash_sections || !bank->bytes ||
        bank->bytes > 0x20000u || (bank->physical & 1023u) || (bank->bytes & 1023u) ||
        bank->physical >= 0x200000u || bank->bytes > 0x200000u - bank->physical)
        return 0;
    uint32_t first = bank->physical >> 20;
    uint32_t last = (bank->physical + bank->bytes - 1u) >> 20;
    for (uint32_t section = first; section <= last; ++section)
        if ((flash_sections[section] & UINT32_C(0xfff00803)) != ((section << 20) | UINT32_C(0x802)))
            return 0;
    *memory = (struct hpclang_rpl_stock_rom_bank){bank_bytes, bank->bytes};
    *reader = (struct hpclang_rpl_reader){rom_bank_read, memory};
    return 1;
}

int hpclang_rpl_stock_rom_bank_reader(struct hpclang_rpl_stock_rom_bank *memory,
                                      struct hpclang_rpl_reader *reader,
                                      const struct hpclang_rpl_rom_bank *bank)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!memory || !reader || !bank || !boot || !boot->saturn_state ||
        (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G))
        return 0;
    volatile const uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    const uint32_t sections[] = {l1[0], l1[1]};
    return hpclang_rpl_stock_rom_bank_capture(
        memory, reader, (const void *)(uintptr_t)bank->physical, bank, sections);
}

int hpclang_rpl_stock_port2_capture(struct hpclang_rpl_reader *reader,
                                    const volatile uint8_t *bank_bytes, uint32_t flash_section)
{
    if (!reader || !bank_bytes || (flash_section & UINT32_C(0xfff00803)) != UINT32_C(0x00100802))
        return 0;
    *reader = (struct hpclang_rpl_reader){flash_read, (void *)(uintptr_t)bank_bytes};
    return 1;
}

int hpclang_rpl_stock_port2_reader(uint32_t bank, struct hpclang_rpl_reader *reader)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!reader || bank >= HPCLANG_RPL_PORT2_BANKS || !boot || !boot->saturn_state ||
        (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G))
        return 0;
    volatile const uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    return hpclang_rpl_stock_port2_capture(
        reader, (const void *)(uintptr_t)(0x00140000u + bank * 0x20000u), l1[1]);
}

int hpclang_rpl_stock_port1_capture(struct hpclang_rpl_reader *reader,
                                    const volatile uint8_t *bytes, uint32_t ram_section)
{
    if (!reader || !bytes || (ram_section & UINT32_C(0xfff00c03)) != UINT32_C(0x08000c02))
        return 0;
    *reader = (struct hpclang_rpl_reader){port1_read, (void *)(uintptr_t)bytes};
    return 1;
}

int hpclang_rpl_stock_port1_reader(struct hpclang_rpl_reader *reader)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!reader || !boot || !boot->saturn_state ||
        (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G))
        return 0;
    volatile const uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    return hpclang_rpl_stock_port1_capture(reader, (const void *)(uintptr_t)0x08020000u, l1[0x80]);
}

static int port1_write(void *context, uint32_t address, uint8_t value)
{
    if (!context || address >= HPCLANG_RPL_PORT1_NIBBLES || value > 15u)
        return 0;
    volatile uint8_t *bytes = context;
    uint8_t packed = bytes[address / 2u];
    bytes[address / 2u] = address & 1u ? (packed & 15u) | (value << 4u) : (packed & 0xf0u) | value;
    return 1;
}

int hpclang_rpl_stock_port1_mutation_io(struct hpclang_rpl_mutation_io *io, uint32_t *limit)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    const struct hpclang_rpl_memory_bounds *bounds = hpclang_launch_rpl_bounds(boot);
    struct hpclang_rpl_reader reader;
    if (!io || !limit || !bounds ||
        (bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC &&
         bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC_V4) ||
        bounds->port1_limit > HPCLANG_RPL_PORT1_NIBBLES || !hpclang_rpl_stock_port1_reader(&reader))
        return 0;
    *limit = hpclang_launch_port1_limit(boot);
    *io = (struct hpclang_rpl_mutation_io){
        .read_nibble = reader.read_nibble, .write_nibble = port1_write, .context = reader.context};
    return 1;
}

int hpclang_rpl_stock_memory_capture(struct hpclang_rpl_stock_memory *memory,
                                     const uint32_t *read_map, uint32_t ram_section)
{
    if (memory == NULL || read_map == NULL)
        return 0;
    /* Classic Stock main RAM uses the S3C2410 bank at 0x08000000. Require
       its identity section with user read/write access, not a ROM alias. */
    if ((ram_section & UINT32_C(0xfff00c03)) != UINT32_C(0x08000c02))
        return 0;
    for (uint32_t i = 0u; i < HPCLANG_RPL_MAIN_PAGES; ++i) {
        uint32_t physical;
        if (hpclang_rpl_stock_resolve_span(read_map, 0x80000u + i * 0x1000u, 0x1000u, &physical) ||
            physical < 0x10000000u)
            return 0;
        uint32_t base = physical / 2u;
        for (uint32_t j = 0u; j < i; ++j)
            if (memory->pages[j] == (uintptr_t)base)
                return 0;
        memory->pages[i] = (uintptr_t)base;
    }
    return 1;
}

static volatile uint8_t *mapped_byte(void *context, uint32_t address)
{
    if (context == NULL || address < 0x80000u || address >= 0x100000u)
        return NULL;
    const struct hpclang_rpl_stock_memory *memory = context;
    return (volatile uint8_t *)(memory->pages[(address >> 12) - 128u] + ((address & 0xfffu) >> 1));
}

int hpclang_rpl_stock_memory_read(void *context, uint32_t address, uint8_t *value)
{
    volatile uint8_t *p = mapped_byte(context, address);
    if (p == NULL || value == NULL)
        return 0;
    uint8_t packed = *p;
    *value = (address & 1u) != 0u ? (uint8_t)(packed >> 4) : (uint8_t)(packed & 15u);
    return 1;
}

int hpclang_rpl_stock_memory_write(void *context, uint32_t address, uint8_t value)
{
    volatile uint8_t *p = mapped_byte(context, address);
    if (p == NULL || value > 15u)
        return 0;
    uint8_t packed = *p;
    *p = (address & 1u) != 0u ? (uint8_t)((packed & 15u) | (uint8_t)(value << 4))
                              : (uint8_t)((packed & 0xf0u) | value);
    return 1;
}

int hpclang_rpl_stock_mutation_io(struct hpclang_rpl_mutation_io *io,
                                  struct hpclang_rpl_stock_memory *memory)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (io == NULL || memory == NULL)
        return 0;
    const struct hpclang_rpl_memory_bounds *bounds = hpclang_launch_rpl_bounds(boot);
    if (bounds == NULL)
        return 0;
    volatile const uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    if (!hpclang_rpl_stock_memory_capture(memory, (const void *)boot->saturn_state, l1[0x80]))
        return 0;
    /* Paired HP49g+/HP50g native library-registration captures relocate both
     * graph object bases and their pixel-data pointers outside SYSUPD. */
    static const uint32_t display_roots[] = {0x8068du, 0x80695u, 0x806d0u, 0x806d5u};
    int display_supported =
        boot->model == HPCLANG_MODEL_HP49GPLUS || boot->model == HPCLANG_MODEL_HP50G;
    *io = (struct hpclang_rpl_mutation_io){
        .read_nibble = hpclang_rpl_stock_memory_read,
        .write_nibble = hpclang_rpl_stock_memory_write,
        .context = memory,
        .temp_limit = bounds->temp_limit,
        .user_limit = bounds->user_limit,
        .stack_reserve_nibbles = bounds->stack_reserve_nibbles,
        .native_roots = bounds->native_roots,
        .native_root_count = bounds->native_root_count,
        .native_root_masks = hpclang_launch_native_root_masks(bounds),
        .library_table = boot->model >= HPCLANG_MODEL_HP48GII && boot->model <= HPCLANG_MODEL_HP50G
                             ? 0x8611du
                             : 0u,
        .display_roots = display_supported ? display_roots : NULL,
        .display_root_count =
            display_supported ? sizeof display_roots / sizeof display_roots[0] : 0,
    };
    return 1;
}
