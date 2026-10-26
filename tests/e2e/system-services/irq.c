#include <stdint.h>
#include <errno.h>
#include "clock-internal.h"
#include "irq.h"
#include "sound-alarm.h"
#include "sound-playback.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))

volatile uint32_t hpclang_e2e_irq_hits;
/* Capture the compound PWM failure before foreground cleanup changes ownership.
 * The distinct checkpoint separates this probe from the watchdog diagnostic. */
struct pwm_observation
{
    uint32_t version, hits, finished, error, alarm_owned, tone_owned, wait_errno, failure;
    uint64_t last_time, deadline, foreground_time;
    uint32_t clock_error, audio_mismatch;
};
volatile struct pwm_observation hpclang_e2e_pwm_observation;
_Static_assert(sizeof(struct pwm_observation) == 64, "diagnostic word layout");
extern volatile uint32_t hpclang_e2e_result;
static void report_failure(void)
{
    static uint32_t report[18];
    report[0] = 0x7b01;
    report[1] = 16;
    uint8_t *destination = (uint8_t *)(report + 2);
    volatile const uint8_t *source = (volatile const uint8_t *)&hpclang_e2e_pwm_observation;
    for (unsigned i = 0; i < sizeof(hpclang_e2e_pwm_observation); ++i)
        destination[i] = source[i];
    uint32_t offset = (uint32_t)(uintptr_t)report - 0x01100000u;
    uint64_t start, now;
    if (hpclang_time_read_us_masked(&start))
        return;
    hpclang_e2e_result = 0x4d100000u | offset;
    for (unsigned spin = 0; spin < 20000000 && hpclang_e2e_result != (0x4d200000u | offset);
         ++spin) {
        if (hpclang_time_read_us_masked(&now) || now - start > 2000000u)
            break;
    }
}
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
    hpclang_e2e_pwm_observation = (struct pwm_observation){.version = 1};
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
    int wait_errno = errno;
    uint64_t foreground_time = 0;
    int clock_error = hpclang_time_read_us_masked(&foreground_time);
    errno = wait_errno;
    hpclang_e2e_pwm_observation.hits = hpclang_e2e_irq_hits;
    hpclang_e2e_pwm_observation.finished = irq_finished;
    hpclang_e2e_pwm_observation.error = irq_error;
    hpclang_e2e_pwm_observation.alarm_owned = playback.alarm.owned;
    hpclang_e2e_pwm_observation.tone_owned = playback.tone.owned;
    hpclang_e2e_pwm_observation.wait_errno = (uint32_t)wait_errno;
    hpclang_e2e_pwm_observation.last_time = irq_last_time;
    hpclang_e2e_pwm_observation.deadline = irq_deadline;
    hpclang_e2e_pwm_observation.foreground_time = foreground_time;
    hpclang_e2e_pwm_observation.clock_error = (uint32_t)clock_error;
    for (unsigned i = 0; i < 7; ++i)
        if (REG(audio_addresses[i]) != audio_before[i])
            hpclang_e2e_pwm_observation.audio_mismatch |= 1u << i;
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
    hpclang_e2e_pwm_observation.failure = failure;
    if (failure)
        report_failure();
    hpclang_irq_restore(cpsr);
    return failure;
}
