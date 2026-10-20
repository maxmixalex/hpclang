#include "rtc-tick.h"

static const uintptr_t registers[] = {0x07b00040u, 0x07b00044u, 0x07100004u,
                                      0x07100008u, 0x07100000u, 0x07100010u};

static uint32_t read_reg(void *context, enum hpclang_rtc_tick_reg reg)
{
    (void)context;
    if (reg == RTC_TICK_CONTROL || reg == RTC_TICK_COUNT)
        return *(volatile uint8_t *)registers[reg];
    return *(volatile uint32_t *)registers[reg];
}

static void write_reg(void *context, enum hpclang_rtc_tick_reg reg, uint32_t value)
{
    (void)context;
    if (reg == RTC_TICK_CONTROL || reg == RTC_TICK_COUNT)
        *(volatile uint8_t *)registers[reg] = (uint8_t)value;
    else
        *(volatile uint32_t *)registers[reg] = value;
}

const struct hpclang_rtc_tick_bus hpclang_rtc_tick_bus = {0, read_reg, write_reg};
