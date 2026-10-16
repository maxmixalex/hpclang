/* Clean-service port of HPGCC 1.1 examples/other/sieve.c and HPGCC2
 * examples/sieve.c: allocate a prime list, repeat Eratosthenes' sieve, measure
 * elapsed time and display the result. The workload is bounded instead of
 * consuming all free calculator memory; timings use monotonic microseconds.
 * CPU speed selection remains a separate power-policy capability, not a no-op
 * compatibility claim. The original interactive pause becomes a capture. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <hpsys.h>
#include <hptime.h>
#include <hpclang/console.h>
#include <hpclang/datetime.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/power.h>
#include <hpclang/time.h>

extern const struct hpclang_font hpclang_benchmark_font;

static int legacy_power(void)
{
    uint32_t host, slow, restored;
    uint64_t before, after;
    if (hpclang_power_cpu_frequency_hz(&host) || hpclang_time_read_us(&before))
        return -1;
    errno = 0;
    sys_slowOn();
    sys_slowOn();
    if (errno || hpclang_power_get_profile() != HPCLANG_POWER_ECONOMY ||
        hpclang_power_cpu_frequency_hz(&slow) || slow > host || slow > 12000000u)
        return -1;
    sys_sleep(1);
    sys_slowOff();
    sys_slowOff();
    if (errno || hpclang_power_get_profile() != HPCLANG_POWER_HOST ||
        hpclang_power_cpu_frequency_hz(&restored) || restored != host ||
        hpclang_time_read_us(&after) || after < before || after - before < 1000u)
        return -1;
    return 0;
}

static int calendar(void)
{
    struct hpclang_datetime before, after;
    struct tm legacy;
    if (hpclang_datetime_read(&before))
        return -1;
    errno = 0;
    sys_tm_RTC(&legacy);
    if (errno || hpclang_datetime_read(&after))
        return -1;
    if (legacy.tm_year < 100 || legacy.tm_year > 199 || legacy.tm_mon < 0 || legacy.tm_mon > 11 ||
        legacy.tm_isdst != -1)
        return -1;
    const struct hpclang_datetime sampled = {
        (uint32_t)(legacy.tm_year + 1900), (uint32_t)(legacy.tm_mon + 1), (uint32_t)legacy.tm_mday,
        (uint32_t)legacy.tm_hour,          (uint32_t)legacy.tm_min,       (uint32_t)legacy.tm_sec};
    const struct hpclang_datetime year_start = {sampled.year, 1, 1, 0, 0, 0};
    int64_t low, high, value, first;
    unsigned weekday;
    if (hpclang_datetime_to_seconds(&before, &low) || hpclang_datetime_to_seconds(&after, &high) ||
        hpclang_datetime_to_seconds(&sampled, &value) ||
        hpclang_datetime_to_seconds(&year_start, &first) ||
        hpclang_datetime_weekday(&sampled, &weekday))
        return -1;
    return value < low || value > high || legacy.tm_wday != (int)weekday ||
                   legacy.tm_yday != (int)((value - first) / 86400)
               ? -1
               : 0;
}

unsigned hpclang_historical_system(int (*checkpoint)(uint32_t, uint32_t))
{
    if (sys_bcd2bin_nibbles(0x12345678u, -8) != 12345678u ||
        sys_bcd2bin_nibbles(0x12345678u, 2) != 78u ||
        strcmp(sys_bcd2str(0x12345678u), "12345678") || strcmp(sys_bcd2str(0), "0"))
        return 12;
    errno = 0;
    sys_playTone2(1000, 0);
    if (errno)
        return 13;
    sys_playTone2(0, 1);
    if (errno != EINVAL)
        return 13;
    uint64_t tone_start, tone_end;
    if (hpclang_time_read_us(&tone_start))
        return 13;
    errno = 0;
    sys_playTone2(1000, 5);
    if (errno || hpclang_time_read_us(&tone_end) || tone_end < tone_start ||
        tone_end - tone_start < 5000u || tone_end - tone_start > 250000u)
        return 13;
    if (hpclang_time_read_us(&tone_start))
        return 14;
    errno = 0;
    sys_playTone(1000, 1);
    if (errno || hpclang_time_read_us(&tone_end) || tone_end < tone_start ||
        tone_end - tone_start < 10000u || tone_end - tone_start > 250000u)
        return 14;
    if (calendar())
        return 11;
    if (legacy_power())
        return 15;
    uint64_t delay_start, delay_end;
    sys_timer_t legacy_timer = SYS_TIMER_INITIALIZER;
    if (sys_updateTimer(&legacy_timer) != 0 || !legacy_timer.inited)
        return 8;
    int seconds = sys_RTC_seconds();
    if (seconds < 0 || seconds >= 86400 || sys_delta_seconds(86399, 1) != 2 ||
        hpclang_time_read_us(&delay_start))
        return 4;
    sys_sleep(10);
    /* Wide headroom for clock resolution, but reject seconds/microseconds
     * confusion in either direction. */
    if (hpclang_time_read_us(&delay_end) || delay_end < delay_start ||
        delay_end - delay_start < 10000u || delay_end - delay_start > 250000u)
        return 5;
    ULONGLONG first_elapsed = sys_updateTimer(&legacy_timer);
    if (first_elapsed < 10000000u || first_elapsed > 300000000u ||
        legacy_timer.current != first_elapsed)
        return 9;
    if (hpclang_time_read_us(&delay_start))
        return 6;
    sys_waitTicks(7500);
    if (hpclang_time_read_us(&delay_end) || delay_end < delay_start ||
        delay_end - delay_start < 10000u || delay_end - delay_start > 250000u)
        return 7;
    ULONGLONG second_elapsed = sys_updateTimer(&legacy_timer);
    if (second_elapsed < 10000000u || second_elapsed > 300000000u ||
        legacy_timer.current != first_elapsed + second_elapsed)
        return 10;
    enum
    {
        LIMIT = 1000,
        ITERATIONS = 16
    };
    /* Keep the bounded scratch list in the reserved application stack;
     * the report consoles have their own heap allocations. */
    uint32_t primes[LIMIT];
    struct hpclang_console *console = hpclang_console_create(32, 4, &hpclang_benchmark_font);
    struct hpclang_console *reference = hpclang_console_create(32, 2, &hpclang_benchmark_font);
    unsigned result = 1;
    if (!console || !reference)
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
    return result;
}
