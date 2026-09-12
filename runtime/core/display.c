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
    uint32_t lcdsaddr1 = LCDSADDR1;
    uintptr_t pixels =
        ((lcdsaddr1 & UINT32_C(0xffff)) * 2u) + LCD_RAM_BASE;

    display.pixels = (volatile uint8_t *)pixels;
    display.width = device->display_width;
    display.height = device->display_height;
    display.stride = LCD_STRIDE_BYTES;
    display.levels = LCD_LEVELS;

    /*
     * TEMPORARY HP39 Gallery diagnostic. The bootstrap leaves its selected
     * physical-NOR alias VA in boot_info.platform_cookie. Record the active
     * LCD buffer interval and the corresponding L1 word before the first
     * Gallery clear/draw. The x50ng diagnostic frontend already prints these
     * five private boot-page words on failure.
     *
     * This tests whether the stock HP39 LCD framebuffer occupies bytes inside
     * the active 16 KiB L1 table. If so, ordinary surface drawing can overwrite
     * an otherwise-empty L1 descriptor used as a long-lived NOR alias.
     */
    volatile uint32_t *boot =
        (volatile uint32_t *)(uintptr_t)UINT32_C(0x01100000);
    uint32_t model = boot[0];
    if (model == 2u || model == 3u) {
        uint32_t cookie = boot[3];
        uint32_t index = cookie >> 20;
        volatile uint32_t *diag =
            (volatile uint32_t *)(uintptr_t)UINT32_C(0x01100020);
        volatile uint32_t *l1 =
            (volatile uint32_t *)(uintptr_t)LCD_RAM_BASE;

        diag[0] = UINT32_C(0x46425031); /* "FBP1" */
        diag[1] = lcdsaddr1;
        diag[2] = (uint32_t)pixels;
        diag[3] = (uint32_t)(
            pixels + (uintptr_t)LCD_STRIDE_BYTES * device->display_height);
        diag[4] = index < 4096u ? l1[index] : UINT32_C(0xffffffff);
    }

    return &display;
}
