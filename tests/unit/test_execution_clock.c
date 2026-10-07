#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <time.h>
#include "execution-clock.h"

int main(void)
{
    /* A 10 ms idle wait may resume much later under contention. */
    assert(hpclang_execution_idle_credit(810000, 100, 10) == 10000);
    /* A timer signal may interrupt that wait early. */
    assert(hpclang_execution_idle_credit(1000, 100, 10) == 900);
    assert(hpclang_execution_idle_credit(100, 100, 10) == 0);
    assert(hpclang_execution_idle_credit(100, 101, 10) == 0);
    assert(hpclang_execution_idle_credit(810000, 100, 0) == 0);

    hpclang_execution_clock_enable();
    int64_t before = hpclang_execution_clock_read();
    struct timespec pause = {0, 200000000};
    while (nanosleep(&pause, &pause) != 0) {
    }
    int64_t after = hpclang_execution_clock_read();
    /* Host suspension must not turn a short tap into a long press. */
    assert(after >= before && after - before < 100000);
    before = after;
    hpclang_execution_clock_poll(20);
    after = hpclang_execution_clock_read();
    /* Guest WFI still advances the clock and can wake hardware timers. */
    assert(after - before >= 19000);
    return 0;
}
