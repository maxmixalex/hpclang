#include <stdint.h>
#include <errno.h>
#include "clock-internal.h"
#include "irq.h"
#include "irq-owner.h"
#include "watchdog-alarm.h"
#include "sound-engine.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))

volatile uint32_t hpclang_e2e_watchdog_hits;
static volatile unsigned irq_finished, irq_error;
static uint64_t irq_deadline, irq_last_time;
static struct hpclang_watchdog_alarm alarm;
static struct hpclang_sound_saved tone;
static uint32_t irq_stack[256] __attribute__((aligned(8)));
static unsigned register_writes;
void hpclang_e2e_irq_snapshot(struct hpclang_irq_bank *state);
void hpclang_e2e_watchdog_handler(void);

static const uintptr_t addresses[] = {0x07800000, 0x07800004, 0x07800008,
                                      0x07100008, 0x07100000, 0x07100010};
static uint32_t read_reg(void *context, enum hpclang_watchdog_reg reg)
{
    (void)context;
    return REG(addresses[reg]);
}
static void write_reg(void *context, enum hpclang_watchdog_reg reg, uint32_t value)
{
    (void)context;
    ++register_writes;
    REG(addresses[reg]) = value;
}
static const struct hpclang_watchdog_bus bus = {0, read_reg, write_reg};
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

static const uintptr_t irq_addresses[] = {0x07100008, 0x07100004, 0x07100010, 0x08000018};
static uint32_t owner_read(void *context, enum hpclang_irq_owner_reg reg)
{
    (void)context;
    return REG(irq_addresses[reg]);
}
static void owner_write(void *context, enum hpclang_irq_owner_reg reg, uint32_t value)
{
    (void)context;
    REG(irq_addresses[reg]) = value;
}
static void owner_exchange(void *context, struct hpclang_irq_bank *bank)
{
    (void)context;
    hpclang_irq_bank_exchange(bank);
}
static const struct hpclang_irq_owner_bus owner_bus = {0, owner_read, owner_write, owner_exchange};

static void stop(void)
{
    hpclang_sound_release(&audio, &tone);
    hpclang_watchdog_release(&bus, &alarm);
}

/* Only the actual IRQ entry calls this. The foreground below executes no
 * clock or SDK calls until completion. Preserve the interrupted errno. */
void hpclang_e2e_watchdog_dispatch(void)
{
    int saved_errno = errno;
    uint64_t now;
    ++hpclang_e2e_watchdog_hits;
    if (hpclang_time_read_us_masked(&now) || now < irq_last_time) {
        irq_error = 1;
        stop();
        irq_finished = 1;
    } else {
        irq_last_time = now;
        int result = hpclang_watchdog_advance(&bus, &alarm, now, irq_deadline);
        if (result) {
            stop();
            irq_error = result < 0;
            irq_finished = 1;
        }
    }
    if (irq_finished)
        REG(0x07100008) = UINT32_MAX;
    errno = saved_errno;
}

unsigned hpclang_e2e_watchdog_test(void)
{
    uint64_t num, den;
    hpclang_clock_pclk(&num, &den);
    uint32_t cpsr = hpclang_irq_save_disable();
    /* Validate the inherited Stock section before touching watchdog MMIO. */
    if ((REG(0x07f001e0) & 0xfff00003u) != 0x53000002u) {
        hpclang_irq_restore(cpsr);
        return 12;
    }
    uint32_t watchdog_before[3] = {REG(addresses[0]), REG(addresses[1]), REG(addresses[2])};
    uint32_t mask = REG(0x07100008), mode = REG(0x07100004);
    uint32_t vector = REG(0x08000018), swi = REG(0x08000008);
    uint32_t source = REG(0x07100000);
    uint32_t audio_before[7];
    for (unsigned i = 0; i < 7; ++i)
        audio_before[i] = REG(audio_addresses[i]);
    unsigned failure = 0;
    hpclang_e2e_watchdog_hits = 0;
    irq_finished = irq_error = 0;
    irq_stack[0] = 0x51ac7ead;
    /* This acceptance fixture requires actual watchdog delivery on every
     * model. Busy state must reject without writes, but is not a timing pass. */
    unsigned busy = (REG(addresses[0]) & 0x20u) || ((source | REG(0x07100010)) & 0x200u);
    register_writes = 0;
    errno = 0;
    if (hpclang_time_read_us_masked(&irq_last_time) || irq_last_time > UINT64_MAX - 100000) {
        hpclang_irq_restore(cpsr);
        return 7;
    }
    irq_deadline = irq_last_time + 100000;
    if (hpclang_watchdog_acquire(&bus, &alarm, num, den, irq_last_time, irq_last_time + 1000)) {
        failure = busy && errno == EBUSY && !register_writes && !alarm.owned ? 11 : 1;
        hpclang_irq_restore(cpsr);
        return failure;
    }
    if (hpclang_sound_acquire(&audio, &tone, num, den, 880)) {
        stop();
        hpclang_irq_restore(cpsr);
        return 13;
    }
    if ((REG(0x07600008) & 0xf000) != 0x9000 || (REG(0x07a00010) & 0x30) != 0x20)
        failure = 9;
    struct hpclang_irq_bank before, after;
    hpclang_e2e_irq_snapshot(&before);
    struct hpclang_irq_owner owner = {0};
    /* CPU IRQ/FIQ remain masked. Snapshot the original controller mask, not
     * the temporary IRQ9 mask installed by peripheral admission. */
    REG(0x07100008) = mask;
    if (hpclang_irq_owner_acquire(&owner_bus, &owner, 0x200, (uintptr_t)(irq_stack + 256),
                                  (uintptr_t)hpclang_e2e_watchdog_handler)) {
        stop();
        hpclang_irq_restore(cpsr);
        return 15;
    }
    if (owner.bank.stack != before.stack || owner.bank.link != before.link ||
        owner.bank.status != before.status)
        failure = 2;
    errno = EDOM;
    hpclang_irq_restore((cpsr | 0x40u) & ~0x80u);
    /* A bounded busy loop: delivery cannot depend on SDK service calls. */
    for (volatile unsigned i = 0; i < 20000000 && !irq_finished; ++i)
        __asm__ volatile("nop");
    hpclang_irq_save_disable();
    if (hpclang_e2e_watchdog_hits < 2 || !irq_finished || irq_error ||
        irq_last_time < irq_deadline || alarm.owned || tone.owned || errno != EDOM)
        failure = 3;
    for (unsigned i = 0; i < 3; ++i)
        if (REG(addresses[i]) != watchdog_before[i])
            failure = 14;
    /* Check audio restoration before foreground cleanup could repair it. */
    for (unsigned i = 0; i < 7; ++i)
        if (REG(audio_addresses[i]) != audio_before[i])
            failure = 10;
    stop();
    /* Release runs only on the foreground stack, after stopping the source. */
    hpclang_irq_owner_release(&owner_bus, &owner);
    hpclang_e2e_irq_snapshot(&after);
    if (before.stack != after.stack || before.link != after.link || before.status != after.status)
        failure = 4;
    if (irq_stack[0] != 0x51ac7ead || REG(0x08000008) != swi)
        failure = 5;
    if ((REG(0x07100000) & source) != source || REG(0x07100004) != mode ||
        REG(0x07100008) != mask || REG(0x08000018) != vector)
        failure = 6;
    hpclang_irq_restore(cpsr);
    return failure;
}
