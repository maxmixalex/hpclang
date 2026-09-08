#include <stdint.h>

#include <hpclang/device.h>
#include <hpclang/display.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define LCDSADDR1 REG32(0x07300014u)

#define LCD_RAM_BASE UINT32_C(0x07f00000)
#define LCD_STRIDE_BYTES 20u
#define LCD_LEVELS 2u

struct hpclang_surface {
    volatile uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t levels;
};

static struct hpclang_surface display;

static int black(uint8_t intensity)
{
    return intensity >= 128u;
}

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

uint32_t hpclang_surface_width(const struct hpclang_surface *surface)
{
    return surface->width;
}

uint32_t hpclang_surface_height(const struct hpclang_surface *surface)
{
    return surface->height;
}

uint32_t hpclang_surface_levels(const struct hpclang_surface *surface)
{
    return surface->levels;
}

void hpclang_surface_clear(
    struct hpclang_surface *surface,
    uint8_t intensity)
{
    uint32_t full_bytes = surface->width >> 3;
    uint32_t tail_bits = surface->width & 7u;
    uint8_t value = black(intensity) ? UINT8_C(0xff) : 0u;

    for (uint32_t y = 0u; y < surface->height; ++y) {
        volatile uint8_t *row = surface->pixels + y * surface->stride;

        for (uint32_t x = 0u; x < full_bytes; ++x)
            row[x] = value;

        if (tail_bits != 0u) {
            uint8_t mask = (uint8_t)((1u << tail_bits) - 1u);
            row[full_bytes] =
                (uint8_t)((row[full_bytes] & (uint8_t)~mask) |
                          (value & mask));
        }
    }
}

void hpclang_surface_set_pixel(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint8_t intensity)
{
    if (x < 0 || y < 0 ||
        (uint32_t)x >= surface->width ||
        (uint32_t)y >= surface->height)
        return;

    volatile uint8_t *pixel =
        surface->pixels + (uint32_t)y * surface->stride + ((uint32_t)x >> 3);
    uint8_t mask = (uint8_t)(1u << ((uint32_t)x & 7u));

    if (black(intensity))
        *pixel |= mask;
    else
        *pixel &= (uint8_t)~mask;
}

void hpclang_surface_draw_line(
    struct hpclang_surface *surface,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    uint8_t intensity)
{
    int32_t dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy_abs = y1 >= y0 ? y1 - y0 : y0 - y1;
    int32_t dy = -dy_abs;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;

    for (;;) {
        hpclang_surface_set_pixel(surface, x0, y0, intensity);
        if (x0 == x1 && y0 == y1)
            return;

        int32_t twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void hpclang_surface_fill_rect(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint8_t intensity)
{
    if (width <= 0 || height <= 0)
        return;

    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (width <= 0 || height <= 0 ||
        (uint32_t)x >= surface->width ||
        (uint32_t)y >= surface->height)
        return;

    if ((uint32_t)width > surface->width - (uint32_t)x)
        width = (int32_t)(surface->width - (uint32_t)x);
    if ((uint32_t)height > surface->height - (uint32_t)y)
        height = (int32_t)(surface->height - (uint32_t)y);

    for (int32_t yy = 0; yy < height; ++yy)
        for (int32_t xx = 0; xx < width; ++xx)
            hpclang_surface_set_pixel(
                surface, x + xx, y + yy, intensity);
}
