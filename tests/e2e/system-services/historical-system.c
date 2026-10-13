/* Clean-service port of HPGCC 1.1 examples/other/sieve.c and HPGCC2
 * examples/sieve.c: allocate a prime list, repeat Eratosthenes' sieve, measure
 * elapsed time and display the result. The workload is bounded instead of
 * consuming all free calculator memory; timings use monotonic microseconds.
 * CPU speed selection remains a separate power-policy capability, not a no-op
 * compatibility claim. The original interactive pause becomes a capture. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hpsys.h>
#include <hpclang/console.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/time.h>

extern const struct hpclang_font hpclang_benchmark_font;

unsigned hpclang_historical_system(int (*checkpoint)(uint32_t, uint32_t))
{
    uint64_t delay_start, delay_end;
    int seconds = sys_RTC_seconds();
    if (seconds < 0 || seconds >= 86400 || sys_delta_seconds(86399, 1) != 2 ||
        hpclang_time_read_us(&delay_start))
        return 4;
    sys_sleep(1);
    if (hpclang_time_read_us(&delay_end) || delay_end - delay_start < 1000000u)
        return 5;
    if (hpclang_time_read_us(&delay_start))
        return 6;
    sys_waitTicks(7500);
    if (hpclang_time_read_us(&delay_end) || delay_end - delay_start < 10000u)
        return 7;
    enum
    {
        LIMIT = 1000,
        ITERATIONS = 16
    };
    uint32_t *primes = calloc(LIMIT, sizeof *primes);
    struct hpclang_console *console = hpclang_console_create(32, 4, &hpclang_benchmark_font);
    struct hpclang_console *reference = hpclang_console_create(32, 2, &hpclang_benchmark_font);
    unsigned result = 1;
    if (!primes || !console || !reference)
        goto done;
    uint64_t before, after;
    if (hpclang_time_read_us(&before))
        goto done;
    unsigned count = 0;
    for (unsigned iteration = 0; iteration < ITERATIONS; ++iteration) {
        uint32_t bits[(LIMIT + 31) / 32];
        memset(bits, 0xff, sizeof bits);
        count = 0;
        for (unsigned candidate = 2; candidate < LIMIT; ++candidate) {
            if (!(bits[candidate / 32] & (UINT32_C(1) << (candidate % 32))))
                continue;
            primes[count++] = candidate;
            for (unsigned composite = candidate; composite < LIMIT; composite += candidate)
                bits[composite / 32] &= ~(UINT32_C(1) << (composite % 32));
        }
    }
    if (hpclang_time_read_us(&after) || after < before)
        goto done;
    uint32_t sum = 0;
    for (unsigned i = 0; i < count; ++i)
        sum += primes[i];
    result = 2;
    if (count != 168 || primes[0] != 2 || primes[count - 1] != 997 || sum != 76127)
        goto done;
    char report[96];
    int length = snprintf(report, sizeof report, "%u primes\n%u iterations\n%llu us", count,
                          ITERATIONS, (unsigned long long)(after - before));
    struct hpclang_surface *screen = hpclang_display_surface();
    hpclang_surface_clear(screen, 0);
    result = 3;
    if (length < 0 || (size_t)length >= sizeof report ||
        hpclang_console_write(console, report, (size_t)length) ||
        hpclang_console_render(console, screen, 0, 0))
        goto done;
    /* Fixed expected bands are independent of the calculated list/report.
     * Elapsed time is deliberately not compared to a host wall-clock number. */
    static const char expected[] = "168 primes\n16 iterations";
    if (hpclang_console_write(reference, expected, sizeof expected - 1) ||
        hpclang_console_render(reference, screen, 0, 32) || checkpoint(0x465200ccu, 0x464b00ccu))
        goto done;
    result = 0;
done:
    hpclang_console_destroy(reference);
    hpclang_console_destroy(console);
    free(primes);
    return result;
}
