#include <stddef.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
#include "rpl-stock-memory.h"
#include "launch-services.h"

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
        uint32_t base = read_map[128u + i];
        if ((base & UINT32_C(0xfff007ff)) != UINT32_C(0x08000000))
            return 0;
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
    *io = (struct hpclang_rpl_mutation_io){
        .read_nibble = hpclang_rpl_stock_memory_read,
        .write_nibble = hpclang_rpl_stock_memory_write,
        .context = memory,
        .temp_limit = bounds->temp_limit,
        .user_limit = bounds->user_limit,
        .stack_reserve_nibbles = bounds->stack_reserve_nibbles,
        .native_roots = bounds->native_roots,
        .native_root_count = bounds->native_root_count,
        .library_table = boot->model >= HPCLANG_MODEL_HP48GII && boot->model <= HPCLANG_MODEL_HP50G
                             ? 0x8611du
                             : 0u,
    };
    return 1;
}
