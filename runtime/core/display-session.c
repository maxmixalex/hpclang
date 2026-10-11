#include <errno.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/time.h>
#include "display-internal.h"
#include "display-surface.h"

static struct hpclang_lcd_state saved;
static struct hpclang_dma_buffer pages[2];
static struct hpclang_surface surfaces[2];
static unsigned back;

struct hpclang_surface *hpclang_display_back(void)
{
    return saved.owned ? &surfaces[back] : 0;
}

int hpclang_display_begin(uint32_t levels)
{
    if (levels != 2 && levels != 4 && levels != 16) {
        errno = EINVAL;
        return -1;
    }
    if (saved.owned) {
        errno = EBUSY;
        return -1;
    }
    const struct hpclang_device_info *device = hpclang_device_info();
    if (!device->family) {
        errno = ENOSYS;
        return -1;
    }
    const struct hpclang_lcd_bus *bus = &hpclang_display_bus;
    uint32_t rows = ((bus->read(bus->context, LCD_CONTROL2) >> 14) & 1023u) + 1u;
    uint32_t scan_width = (((bus->read(bus->context, LCD_CONTROL3) >> 8) & 2047u) + 1u) * 4u;
    uint32_t bits = levels == 2 ? 1u : levels == 4 ? 2u : 4u;
    uint32_t stride = (scan_width * bits + 7u) / 8u;
    for (unsigned i = 0; i < 2; ++i) {
        if (hpclang_dma_buffer_acquire(&hpclang_display_allocator, rows * stride, 4, &pages[i]))
            goto fail;
        if (hpclang_lcd_buffer_validate(pages[i].physical, pages[i].bytes))
            goto fail;
        surfaces[i] = (struct hpclang_surface){pages[i].pixels, device->display_width,
                                               device->display_height, stride, levels};
        hpclang_cache_clean(pages[i].pixels, pages[i].bytes);
    }
    struct hpclang_lcd_layout layout;
    uint32_t irq = hpclang_irq_save_disable();
    int result = hpclang_lcd_acquire(bus, &saved, device->display_width, device->display_height,
                                     levels, pages[0].physical, pages[0].bytes, &layout);
    hpclang_irq_restore(irq);
    if (result)
        goto fail;
    back = 1;
    return 0;
fail: {
    int error = errno;
    hpclang_dma_buffer_release(&pages[1]);
    hpclang_dma_buffer_release(&pages[0]);
    errno = error;
    return -1;
}
}

int hpclang_display_wait_frame(uint32_t timeout_us)
{
    if (!hpclang_device_info()->family) {
        errno = ENOSYS;
        return -1;
    }
    return hpclang_lcd_wait_frame(&hpclang_display_bus, hpclang_time_read_us, timeout_us);
}

int hpclang_display_present(uint32_t timeout_us)
{
    if (!saved.owned) {
        errno = EPERM;
        return -1;
    }
    hpclang_cache_clean(pages[back].pixels, pages[back].bytes);
    if (hpclang_display_wait_frame(timeout_us))
        return -1;
    /* Stop DMA rather than updating live addresses with stale FIFO contents.
     * Serialize the entire register transaction, not the potentially long wait. */
    struct hpclang_lcd_state previous = {0};
    struct hpclang_lcd_layout layout;
    struct hpclang_surface *surface = &surfaces[back];
    uint32_t irq = hpclang_irq_save_disable();
    int result =
        hpclang_lcd_acquire(&hpclang_display_bus, &previous, surface->width, surface->height,
                            surface->levels, pages[back].physical, pages[back].bytes, &layout);
    hpclang_irq_restore(irq);
    if (!result)
        back ^= 1u;
    return result;
}

int hpclang_display_set_enabled(int enabled, uint32_t timeout_us)
{
    if ((enabled != 0 && enabled != 1) || !timeout_us || timeout_us > 1000000u) {
        errno = EINVAL;
        return -1;
    }
    if (!saved.owned) {
        errno = EPERM;
        return -1;
    }
    const struct hpclang_lcd_bus *bus = &hpclang_display_bus;
    uint32_t control = bus->read(bus->context, LCD_CONTROL1);
    if ((control & 1u) == (unsigned)enabled)
        return 0;
    if (!enabled && hpclang_display_wait_frame(timeout_us))
        return -1;
    uint32_t irq = hpclang_irq_save_disable();
    control = bus->read(bus->context, LCD_CONTROL1);
    bus->write(bus->context, LCD_CONTROL1, (control & 0x3fffeu) | (unsigned)enabled);
    hpclang_irq_restore(irq);
    return 0;
}

int hpclang_display_indicator_set(unsigned segment, uint8_t intensity)
{
    if (segment >= 6u) {
        errno = EINVAL;
        return -1;
    }
    if (!saved.owned) {
        errno = EPERM;
        return -1;
    }
    struct hpclang_surface status = surfaces[back];
    uint32_t bits = status.levels == 2 ? 1u : status.levels == 4 ? 2u : 4u;
    if (status.stride * 8u / bits <= status.width || status.height <= segment) {
        errno = ENOTSUP;
        return -1;
    }
    ++status.width;
    hpclang_surface_set_pixel(&status, (int32_t)status.width - 1, (int32_t)segment, intensity);
    return 0;
}

void hpclang_display_end(void)
{
    (void)hpclang_display_contrast_restore();
    if (!saved.owned)
        return;
    uint32_t irq = hpclang_irq_save_disable();
    hpclang_lcd_release(&hpclang_display_bus, &saved);
    hpclang_irq_restore(irq);
    hpclang_dma_buffer_release(&pages[1]);
    hpclang_dma_buffer_release(&pages[0]);
    surfaces[0] = surfaces[1] = (struct hpclang_surface){0};
}
