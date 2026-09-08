#ifndef HPCLANG_TIME_MATH_H
#define HPCLANG_TIME_MATH_H

#include <stdint.h>

/*
 * Infer elapsed microseconds from a coarse whole-second RTC delta and a
 * wrapping down-counter. The counter frequency is freq_num / freq_den Hz.
 *
 * The caller guarantees that the counter wrap interval is greater than two
 * seconds, which makes the wrap count unambiguous despite RTC quantization.
 */
uint64_t hpclang_time_elapsed_us(
    uint64_t rtc_delta_seconds,
    uint32_t start_count,
    uint32_t current_count,
    uint32_t period,
    uint64_t freq_num,
    uint64_t freq_den);

#endif
