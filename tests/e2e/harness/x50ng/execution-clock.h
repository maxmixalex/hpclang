#ifndef HPCLANG_EXECUTION_CLOCK_H
#define HPCLANG_EXECUTION_CLOCK_H

#include <stdint.h>

/* Enable before creating hardware timers. Interactive frontends keep wall time. */
void hpclang_execution_clock_enable(void);
int64_t hpclang_execution_clock_read(void);
void hpclang_execution_clock_poll(int timeout_ms);

/* Credit intentional idle waiting, never scheduler delay or CPU time twice. */
static inline int64_t hpclang_execution_idle_credit(int64_t wall_us, int64_t cpu_us, int timeout_ms)
{
    int64_t budget = (int64_t)timeout_ms * 1000;
    int64_t waited = wall_us - cpu_us;
    if (budget <= 0 || waited <= 0)
        return 0;
    return waited < budget ? waited : budget;
}

#endif
