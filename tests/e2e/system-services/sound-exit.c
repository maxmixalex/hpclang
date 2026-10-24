#include <stdint.h>
#include <stdlib.h>
#include <kos.h>
#include <hpclang/sound.h>
#include "exit-hooks.h"
#include "irq.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))
static const uintptr_t addresses[] = {0x07600000, 0x07600004, 0x07600008, 0x07600024, 0x07600028,
                                      0x0760002c, 0x07a00010, 0x07800000, 0x07800004, 0x07800008,
                                      0x07100008, 0x07100004, 0x08000018, 0x08000008};
static uint32_t before[14], baseline;
static struct hpclang_irq_bank bank;
static int armed;
extern volatile uint32_t hpclang_e2e_result;
void hpclang_e2e_irq_snapshot(struct hpclang_irq_bank *);
void __real_hpclang_sound_restore(void);

static void fail(unsigned code)
{
    hpclang_irq_save_disable();
    hpclang_e2e_result = UINT32_C(0x46417d00) | code;
    for (;;)
        __asm__ volatile("nop");
}

void hpclang_e2e_sound_exit(void)
{
    baseline = hpclang_irq_save_disable();
    for (unsigned i = 0; i < 14; ++i)
        before[i] = REG(addresses[i]);
    hpclang_e2e_irq_snapshot(&bank);
    hpclang_irq_restore(baseline);
    /* Long enough to require cancellation, rather than natural completion. */
    if (kos_beep(880, 60000, 1))
        fail(1);
    if (hpclang_sound_status() != 1 || !(REG(0x07600008) & 0x1000u) || !(REG(0x07800000) & 0x20u))
        fail(2);
    armed = 1;
    HPCLANG_E2E_ASYNC_EXIT(0);
}

/* Observe immediately after the real runtime release, before power cleanup,
 * launcher return or host restoration can hide a leaked peripheral/IRQ lease.
 * This wrapper neither stops playback nor collects its completed state. */
void __wrap_hpclang_sound_restore(void)
{
    __real_hpclang_sound_restore();
    if (!armed)
        return;
    uint32_t current = hpclang_irq_save_disable();
    if ((current & 0xc0u) != (baseline & 0xc0u))
        fail(3);
    for (unsigned i = 0; i < 14; ++i)
        if (REG(addresses[i]) != before[i])
            fail(0x10 + i);
    struct hpclang_irq_bank after;
    hpclang_e2e_irq_snapshot(&after);
    if (bank.stack != after.stack || bank.link != after.link || bank.status != after.status)
        fail(4);
    if (hpclang_exit_hooks.sound_async || hpclang_exit_hooks.sound_async_active)
        fail(5);
    armed = 0;
    hpclang_irq_restore(current);
}
