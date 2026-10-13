#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include "display-internal.h"
#include "display-surface.h"

static uint32_t registers[25], original[25];
static struct hpclang_device_info device = {0};
static unsigned live, allocations, fail_allocation, clean_count, irq_depth;
static int invalid_second;
static unsigned tick, frozen, disabled, clock_error, frozen_line;
static uint64_t now;
static void *owned[4];

int hpclang_display_contrast_restore(void)
{
    return 0;
}

const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
int hpclang_time_read_us(uint64_t *value)
{
    if (clock_error) {
        errno = EIO;
        return -1;
    }
    if (!frozen)
        now += 10;
    *value = now;
    return 0;
}
uint32_t hpclang_irq_save_disable(void)
{
    assert(!irq_depth);
    irq_depth = 1;
    return 123;
}
void hpclang_irq_restore(uint32_t state)
{
    assert(irq_depth && state == 123);
    irq_depth = 0;
}
void hpclang_cache_clean(const void *address, uint32_t bytes)
{
    assert(address && bytes);
    ++clean_count;
}
static uint32_t read_reg(void *context, enum hpclang_lcd_reg reg)
{
    (void)context;
    if (reg == LCD_CONTROL1) {
        uint32_t line = frozen_line ? 3u : 68u - (tick++ % 69u);
        return (registers[0] & 0x3ffffu & (disabled ? ~1u : ~0u)) | (line << 18);
    }
    return registers[reg];
}
static void write_reg(void *context, enum hpclang_lcd_reg reg, uint32_t value)
{
    (void)context;
    assert(irq_depth);
    if (reg != LCD_CONTROL1)
        assert(!(registers[0] & 1u));
    registers[reg] = value;
}
static void *allocate(void *context, size_t bytes)
{
    (void)context;
    if (++allocations == fail_allocation)
        return NULL;
    void *p = malloc(bytes);
    assert(p && live < 4);
    for (unsigned i = 0; i < 4; ++i)
        if (!owned[i]) {
            owned[i] = p;
            break;
        }
    ++live;
    return p;
}
static void release(void *context, void *p)
{
    (void)context;
    /* No page can be freed while the LCD still uses it. */
    for (unsigned i = 0; i < 4; ++i)
        if (owned[i] == p) {
            assert(registers[5] != (0x08004000u + i * 0x4000u) / 2);
            owned[i] = NULL;
            --live;
            free(p);
            return;
        }
    assert(0);
}
static int translate(void *context, const void *p, uint32_t bytes, uint32_t *physical)
{
    (void)context;
    assert(bytes && p);
    unsigned index = 0;
    while (index < 4 && owned[index] != p)
        ++index;
    assert(index < 4);
    *physical = 0x08004000u + index * 0x4000u;
    if (live == 2 && invalid_second)
        *physical = 0x083ffffcu;
    return 0;
}
const struct hpclang_lcd_bus hpclang_display_bus = {read_reg, write_reg, NULL};
const struct hpclang_dma_allocator hpclang_display_allocator = {allocate, release, translate, NULL};

int main(void)
{
    assert(hpclang_display_begin(3) == -1 && errno == EINVAL);
    assert(hpclang_display_begin(4) == -1 && errno == ENOSYS);
    assert(hpclang_display_present(1000) == -1 && errno == EPERM);
    assert(hpclang_display_set_enabled(0, 1000) == -1 && errno == EPERM);
    assert(hpclang_display_set_enabled(2, 1000) == -1 && errno == EINVAL);
    device.family = HPCLANG_FAMILY_HP39;
    device.display_width = 131;
    device.display_height = 64;
    registers[0] = (50u << 8) | 0x21u;
    registers[1] = 68u << 14;
    registers[2] = 39u << 8;
    registers[4] = 0x302;
    registers[5] = 0x20000400;
    registers[6] = 0x6b2;
    registers[7] = 10;
    memcpy(original, registers, sizeof original);
    invalid_second = 1;
    assert(hpclang_display_begin(16) == -1 && errno == ERANGE && !live);
    assert(memcmp(registers, original, sizeof original) == 0);
    invalid_second = 0;
    for (uint32_t levels = 2; levels <= 16; levels = levels == 2 ? 4 : 16) {
        fail_allocation = allocations + 2;
        assert(hpclang_display_begin(levels) == -1 && errno == ENOMEM && !live);
        assert(memcmp(registers, original, sizeof original) == 0);
        fail_allocation = 0;
        assert(hpclang_display_begin(levels) == 0 && live == 2);
        assert(hpclang_display_begin(levels) == -1 && errno == EBUSY && live == 2);
        struct hpclang_surface *back = hpclang_display_back();
        assert(back && back->levels == levels);
        hpclang_surface_clear(back, 85);
        assert(hpclang_display_indicator_set(6, 255) == -1 && errno == EINVAL);
        assert(hpclang_display_indicator_set(5, 255) == 0);
        struct hpclang_surface status = *back;
        status.width = 132;
        assert(hpclang_surface_get_pixel(&status, 131, 5) == 255);
        assert(hpclang_surface_get_pixel(&status, 131, 4) == 0);
        uint32_t address = registers[5];
        disabled = 1;
        assert(hpclang_display_present(1000) == -1 && errno == ENODEV);
        disabled = 0;
        assert(registers[5] == address && hpclang_display_back() == back);
        assert(hpclang_display_present(1) == -1 && errno == ETIMEDOUT);
        assert(hpclang_display_present(0) == -1 && errno == EINVAL);
        assert(hpclang_display_present(100000) == 0);
        assert(registers[5] != address && hpclang_display_back() != back);
        assert(hpclang_surface_get_pixel(hpclang_display_back(), 0, 0) == 0);
        assert(hpclang_display_present(100000) == 0 && hpclang_display_back() == back);
        assert(hpclang_display_set_enabled(0, 0) == -1 && errno == EINVAL);
        assert(hpclang_display_set_enabled(0, 1) == -1 && errno == ETIMEDOUT);
        assert(registers[0] & 1u);
        address = registers[5];
        assert(hpclang_display_set_enabled(0, 100000) == 0 && !(registers[0] & 1u));
        assert(hpclang_display_set_enabled(0, 100000) == 0);
        assert(hpclang_display_present(100000) == -1 && errno == ENODEV);
        assert(registers[5] == address && hpclang_display_back() == back);
        assert(hpclang_display_set_enabled(1, 100000) == 0 && (registers[0] & 1u));
        assert(hpclang_display_set_enabled(1, 100000) == 0);
        assert(hpclang_display_set_enabled(0, 100000) == 0);
        hpclang_display_end();
        hpclang_display_end();
        assert(!live && !hpclang_display_back());
        assert(memcmp(registers, original, sizeof original) == 0);
        if (levels == 16)
            break;
    }
    assert(clean_count >= 6);
    assert(hpclang_display_begin_pages(2, 0) == -1 && errno == EINVAL);
    assert(hpclang_display_begin_pages(2, 3) == -1 && errno == EINVAL);
    assert(hpclang_display_publish() == -1 && errno == EPERM);
    assert(hpclang_display_replace_pages(2, 1) == -1 && errno == EPERM);
    for (unsigned levels = 2; levels <= 16; levels = levels == 2 ? 4 : 16) {
        fail_allocation = allocations + 1;
        assert(hpclang_display_begin_pages(levels, 1) == -1 && errno == ENOMEM && !live);
        assert(memcmp(registers, original, sizeof original) == 0);
        fail_allocation = 0;
        assert(hpclang_display_begin_pages(levels, 1) == 0 && live == 1);
        struct hpclang_surface *front = hpclang_display_back();
        assert(front && front->levels == levels);
        uint32_t address = registers[5];
        unsigned cleaned = clean_count;
        hpclang_surface_set_pixel(front, 1, 1, 255);
        assert(hpclang_display_publish() == 0 && clean_count == cleaned + 1);
        assert(registers[5] == address && hpclang_display_back() == front);
        assert(hpclang_display_present(100000) == 0);
        assert(registers[5] == address && hpclang_display_back() == front);
        assert(hpclang_surface_get_pixel(front, 1, 1) == 255);
        assert(hpclang_display_indicator_set(3, 255) == 0);
        assert(hpclang_display_publish() == 0);
        uint32_t before[25];
        memcpy(before, registers, sizeof before);
        fail_allocation = allocations + 2;
        assert(hpclang_display_replace_pages(16, 2) == -1 && errno == ENOMEM);
        assert(live == 1 && hpclang_display_back() == front);
        assert(hpclang_surface_get_pixel(front, 1, 1) == 255);
        assert(memcmp(before, registers, sizeof before) == 0);
        fail_allocation = 0;
        assert(hpclang_display_replace_pages(16, 2) == 0 && live == 2);
        assert(hpclang_display_back()->levels == 16);
        assert(hpclang_display_present(100000) == 0);
        assert(hpclang_display_replace_pages(2, 1) == 0 && live == 1);
        assert(hpclang_display_back()->levels == 2);
        hpclang_display_end();
        assert(!live && !hpclang_display_back());
        assert(memcmp(registers, original, sizeof original) == 0);
        if (levels == 16)
            break;
    }
    clock_error = 1;
    assert(hpclang_display_wait_frame(1000) == -1 && errno == EIO);
    clock_error = 0;
    frozen = 1;
    /* A fixed frame counter plus a stopped clock must not hang. */
    frozen_line = 1;
    assert(hpclang_display_wait_frame(1000) == -1 && errno == EIO);
    assert(hpclang_display_indicator_set(0, 255) == -1 && errno == EPERM);
    return 0;
}
