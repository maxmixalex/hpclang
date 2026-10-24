#include <stdint.h>
#include <errno.h>
#include "clock-internal.h"
#include "irq.h"
#include "irq-owner.h"
#include "watchdog-alarm.h"
#include "sound-engine.h"
#include "sound-session.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))

volatile uint32_t hpclang_e2e_watchdog_hits;
static volatile unsigned irq_finished, irq_error;
static uint64_t irq_deadline, irq_last_time;
static struct hpclang_sound_session session;
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
static const struct hpclang_sound_session_bus session_bus = {
    {audio_read, audio_write, 0},
    {0, read_reg, write_reg},
    {0, owner_read, owner_write, owner_exchange}};

static void stop(void)
{
    hpclang_sound_session_close(&session_bus, &session);
}

void hpclang_e2e_watchdog_dispatch(void)
{
    int saved_errno = errno;
    uint64_t now = 0;
    ++hpclang_e2e_watchdog_hits;
    int error = hpclang_time_read_us_masked(&now) ? EIO : 0;
    hpclang_sound_session_advance(&session_bus, &session, now, error);
    irq_last_time = session.last_time;
    irq_error = session.state == SOUND_SESSION_FAILED;
    irq_finished = session.state != SOUND_SESSION_RUNNING;
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
    register_writes = 0;
    if (hpclang_time_read_us_masked(&irq_last_time) || irq_last_time > UINT64_MAX - 100000) {
        hpclang_irq_restore(cpsr);
        return 7;
    }
    irq_deadline = irq_last_time + 100000;
    struct hpclang_irq_bank before, after;
    hpclang_e2e_irq_snapshot(&before);
    if (hpclang_sound_session_start(&session_bus, &session, num, den, 880, irq_last_time,
                                    irq_deadline, (uintptr_t)(irq_stack + 256),
                                    (uintptr_t)hpclang_e2e_watchdog_handler, 0)) {
        hpclang_irq_restore(cpsr);
        return 15;
    }
    if (session.irq.bank.stack != before.stack || session.irq.bank.link != before.link ||
        session.irq.bank.status != before.status)
        failure = 2;
    if ((REG(0x07600008) & 0xf000) != 0x9000 || (REG(0x07a00010) & 0x30) != 0x20)
        failure = 9;
    /* Force an intermediate expiry; only the owned dispatch can complete. */
    if (hpclang_watchdog_advance(&bus, &session.alarm, irq_last_time, irq_last_time + 1000)) {
        stop();
        hpclang_irq_restore(cpsr);
        return 8;
    }
    errno = EDOM;
    hpclang_irq_restore((cpsr | 0x40u) & ~0x80u);
    /* A bounded busy loop: delivery cannot depend on SDK service calls. */
    for (volatile unsigned i = 0; i < 20000000 && !irq_finished; ++i)
        __asm__ volatile("nop");
    hpclang_irq_save_disable();
    if (hpclang_e2e_watchdog_hits < 2 || !irq_finished || irq_error ||
        irq_last_time < irq_deadline || session.alarm.owned || session.tone.owned || errno != EDOM)
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
