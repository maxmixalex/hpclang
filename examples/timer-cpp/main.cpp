#include <hpconio.h>
#include <hpclang/timer.hpp>
#include <cstdio>
#include <utility>

int main()
{
    hpclang::cooperative_timer schedule;
    if (schedule.start(hpclang::microseconds{1000000}, hpclang::microseconds{1000000}))
        return 1;
    auto timer = std::move(schedule); // Only timer now owns the repeating schedule.
    unsigned remaining = 5;
    while (remaining) {
        uint64_t expirations;
        if (timer.poll(expirations))
            return 1;
        if (expirations) {
            remaining -= expirations >= remaining ? remaining : static_cast<unsigned>(expirations);
            if (std::printf("%u / 5 timer ticks\n", 5u - remaining) < 0)
                return 1;
        }
        hpclang::delay(hpclang::microseconds{1000});
    }
    return 0;
}
