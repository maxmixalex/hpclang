#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "time-math.h"

#define PERIOD 65536u
#define FREQ_NUM UINT64_C(187500)
#define FREQ_DEN UINT64_C(23)
#define USEC_PER_SEC UINT64_C(1000000)

static uint32_t count_after(uint32_t start, uint64_t ticks)
{
    uint32_t phase = (uint32_t)(ticks % PERIOD);
    return (uint32_t)(((uint64_t)start + PERIOD - phase) % PERIOD);
}

static int check(uint32_t start, uint64_t origin_fraction_us, uint64_t elapsed_us)
{
    uint64_t ticks =
        elapsed_us * FREQ_NUM / (FREQ_DEN * USEC_PER_SEC);
    uint32_t current = count_after(start, ticks);
    uint64_t rtc_delta =
        (origin_fraction_us + elapsed_us) / USEC_PER_SEC;

    uint64_t inferred = hpclang_time_elapsed_us(
        rtc_delta, start, current, PERIOD, FREQ_NUM, FREQ_DEN);

    uint64_t error =
        inferred >= elapsed_us ? inferred - elapsed_us : elapsed_us - inferred;

    /* One stock Timer0 tick is about 123 microseconds. */
    if (error > 130u) {
        fprintf(
            stderr,
            "time inference failed: elapsed=%llu inferred=%llu error=%llu\n",
            (unsigned long long)elapsed_us,
            (unsigned long long)inferred,
            (unsigned long long)error);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const uint64_t cases[] = {
        0u,
        1000u,
        123456u,
        999999u,
        1000000u,
        3000000u,
        7900000u,
        8100000u,
        16000000u,
        60000000u,
        UINT64_C(3600000000),
        UINT64_C(198000000000)
    };

    for (unsigned i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!check(50000u, 123456u, cases[i]))
            return EXIT_FAILURE;
        if (!check(1234u, 900000u, cases[i]))
            return EXIT_FAILURE;
    }

    for (uint64_t i = 0u; i < 20000u; ++i) {
        uint64_t elapsed =
            (i * UINT64_C(982451653) + UINT64_C(12345)) %
            UINT64_C(198000000000);
        uint64_t fraction =
            (i * UINT64_C(7919) + UINT64_C(17)) % USEC_PER_SEC;
        uint32_t start =
            (uint32_t)((i * UINT64_C(104729) + 101u) % PERIOD);

        if (!check(start, fraction, elapsed))
            return EXIT_FAILURE;
    }

    puts("PASS: stock Timer0 wrap inference remains accurate across 55 hours.");
    return EXIT_SUCCESS;
}
