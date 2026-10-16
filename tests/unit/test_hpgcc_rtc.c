#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <hpsys.h>
#include <hpclang/device.h>
#include "rtc-tick.h"
#include "exit-hooks.h"

static struct hpclang_device_info device;
static uint32_t regs[6] = {0, 0x89, 0x1000, 0x4000, 0, 0};
static unsigned writes, reads, irq_depth, generated, generate;

const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
uint32_t hpclang_irq_save_disable(void)
{
    assert(!irq_depth++);
    return 0x53;
}
void hpclang_irq_restore(uint32_t value)
{
    assert(value == 0x53 && irq_depth-- == 1);
}
static uint32_t read_reg(void *context, enum hpclang_rtc_tick_reg reg)
{
    (void)context;
    assert(irq_depth);
    ++reads;
    if (reg == RTC_TICK_SOURCE && generate && !(regs[reg] & 0x100)) {
        regs[reg] |= 0x100;
        ++generated;
    }
    return regs[reg];
}
static void write_reg(void *context, enum hpclang_rtc_tick_reg reg, uint32_t value)
{
    (void)context;
    assert(irq_depth);
    ++writes;
    if (reg == RTC_TICK_SOURCE || reg == RTC_TICK_PENDING)
        regs[reg] &= ~value;
    else
        regs[reg] = value;
}
const struct hpclang_rtc_tick_bus hpclang_rtc_tick_bus = {0, read_reg, write_reg};

int main(void)
{
    assert(sys_setRTCTickPeriod(0) == 15 && sys_setRTCTickPeriod(255) == 15);
    sys_waitRTCTicks(0);
    sys_waitRTCTicks(INT_MIN);
    assert(!writes && !reads && !irq_depth && !hpclang_exit_hooks.rtc_tick);
    assert(!sys_setRTCTickPeriod(31) && errno == ENOSYS);
    assert(sys_isRTCTick() == -1 && errno == ENOSYS);
    sys_waitRTCTicks(1);
    assert(errno == ENOSYS && !reads && !writes && !irq_depth);
    device.family = HPCLANG_FAMILY_HP4950;
    regs[RTC_TICK_MODE] |= 0x100;
    assert(!sys_setRTCTickPeriod(31) && errno == ENOTSUP);
    assert(sys_setRTCTickPeriod(0) == 15 && !hpclang_exit_hooks.rtc_tick && !writes);
    regs[RTC_TICK_MODE] &= ~0x100;
    regs[RTC_TICK_SOURCE] = 0x100;
    assert(sys_setRTCTickPeriod(31) == 15);
    assert(sys_setRTCTickPeriod(128) == 31 && regs[RTC_TICK_COUNT] == 0x9f);
    assert(hpclang_exit_hooks.rtc_tick);
    sys_waitRTCTicks(1); /* Existing pending tick satisfies the entire wait. */
    assert(!generated && !regs[RTC_TICK_SOURCE]);
    assert(sys_isRTCTick() == 0);
    generate = 1;
    sys_waitRTCTicks(3);
    generate = 0;
    assert(generated == 3);
    assert(sys_setRTCTickPeriod(1) == 31 && regs[RTC_TICK_COUNT] == 0x81);
    assert(sys_setRTCTickPeriod(127) == 1 && regs[RTC_TICK_COUNT] == 0xff);
    hpclang_rtc_tick_restore();
    assert(!hpclang_exit_hooks.rtc_tick && regs[RTC_TICK_COUNT] == 0x89);
    assert(regs[RTC_TICK_MASK] == 0x4000 && !irq_depth);
    assert(sys_setRTCTickPeriod(0) == 15);
    unsigned before = writes;
    hpclang_rtc_tick_restore();
    assert(writes == before);
    assert(sys_isRTCTick() == 0 && hpclang_exit_hooks.rtc_tick);
    hpclang_rtc_tick_restore();
    return 0;
}
