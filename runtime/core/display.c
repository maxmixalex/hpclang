#include <stdint.h>

#include <hpclang/device.h>
#include <hpclang/display.h>

#include "display-surface.h"

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define LCDSADDR1 REG32(0x07300014u)

#define LCD_RAM_BASE UINT32_C(0x07f00000)
#define LCD_STRIDE_BYTES 20u
#define LCD_LEVELS 2u

static struct hpclang_surface display;

struct hpclang_surface *hpclang_display_surface(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();

    display.pixels = (volatile uint8_t *)(uintptr_t)(
        ((LCDSADDR1 & UINT32_C(0xffff)) * 2u) + LCD_RAM_BASE);
    display.width = device->display_width;
    display.height = device->display_height;
    display.stride = LCD_STRIDE_BYTES;
    display.levels = LCD_LEVELS;
    return &display;
}
