#ifndef HPCLANG_HPGCC_HPSYS_H
#define HPCLANG_HPGCC_HPSYS_H

#include <time.h>

#ifndef HPCLANG_HPGCC_LONG_TYPES_DEFINED
#define HPCLANG_HPGCC_LONG_TYPES_DEFINED
typedef unsigned long long ULONGLONG;
typedef long long LONGLONG;
#endif

/* Source-compatible elapsed-time state; not the historical binary layout.
 * current is nanoseconds. last_val is private monotonic sampling state,
 * widened from the historical raw 16-bit PWM count. timer 0..4 selects a
 * logical stopwatch on the shared monotonic clock, not a PWM controller. */
typedef struct
{
    ULONGLONG current;
    ULONGLONG last_val;
    unsigned timer : 3;
    unsigned inited : 1;
} sys_timer_t;

#define SYS_TIMER_INITIALIZER {0, 0, 0, 0}
#define SYS_TIMER_INITIALIZER_NUM(n) {0, 0, (n), 0}

#ifdef __cplusplus
extern "C"
{
#endif

    /* HPGCC 1.1/2.0 milliseconds delay. Busy-waits; does not suspend the CPU. */
    void sys_sleep(unsigned int millis);

    /* Return elapsed nanoseconds since the last sample and add them to current.
     * First sample initializes current to zero. Reset inited to restart.
     * Error returns zero with errno, leaving the state unchanged: EINVAL for
     * NULL/invalid timer index, EIO for clock failure/regression, ERANGE for
     * nanosecond overflow. Resolution is that of hpclang_time_read_us(). */
    ULONGLONG sys_updateTimer(sys_timer_t *timer);

    /* Intended historical 750 kHz tick duration, rounded up to microseconds.
     * Does not require slow mode or reconfigure PWM/interrupt registers. */
    void sys_waitTicks(unsigned int ticks);

    /* Civil seconds since midnight, or -1 with the clean RTC service's errno. */
    int sys_RTC_seconds(void);

    /* Read civil RTC into standard struct tm. Weekday/year-day are computed;
     * tm_isdst is -1 (no timezone policy). On error leave output unchanged and
     * set errno; NULL reports EINVAL. Does not copy the old broken weekday. */
    void sys_tm_RTC(struct tm *value);

    /* Forward difference within one civil day, including midnight wrap.
     * Inputs must be 0..86399; invalid input returns -1/EINVAL. */
    int sys_delta_seconds(int start, int end);

#ifdef __cplusplus
}
#endif
#endif
