#include <stdint.h>
#include "clock-internal.h"
#include "irq.h"
#include "sound-alarm.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))

volatile uint32_t hpclang_e2e_irq_hits;
static uint32_t irq_stack[256] __attribute__((aligned(8)));
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
    REG(addresses[reg]) = value;
}

unsigned hpclang_e2e_irq_test(void)
{
    const struct hpclang_sound_alarm_bus bus = {0, read_reg, write_reg};
    struct hpclang_sound_alarm alarm = {0};
    uint64_t num, den;
    hpclang_clock_pclk(&num, &den);
    uint32_t cpsr = hpclang_irq_save_disable();
    uint32_t mask = REG(0x07100008), mode = REG(0x07100004);
    uint32_t vector = REG(0x08000018), swi = REG(0x08000008);
    uint32_t source = REG(0x07100000);
    unsigned failure = 0;
    hpclang_e2e_irq_hits = 0;
    irq_stack[0] = 0x51ac7ead;
    if (hpclang_sound_alarm_acquire(&bus, &alarm, num, den, 20000)) {
        hpclang_irq_restore(cpsr);
        return 1;
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
    hpclang_irq_restore((cpsr | 0x40u) & ~0x80u);
    /* A bounded busy loop: delivery cannot depend on SDK service calls. */
    for (volatile unsigned i = 0; i < 20000000 && !hpclang_e2e_irq_hits; ++i)
        __asm__ volatile("nop");
    hpclang_irq_save_disable();
    if (hpclang_e2e_irq_hits != 1)
        failure = 3;
    REG(0x07100008) = UINT32_MAX;
    REG(0x08000018) = vector;
    hpclang_irq_bank_exchange(&bank);
    hpclang_e2e_irq_snapshot(&after);
    if (before.stack != after.stack || before.link != after.link || before.status != after.status)
        failure = 4;
    if (irq_stack[0] != 0x51ac7ead || REG(0x08000008) != swi)
        failure = 5;
    hpclang_sound_alarm_release(&bus, &alarm);
    REG(0x07100004) = mode;
    REG(0x07100008) = mask;
    if ((REG(0x07100000) & source) != source || REG(0x07100004) != mode ||
        REG(0x07100008) != mask || REG(0x08000018) != vector)
        failure = 6;
    hpclang_irq_restore(cpsr);
    return failure;
}
