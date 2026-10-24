#include <errno.h>
#include <hpclang/device.h>
#include <hpclang/sound.h>
#include "clock-internal.h"
#include "exit-hooks.h"
#include "sound-session.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))
static const uintptr_t audio_regs[] = {0x07600000, 0x07600004, 0x07600008, 0x07600024, 0x07600028,
                                       0x0760002c, 0x07a00010, 0x07100008, 0x07100000, 0x07100010};
static const uintptr_t watchdog_regs[] = {0x07800000, 0x07800004, 0x07800008,
                                          0x07100008, 0x07100000, 0x07100010};
static const uintptr_t irq_regs[] = {0x07100008, 0x07100004, 0x07100010, 0x08000018};
static uint32_t audio_read(void *ctx, enum hpclang_sound_reg reg)
{
    (void)ctx;
    return REG(audio_regs[reg]);
}
static void audio_write(void *ctx, enum hpclang_sound_reg reg, uint32_t value)
{
    (void)ctx;
    REG(audio_regs[reg]) = value;
}
static uint32_t watchdog_read(void *ctx, enum hpclang_watchdog_reg reg)
{
    (void)ctx;
    return REG(watchdog_regs[reg]);
}
static void watchdog_write(void *ctx, enum hpclang_watchdog_reg reg, uint32_t value)
{
    (void)ctx;
    REG(watchdog_regs[reg]) = value;
}
static uint32_t irq_read(void *ctx, enum hpclang_irq_owner_reg reg)
{
    (void)ctx;
    return REG(irq_regs[reg]);
}
static void irq_write(void *ctx, enum hpclang_irq_owner_reg reg, uint32_t value)
{
    (void)ctx;
    REG(irq_regs[reg]) = value;
}
static void exchange(void *ctx, struct hpclang_irq_bank *bank)
{
    (void)ctx;
    hpclang_irq_bank_exchange(bank);
}
static const struct hpclang_sound_session_bus bus = {{audio_read, audio_write, 0},
                                                     {0, watchdog_read, watchdog_write},
                                                     {0, irq_read, irq_write, exchange}};
static struct hpclang_sound_session session;
static uint32_t stack[256] __attribute__((aligned(8)));
static uint32_t original_masks;
void hpclang_sound_irq_entry(void);

static int active(void)
{
    return session.irq.owned;
}
static uint32_t collect(uint32_t current)
{
    if (session.irq.owned) {
        hpclang_sound_session_close(&bus, &session);
        current = (current & ~0xc0u) | original_masks;
    }
    hpclang_exit_hooks.sound_async = 0;
    hpclang_exit_hooks.sound_async_active = 0;
    return current;
}
void hpclang_sound_stop(void)
{
    uint32_t current = hpclang_irq_save_disable();
    hpclang_irq_restore(collect(current));
}
int hpclang_sound_status(void)
{
    uint32_t current = hpclang_irq_save_disable();
    enum hpclang_sound_session_state state = session.state;
    int error = session.error;
    if (state != SOUND_SESSION_RUNNING)
        current = collect(current);
    hpclang_irq_restore(current);
    if (state == SOUND_SESSION_FAILED) {
        errno = error;
        return -1;
    }
    return state == SOUND_SESSION_RUNNING;
}
int hpclang_sound_start(uint32_t frequency, uint32_t duration_ms, int replace)
{
    if (!frequency || !duration_ms) {
        errno = EINVAL;
        return -1;
    }
    if (hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN) {
        errno = ENOSYS;
        return -1;
    }
    uint32_t current = hpclang_irq_save_disable();
    int error = 0;
    if ((!session.irq.owned && !(current & 0x80u)) || hpclang_sound_blocking_active() ||
        (session.state == SOUND_SESSION_RUNNING && !replace)) {
        error = EBUSY;
        goto fail;
    }
    if ((REG(0x07f001e0) & 0xfff00003u) != 0x53000002u) {
        error = ENOSYS;
        goto fail;
    }
    uint64_t now, num, den;
    if (hpclang_time_read_us_masked(&now)) {
        error = errno;
        goto fail;
    }
    uint64_t duration = (uint64_t)duration_ms * 1000;
    if (now > UINT64_MAX - duration) {
        error = EOVERFLOW;
        goto fail;
    }
    hpclang_clock_pclk(&num, &den);
    uint32_t baseline = session.irq.owned ? original_masks : current & 0xc0u;
    if (hpclang_sound_session_start(&bus, &session, num, den, frequency, now, now + duration,
                                    (uintptr_t)(stack + 256), (uintptr_t)hpclang_sound_irq_entry,
                                    replace)) {
        error = errno;
        if (!session.irq.owned) {
            current = (current & ~0xc0u) | baseline;
            hpclang_exit_hooks.sound_async = 0;
            hpclang_exit_hooks.sound_async_active = 0;
        }
        goto fail;
    }
    original_masks = baseline;
    hpclang_exit_hooks.sound_async = hpclang_sound_stop;
    hpclang_exit_hooks.sound_async_active = active;
    /* Start the requested interval after configuring PWM. */
    if (hpclang_time_read_us_masked(&now) || now > UINT64_MAX - duration) {
        error = now > UINT64_MAX - duration ? EOVERFLOW : errno;
        current = collect(current);
        goto fail;
    }
    session.last_time = now;
    session.deadline = now + duration;
    if (hpclang_watchdog_advance(&bus.watchdog, &session.alarm, now, session.deadline)) {
        error = errno;
        current = collect(current);
        goto fail;
    }
    hpclang_irq_restore((current | 0x40u) & ~0x80u);
    return 0;
fail:
    hpclang_irq_restore(current);
    errno = error;
    return -1;
}
void hpclang_sound_irq_dispatch(void)
{
    int saved_errno = errno;
    uint64_t now = 0;
    int error = hpclang_time_read_us_masked(&now) ? EIO : 0;
    hpclang_sound_session_advance(&bus, &session, now, error);
    errno = saved_errno;
}

void hpclang_sound_irq_fault(void)
{
    int saved_errno = errno;
    hpclang_sound_session_advance(&bus, &session, 0, EIO);
    errno = saved_errno;
}
