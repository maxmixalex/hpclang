#include <errno.h>
#include <stdint.h>
#include <hpclang/device.h>
#include <hpclang/power.h>
#include <hpclang/display.h>
#include "clock-internal.h"
#include "display-internal.h"
#include "power-clock.h"

#define POWER_REG(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define POWER_PLL POWER_REG(0x07200004u)
#define POWER_SLOW POWER_REG(0x07200010u)
#define POWER_DIV POWER_REG(0x07200014u)

static struct
{
    uint32_t slow, pll, div, lcd_clock;
    int owned;
} profile;

static int power_supported(void)
{
    if (hpclang_device_info()->family != HPCLANG_FAMILY_UNKNOWN)
        return 1;
    errno = ENOSYS;
    return 0;
}

int hpclang_power_get_profile(void)
{
    if (!power_supported())
        return -1;
    return profile.owned ? HPCLANG_POWER_ECONOMY : HPCLANG_POWER_HOST;
}

int hpclang_power_cpu_frequency_hz(uint32_t *frequency)
{
    if (!frequency) {
        errno = EINVAL;
        return -1;
    }
    if (!power_supported())
        return -1;
    *frequency = hpclang_power_clock_hz(POWER_PLL, POWER_SLOW);
    return 0;
}

static void apply_clock(uint32_t slow, uint32_t lcd)
{
    const struct hpclang_lcd_bus *bus = &hpclang_display_bus;
    uint32_t current = bus->read(bus->context, LCD_CONTROL1) & 0x3ffffu;
    bus->write(bus->context, LCD_CONTROL1, current & ~1u);
    POWER_SLOW = slow;
    bus->write(bus->context, LCD_CONTROL1, lcd);
    hpclang_display_reclock_saved(lcd);
}

int hpclang_power_set_profile(enum hpclang_power_profile requested)
{
    if (requested != HPCLANG_POWER_HOST && requested != HPCLANG_POWER_ECONOMY) {
        errno = EINVAL;
        return -1;
    }
    if (!power_supported())
        return -1;
    if ((requested == HPCLANG_POWER_ECONOMY) == profile.owned)
        return 0;
    if (hpclang_sound_active()) {
        errno = EBUSY;
        return -1;
    }
    uint32_t slow = POWER_SLOW, pll = POWER_PLL, div = POWER_DIV;
    const struct hpclang_lcd_bus *bus = &hpclang_display_bus;
    uint32_t lcd = bus->read(bus->context, LCD_CONTROL1) & 0x3ffffu;
    uint32_t next_slow, next_lcd;
    if (requested == HPCLANG_POWER_ECONOMY) {
        if ((div & ~3u) || ((slow & 0x20u) && !(slow & 0x10u))) {
            errno = ENOTSUP;
            return -1;
        }
        if (hpclang_power_economy_clock(pll, slow, lcd, &next_slow, &next_lcd))
            return -1;
    } else {
        if (pll != profile.pll || div != profile.div) {
            errno = EBUSY;
            return -1;
        }
        next_slow = profile.slow;
        next_lcd = (lcd & ~0x3ff00u) | profile.lcd_clock;
    }
    if ((lcd & 1u) && hpclang_display_wait_frame(200000u))
        return -1;
    uint32_t irq = hpclang_irq_save_disable();
    struct hpclang_clock_epoch epoch;
    if (hpclang_time_clock_prepare(&epoch)) {
        hpclang_irq_restore(irq);
        return -1;
    }
    apply_clock(next_slow, next_lcd);
    hpclang_time_clock_commit(&epoch);
    if (requested == HPCLANG_POWER_ECONOMY) {
        profile.slow = slow;
        profile.pll = pll;
        profile.div = div;
        profile.lcd_clock = lcd & 0x3ff00u;
        profile.owned = 1;
    } else {
        profile.owned = 0;
    }
    hpclang_irq_restore(irq);
    return 0;
}

void hpclang_power_restore(void)
{
    if (!profile.owned)
        return;
    int saved_errno = errno;
    if (hpclang_power_set_profile(HPCLANG_POWER_HOST)) {
        /* Exit must restore the borrowed clock even if RTC/frame observation
         * fails. All display sessions have already ended at this boundary. */
        uint32_t irq = hpclang_irq_save_disable();
        uint32_t lcd = hpclang_display_bus.read(hpclang_display_bus.context, LCD_CONTROL1);
        apply_clock(profile.slow, (lcd & 0xffu) | profile.lcd_clock);
        profile.owned = 0;
        hpclang_irq_restore(irq);
    }
    errno = saved_errno;
}
