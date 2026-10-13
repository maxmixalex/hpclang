#ifndef HPCLANG_HPGCC_HPSYS_H
#define HPCLANG_HPGCC_HPSYS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* HPGCC 1.1/2.0 implementations use seconds despite the old header's
     * parameter name "millis". Busy-waits; does not suspend the CPU. */
    void sys_sleep(unsigned int seconds);

    /* Intended historical 750 kHz tick duration, rounded up to microseconds.
     * Does not require slow mode or reconfigure PWM/interrupt registers. */
    void sys_waitTicks(unsigned int ticks);

    /* Civil seconds since midnight, or -1 with the clean RTC service's errno. */
    int sys_RTC_seconds(void);

    /* Forward difference within one civil day, including midnight wrap.
     * Inputs must be 0..86399; invalid input returns -1/EINVAL. */
    int sys_delta_seconds(int start, int end);

#ifdef __cplusplus
}
#endif
#endif
