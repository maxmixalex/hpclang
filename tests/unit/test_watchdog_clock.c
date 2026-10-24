#include <assert.h>
#include "watchdog-clock.h"

int main(void)
{
    struct hpclang_watchdog_clock c = {0, 0, 1000, 1000, 160, 60000000, 1};
    assert(hpclang_watchdog_delay(&c) == 2667);
    assert(!hpclang_watchdog_advance(&c, 2666) && c.count == 1);
    assert(hpclang_watchdog_delay(&c) == 1);
    assert(hpclang_watchdog_advance(&c, 2667) && c.count == 1000);
    assert(c.phase == 20000000 && hpclang_watchdog_delay(&c) == 2667);
    /* One million microseconds is exactly 375 periods, irrespective of how
     * often the guest reads WTCNT. No cumulative per-tick rounding error. */
    c = (struct hpclang_watchdog_clock){0, 0, 1000, 1000, 160, 60000000, 1};
    unsigned hits = 0;
    for (unsigned now = 1; now <= 1000000; ++now)
        hits += hpclang_watchdog_advance(&c, now);
    assert(hits == 375 && c.count == 1000 && !c.phase);
    c.last_us = 0;
    assert(hpclang_watchdog_advance(&c, 1000000) && c.count == 1000 && !c.phase);

    c = (struct hpclang_watchdog_clock){0, 0, 100, 100, 160, 60000000, 1};
    assert(!hpclang_watchdog_advance(&c, 100) && c.count == 63);
    c.hz = 12000000;
    assert(hpclang_watchdog_delay(&c) == 834);
    assert(!hpclang_watchdog_advance(&c, 933));
    assert(hpclang_watchdog_advance(&c, 934));
    c.enabled = 0;
    unsigned count = c.count;
    assert(!hpclang_watchdog_advance(&c, 1000000) && c.count == count);
    assert(hpclang_watchdog_delay(&c) == UINT64_MAX);
    c.enabled = 1;
    c.hz = 0;
    assert(!hpclang_watchdog_advance(&c, 2000000) && c.count == count);
    assert(hpclang_watchdog_delay(&c) == UINT64_MAX);
    c = (struct hpclang_watchdog_clock){0, 0, 0, 0, 16, 16000000, 1};
    assert(hpclang_watchdog_delay(&c) == 1 && !hpclang_watchdog_advance(&c, 0));
    assert(hpclang_watchdog_advance(&c, 1) && c.count == 0);
    assert(hpclang_watchdog_advance(&c, UINT64_MAX));
    assert(!hpclang_watchdog_advance(&c, 0) && c.last_us == UINT64_MAX);
    return 0;
}
