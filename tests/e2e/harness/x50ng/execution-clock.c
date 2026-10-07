#define _POSIX_C_SOURCE 200809L
#include <poll.h>
#include <sys/time.h>
#include <time.h>
#include "execution-clock.h"

static int enabled;
static int64_t idle_us;

static int64_t read_clock(clockid_t kind)
{
    struct timespec value;
    clock_gettime(kind, &value);
    return (int64_t)value.tv_sec * 1000000 + value.tv_nsec / 1000;
}

void hpclang_execution_clock_enable(void)
{
    enabled = 1;
}

int64_t hpclang_execution_clock_read(void)
{
    if (enabled)
        return read_clock(CLOCK_PROCESS_CPUTIME_ID) + idle_us;
    struct timeval value;
    gettimeofday(&value, NULL);
    return (int64_t)value.tv_sec * 1000000 + value.tv_usec;
}

void hpclang_execution_clock_poll(int timeout_ms)
{
    if (!enabled) {
        poll(NULL, 0, timeout_ms);
        return;
    }
    int64_t cpu_before = read_clock(CLOCK_PROCESS_CPUTIME_ID);
    int64_t wall_before = read_clock(CLOCK_MONOTONIC);
    poll(NULL, 0, timeout_ms);
    int64_t wall_after = read_clock(CLOCK_MONOTONIC);
    int64_t cpu_after = read_clock(CLOCK_PROCESS_CPUTIME_ID);
    idle_us +=
        hpclang_execution_idle_credit(wall_after - wall_before, cpu_after - cpu_before, timeout_ms);
}
