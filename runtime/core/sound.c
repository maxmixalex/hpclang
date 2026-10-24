#include <errno.h>
#include <hpclang/device.h>
#include <hpclang/sound.h>
#include <hpclang/time.h>
#include "clock-internal.h"
#include "sound-engine.h"
#include "exit-hooks.h"

static const uintptr_t registers[] = {0x07600000, 0x07600004, 0x07600008, 0x07600024, 0x07600028,
                                      0x0760002c, 0x07a00010, 0x07100008, 0x07100000, 0x07100010};
static uint32_t read_reg(void *context, enum hpclang_sound_reg reg)
{
    (void)context;
    return *(volatile uint32_t *)registers[reg];
}
static void write_reg(void *context, enum hpclang_sound_reg reg, uint32_t value)
{
    (void)context;
    *(volatile uint32_t *)registers[reg] = value;
}
static const struct hpclang_sound_bus bus = {read_reg, write_reg, 0};
static struct hpclang_sound_saved saved;

int hpclang_sound_active(void)
{
    return saved.owned ||
           (hpclang_exit_hooks.sound_async_active && hpclang_exit_hooks.sound_async_active());
}

int hpclang_sound_blocking_active(void)
{
    return saved.owned;
}

void hpclang_sound_restore(void)
{
    if (hpclang_exit_hooks.sound_async)
        hpclang_exit_hooks.sound_async();
    hpclang_sound_release(&bus, &saved);
}

int hpclang_sound_play(uint32_t frequency_hz, uint32_t duration_ms, hpclang_sound_cancel_fn cancel,
                       void *context)
{
    if (!frequency_hz) {
        errno = EINVAL;
        return -1;
    }
    if (!duration_ms)
        return 0;
    if (hpclang_sound_active()) {
        errno = EBUSY;
        return -1;
    }
    if (hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN) {
        errno = ENOSYS;
        return -1;
    }
    uint64_t now;
    if (hpclang_time_read_us(&now) < 0)
        return -1;
    uint64_t duration = (uint64_t)duration_ms * 1000;
    if (duration > UINT64_MAX - now) {
        errno = EOVERFLOW;
        return -1;
    }
    uint64_t numerator, denominator;
    hpclang_clock_pclk(&numerator, &denominator);
    if (hpclang_sound_acquire(&bus, &saved, numerator, denominator, frequency_hz) < 0)
        return -1;
    /* Measure duration from running PWM, excluding frequency setup work. */
    if (hpclang_time_read_us(&now) < 0) {
        hpclang_sound_release(&bus, &saved);
        return -1;
    }
    if (duration > UINT64_MAX - now) {
        hpclang_sound_release(&bus, &saved);
        errno = EOVERFLOW;
        return -1;
    }
    uint64_t deadline = now + duration;
    int result = 0;
    for (;;) {
        if (cancel && cancel(context)) {
            result = 1;
            break;
        }
        if (hpclang_time_read_us(&now) < 0) {
            result = -1;
            break;
        }
        if (now >= deadline)
            break;
    }
    hpclang_sound_release(&bus, &saved);
    return result;
}
