#include <errno.h>
#include <stdlib.h>
#include <hpclang/bootstrap.h>
#include "display-internal.h"
#include "dma-span.h"

static void *allocate(void *context, size_t bytes)
{
    (void)context;
    return malloc(bytes);
}

static void release(void *context, void *allocation)
{
    (void)context;
    free(allocation);
}

static int translate(void *context, const void *pixels, uint32_t bytes, uint32_t *physical)
{
    (void)context;
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    uintptr_t address = (uintptr_t)pixels;
    if (!boot || address < HPCLANG_BOOT_BASE || address - HPCLANG_BOOT_BASE > UINT32_MAX) {
        errno = EFAULT;
        return -1;
    }
    return hpclang_dma_span((const void *)(uintptr_t)HPCLANG_PAGE_TABLE_BASE,
                            boot->rw_bytes / HPCLANG_PAGE_SIZE,
                            (uint32_t)(address - HPCLANG_BOOT_BASE), bytes, 4, physical);
}

static uint32_t read_reg(void *context, enum hpclang_lcd_reg reg)
{
    (void)context;
    return ((volatile uint32_t *)0x07300000)[reg];
}

static void write_reg(void *context, enum hpclang_lcd_reg reg, uint32_t value)
{
    (void)context;
    ((volatile uint32_t *)0x07300000)[reg] = value;
}

const struct hpclang_lcd_bus hpclang_display_bus = {read_reg, write_reg, NULL};
const struct hpclang_dma_allocator hpclang_display_allocator = {allocate, release, translate, NULL};
