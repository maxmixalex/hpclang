#include "time-math.h"

#define USEC_PER_SEC UINT64_C(1000000)

static uint64_t distance(uint64_t a, uint64_t b)
{
    return a >= b ? a - b : b - a;
}

uint64_t hpclang_time_elapsed_us(
    uint64_t rtc_delta_seconds,
    uint32_t start_count,
    uint32_t current_count,
    uint32_t period,
    uint64_t freq_num,
    uint64_t freq_den)
{
    if (period == 0u || freq_num == 0u || freq_den == 0u)
        return 0u;

    uint64_t modulo_ticks =
        start_count >= current_count
            ? (uint64_t)start_count - current_count
            : (uint64_t)start_count + period - current_count;

    /*
     * The hardware RTC used by these calculators covers years 2000..2099.
     * Over that range rtc_delta_seconds * freq_num fits in uint64_t for every
     * S3C2410 clock configuration accepted by the runtime.
     */
    uint64_t expected_num = rtc_delta_seconds * freq_num;
    uint64_t expected_ticks = expected_num / freq_den;
    uint64_t expected_rem = expected_num % freq_den;

    uint64_t cycles = expected_ticks / period;
    uint64_t candidate = cycles * period + modulo_ticks;
    uint64_t best = candidate;
    uint64_t best_distance = distance(candidate, expected_ticks);

    if (candidate >= period) {
        uint64_t previous = candidate - period;
        uint64_t d = distance(previous, expected_ticks);
        if (d < best_distance) {
            best = previous;
            best_distance = d;
        }
    }

    if (candidate <= UINT64_MAX - period) {
        uint64_t next = candidate + period;
        uint64_t d = distance(next, expected_ticks);
        if (d < best_distance)
            best = next;
    }

    /*
     * Convert only the small correction from RTC whole seconds to timer
     * ticks. This avoids multiplying a long process lifetime by the timer
     * period numerator.
     *
     *   correction_ticks =
     *       best - rtc_delta_seconds * freq_num / freq_den
     */
    uint64_t base_us = rtc_delta_seconds * USEC_PER_SEC;

    if (best >= expected_ticks) {
        uint64_t correction_num =
            (best - expected_ticks) * freq_den;
        if (correction_num >= expected_rem) {
            correction_num -= expected_rem;
            uint64_t correction_us =
                correction_num * USEC_PER_SEC / freq_num;
            return base_us + correction_us;
        }

        uint64_t correction_us =
            (expected_rem - correction_num) * USEC_PER_SEC / freq_num;
        return correction_us > base_us ? 0u : base_us - correction_us;
    }

    uint64_t correction_num =
        (expected_ticks - best) * freq_den + expected_rem;
    uint64_t correction_us =
        correction_num * USEC_PER_SEC / freq_num;
    return correction_us > base_us ? 0u : base_us - correction_us;
}
