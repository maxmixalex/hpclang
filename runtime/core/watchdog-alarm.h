#ifndef HPCLANG_WATCHDOG_ALARM_H
#define HPCLANG_WATCHDOG_ALARM_H
#include <stdint.h>

enum hpclang_watchdog_reg
{
    WDT_CONTROL,
    WDT_RELOAD,
    WDT_COUNT,
    WDT_MASK,
    WDT_SOURCE,
    WDT_PENDING
};
struct hpclang_watchdog_bus
{
    void *context;
    uint32_t (*read)(void *, enum hpclang_watchdog_reg);
    void (*write)(void *, enum hpclang_watchdog_reg, uint32_t);
};
struct hpclang_watchdog_alarm
{
    uint32_t control, reload, count, mask;
    uint64_t clock_num, clock_den;
    int owned;
};
/* Internal S3C2410 interrupt-only lease. Caller masks IRQ/FIQ and holds PCLK
 * stable throughout ownership. Only a stopped watchdog without pending IRQ9
 * can be borrowed. Acquisition leaves IRQ9 masked; caller owns routing, stack
 * and dispatch. All running configurations disable reset. Release restores
 * the original stopped configuration/count and mask, clearing only IRQ9.
 * No allocation, clock reads, IRQ enabling or public asynchronous API here.
 * Deadlines are absolute monotonic microseconds. Advance returns 0 for a new
 * chunk, 1 after releasing a due lease, -1 on error. Errors do not write.
 * Caller must stop the associated audio on completion or failure. Long waits
 * use 65535-count chunks at PCLK/128; each final chunk rounds upward.
 */
int hpclang_watchdog_acquire(const struct hpclang_watchdog_bus *, struct hpclang_watchdog_alarm *,
                             uint64_t clock_num, uint64_t clock_den, uint64_t now,
                             uint64_t deadline);
int hpclang_watchdog_advance(const struct hpclang_watchdog_bus *, struct hpclang_watchdog_alarm *,
                             uint64_t now, uint64_t deadline);
void hpclang_watchdog_release(const struct hpclang_watchdog_bus *, struct hpclang_watchdog_alarm *);
#endif
