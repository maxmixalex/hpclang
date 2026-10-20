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

    /* Physical RTC tick divider: (selector+1)/128 seconds, selector 1..127.
     * Return the previous software selector (initially 15); 0 or >127 queries
     * without touching hardware. Setter failure returns 0 and sets errno.
     * First use leases the tick source; changing period restarts its phase.
     * Poll returns 1 and consumes a pending indication, 0 when none, -1/error.
     * Multiple expirations coalesce; an already pending tick counts first.
     * Wait with count <=0 is a no-op; errors terminate the wait with errno.
     * Exit/_Exit restore host divider/enable and interrupt mask, not elapsed
     * phase or indications deliberately consumed by the application. Only the
     * tick source is borrowed; civil RTC and PWM timers remain unchanged. */
    unsigned char sys_setRTCTickPeriod(unsigned char selector);
    int sys_isRTCTick(void);
    void sys_waitRTCTicks(int count);

    /* Select the owned economy profile (at most 12 MHz), or restore the host
     * profile saved on entry to economy. These are not nested scopes. Retain
     * slower host settings; do not copy historical hardcoded LCD dividers or
     * force a presumed host clock. Errors set errno; runtime exit restores HOST. */
    void sys_slowOn(void);
    void sys_slowOff(void);

    /* HPGCC 1.1's corrected tone interface: Hz and milliseconds. Blocks through
     * the owned sound service, restoring borrowed PWM/pin state on return.
     * A nonzero frequency with zero duration does nothing. The void return is retained;
     * failures set errno (EINVAL, ERANGE, EBUSY, EIO, ENOSYS, EOVERFLOW).
     * No timer setup or slow CPU mode is needed. */
    void sys_playTone2(unsigned int frequency_hz, unsigned int duration_ms);
    /* Earlier interface: Hz and hundredths of a second, following its nominal
     * cycle-count duration. Does not reproduce the historical wrong clock
     * assumption. Same ownership/error contract as sys_playTone2; durations
     * exceeding UINT32_MAX milliseconds fail with ERANGE before playback. */
    void sys_playTone(unsigned int frequency_hz, unsigned int duration_cs);

    /* Convert the low min(abs(nibbles),8) packed digits, including the historical
     * negative-width convention. Zero width returns zero. Each four-bit digit
     * is accumulated in base ten without checking that it is <=9. */
    unsigned int sys_bcd2bin_nibbles(unsigned int packed, int nibbles);
    /* Byte-sized convenience, deliberately only two digits. Also has a callable
     * symbol for applications taking the old function's address. */
    unsigned int sys_bcd2bin(unsigned int packed);
#define sys_bcd2bin(packed) sys_bcd2bin_nibbles((packed), 2)

    /* Borrowed static text, replaced by later calls; omit leading zero digits.
     * Zero yields "0". Historical digit mapping is preserved: nibbles 10..15
     * become ':' through '?', rather than validating or formatting hexadecimal.
     * Not reentrant; copy the result before another call. */
    char *sys_bcd2str(unsigned int packed);

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
