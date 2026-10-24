#include <errno.h>
#include <hpclang/sound.h>
#include "irq.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))
void hpclang_e2e_irq_snapshot(struct hpclang_irq_bank *);
unsigned hpclang_e2e_kos_sound_test(void);
unsigned hpclang_e2e_sound_async_test(void)
{
    const uintptr_t addresses[] = {0x07600000, 0x07600004, 0x07600008, 0x07600024, 0x07600028,
                                   0x0760002c, 0x07a00010, 0x07800000, 0x07800004, 0x07800008,
                                   0x07100008, 0x07100004, 0x08000018, 0x08000008};
    uint32_t before[14];
    uint32_t baseline = hpclang_irq_save_disable();
    for (unsigned i = 0; i < 14; ++i)
        before[i] = REG(addresses[i]);
    struct hpclang_irq_bank bank, after;
    hpclang_e2e_irq_snapshot(&bank);
    hpclang_irq_restore(baseline);
    unsigned failure = 0;
    if (hpclang_sound_start(880, 1000, 0)) {
        failure = 1;
        goto finish;
    }
    if (hpclang_sound_status() != 1) {
        failure = 2;
        goto finish;
    }
    if (hpclang_sound_start(440, 1000, 0) != -1 || errno != EBUSY) {
        failure = 3;
        goto finish;
    }
    if (hpclang_sound_play(440, 1, 0, 0) != -1 || errno != EBUSY) {
        failure = 4;
        goto finish;
    }
    if (hpclang_sound_start(440, 10, 1)) {
        failure = 5;
        goto finish;
    }
    errno = EDOM;
    /* No SDK calls: observing PWM does not drive completion. A fixed number
     * of emulator instructions is not an elapsed-time oracle. Wait for the
     * production IRQ to stop PWM, with a bounded failure guard. */
    for (volatile unsigned i = 0; i < 20000000 && (REG(0x07600008) & 0x1000u); ++i)
        __asm__ volatile("nop");
    hpclang_irq_save_disable();
    if (errno != EDOM)
        failure = 6;
    for (unsigned i = 0; i < 10; ++i)
        if (REG(addresses[i]) != before[i])
            failure = 7;
    if (hpclang_sound_status() != 0) {
        failure = failure ? failure : 8;
        goto finish;
    }
    uint32_t restored = hpclang_irq_save_disable();
    if ((restored & 0xc0) != (baseline & 0xc0))
        failure = 9;
    hpclang_e2e_irq_snapshot(&after);
    if (bank.stack != after.stack || bank.link != after.link || bank.status != after.status)
        failure = 10;
    for (unsigned i = 10; i < 14; ++i)
        if (REG(addresses[i]) != before[i])
            failure = 0x20 + i;
    hpclang_irq_restore(restored);
    if (failure)
        goto finish;
    if (hpclang_sound_start(880, 1000, 0)) {
        failure = 12;
        goto finish;
    }
    hpclang_sound_stop();
    if (hpclang_sound_status() != 0)
        failure = 13;
finish:
    hpclang_sound_stop();
    if (!failure) {
        unsigned kos_failure = hpclang_e2e_kos_sound_test();
        if (kos_failure)
            failure = 0x40 + kos_failure;
    }
    hpclang_irq_save_disable();
    for (unsigned i = 0; i < 14; ++i)
        if (REG(addresses[i]) != before[i])
            failure = failure ? failure : 14;
    hpclang_irq_restore(baseline);
    return failure;
}
