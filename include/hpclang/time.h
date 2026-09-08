#ifndef HPCLANG_TIME_H
#define HPCLANG_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Microseconds elapsed since the current hpclang process entered its runtime.
 *
 * The clock is monotonic and does not modify the calculator's stock timer or
 * RTC configuration. Resolution is determined by the stock PWM Timer0 setup.
 */
uint64_t hpclang_time_monotonic_us(void);

/*
 * Busy-wait for at least the requested duration according to the monotonic
 * clock. This does not yield the CPU and is deliberately named delay rather
 * than sleep.
 */
void hpclang_delay_us(uint64_t microseconds);

#ifdef __cplusplus
}
#endif

#endif
