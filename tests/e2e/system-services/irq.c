#include <stdint.h>
#include <errno.h>
#include "clock-internal.h"
#include "irq.h"
#include "sound-alarm.h"
#include "sound-playback.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))

volatile uint32_t hpclang_e2e_irq_hits;
static volatile unsigned irq_finished, irq_error;
static uint64_t irq_deadline, irq_last_time;
static struct hpclang_sound_playback playback;
static uint32_t irq_stack[256] __attribute__((aligned(8)));
static unsigned register_writes;
void hpclang_e2e_irq_snapshot(struct hpclang_irq_bank *state);
void hpclang_e2e_irq_handler(void);

static const uintptr_t addresses[] = {0x07600000, 0x07600004, 0x07600008, 0x0760003c,
                                      0x07600040, 0x07100008, 0x07100000, 0x07100010};
static uint32_t read_reg(void *context, enum hpclang_sound_alarm_reg reg)
{
    (void)context;
    return REG(addresses[reg]);
}
static void write_reg(void *context, enum hpclang_sound_alarm_reg reg, uint32_t value)
{
    (void)context;
    ++register_writes;
    REG(addresses[reg]) = value;
}
static const struct hpclang_sound_alarm_bus bus = {0, read_reg, write_reg};
static const uintptr_t audio_addresses[] = {0x07600000, 0x07600004, 0x07600008, 0x07600024,
                                            0x07600028, 0x0760002c, 0x07a00010, 0x07100008,
                                            0x07100000, 0x07100010};
static uint32_t audio_read(void *context, enum hpclang_sound_reg reg)
{
    (void)context;
    return REG(audio_addresses[reg]);
}
static void audio_write(void *context, enum hpclang_sound_reg reg, uint32_t value)
{
    (void)context;
    ++register_writes;
    REG(audio_addresses[reg]) = value;
}
static const struct hpclang_sound_bus audio = {audio_read, audio_write, 0};

/* Only the actual IRQ entry calls this. The foreground below executes no
 * clock or SDK calls until completion. Preserve the interrupted errno. */
void hpclang_e2e_irq_dispatch(void)
{
    int saved_errno = errno;
    uint64_t now;
    ++hpclang_e2e_irq_hits;
    if (hpclang_time_read_us_masked(&now) || now < irq_last_time) {
        irq_error = 1;
        hpclang_sound_playback_stop(&audio, &bus, &playback);
        irq_finished = 1;
    } else {
        irq_last_time = now;
        int result = hpclang_sound_playback_advance(&audio, &bus, &playback, now);
        if (result) {
            irq_error = result < 0;
            irq_finished = 1;
        }
    }
    if (irq_finished)
        REG(0x07100008) = UINT32_MAX;
    errno = saved_errno;
}

unsigned hpclang_e2e_irq_test(void)
{
    uint64_t num, den;
    hpclang_clock_pclk(&num, &den);
    uint32_t cpsr = hpclang_irq_save_disable();
    uint32_t mask = REG(0x07100008), mode = REG(0x07100004);
    uint32_t vector = REG(0x08000018), swi = REG(0x08000008);
    uint32_t source = REG(0x07100000);
    uint32_t audio_before[7];
    for (unsigned i = 0; i < 7; ++i)
        audio_before[i] = REG(audio_addresses[i]);
    unsigned failure = 0;
    hpclang_e2e_irq_hits = 0;
    irq_finished = irq_error = 0;
    irq_stack[0] = 0x51ac7ead;
    /* Stock can own PWM4. An owned-IRQ test may borrow an idle channel only;
     * active/update/DMA or pending state must instead exercise rejection with
     * no writes. Direct fixtures still require actual interrupt delivery when
     * the channel is idle. This does not claim a Stock audio timer backend. */
    unsigned busy = (REG(0x07600008) & 0x300000u) || ((REG(0x07600004) >> 20) & 15u) == 5u ||
                    ((source | REG(0x07100010)) & 0x4000u);
    register_writes = 0;
    errno = 0;
    if (hpclang_time_read_us_masked(&irq_last_time) || irq_last_time > UINT64_MAX - 100000) {
        hpclang_irq_restore(cpsr);
        return 7;
    }
    irq_deadline = irq_last_time + 100000;
    if (hpclang_sound_playback_begin(&audio, &bus, &playback, num, den, 880, irq_last_time,
                                     irq_deadline)) {
        failure = busy && errno == EBUSY && !register_writes && !playback.alarm.owned &&
                          !playback.tone.owned
                      ? 0
                      : 1;
        hpclang_irq_restore(cpsr);
        return failure;
    }
    if ((REG(0x07600008) & 0xf000) != 0x9000 || (REG(0x07a00010) & 0x30) != 0x20)
        failure = 9;
    /* Force an intermediate interrupt before the actual deadline. */
    if (hpclang_sound_alarm_rearm(&bus, &playback.alarm, 1000)) {
        hpclang_sound_playback_stop(&audio, &bus, &playback);
        hpclang_irq_restore(cpsr);
        return 8;
    }

    REG(0x07100008) = UINT32_MAX;
    REG(0x07100004) = 0;
    /* Preserve all Stock source latches while releasing their selection. */
    REG(0x07100010) = REG(0x07100010);
    struct hpclang_irq_bank before, after;
    hpclang_e2e_irq_snapshot(&before);
    struct hpclang_irq_bank bank = {(uintptr_t)(irq_stack + 256), 0, 0};
    hpclang_irq_bank_exchange(&bank);
    if (bank.stack != before.stack || bank.link != before.link || bank.status != before.status)
        failure = 2;
    REG(0x08000018) = (uintptr_t)hpclang_e2e_irq_handler;
    REG(0x07100008) = ~UINT32_C(0x4000);
    errno = EDOM;
    hpclang_irq_restore((cpsr | 0x40u) & ~0x80u);
    /* A bounded busy loop: delivery cannot depend on SDK service calls. */
    for (volatile unsigned i = 0; i < 20000000 && !irq_finished; ++i)
        __asm__ volatile("nop");
    hpclang_irq_save_disable();
    if (hpclang_e2e_irq_hits < 2 || !irq_finished || irq_error || irq_last_time < irq_deadline ||
        playback.alarm.owned || playback.tone.owned || errno != EDOM)
        failure = 3;
    REG(0x07100008) = UINT32_MAX;
    REG(0x08000018) = vector;
    hpclang_irq_bank_exchange(&bank);
    hpclang_e2e_irq_snapshot(&after);
    if (before.stack != after.stack || before.link != after.link || before.status != after.status)
        failure = 4;
    if (irq_stack[0] != 0x51ac7ead || REG(0x08000008) != swi)
        failure = 5;
    /* Preserve an earlier delivery/ownership failure; otherwise identify the
     * exact audio register before foreground cleanup could repair it. */
    for (unsigned i = 0; i < 7; ++i)
        if (!failure && REG(audio_addresses[i]) != audio_before[i])
            failure = 0x20u + i;
    hpclang_sound_playback_stop(&audio, &bus, &playback);
    REG(0x07100004) = mode;
    REG(0x07100008) = mask;
    if ((REG(0x07100000) & source) != source || REG(0x07100004) != mode ||
        REG(0x07100008) != mask || REG(0x08000018) != vector)
        failure = 6;
    hpclang_irq_restore(cpsr);
    return failure;
}
