#include <hpclang/timer.hpp>
#include <utility>

extern "C" unsigned hpclang_e2e_timer_cpp_test(void)
{
    using hpclang::microseconds;
    uint64_t count = 99;
    hpclang::cooperative_timer original;
    if (original.start(microseconds{1000}))
        return 1;
    auto moved = std::move(original);
    if (original.armed() || original.poll(count) || count)
        return 2;
    if (moved.start(microseconds{-1}) != -1 || errno != EINVAL || !moved.armed())
        return 3;
    hpclang::delay(microseconds{2000});
    if (moved.poll(count) || count != 1 || moved.armed())
        return 4;
    if (moved.poll(count) || count)
        return 5;
    if (original.start(microseconds{0}, microseconds{1000}) || moved.start(microseconds{1000000}))
        return 6;
    moved = std::move(original);
    hpclang::delay(microseconds{3000});
    if (original.armed() || moved.poll(count) || count < 3 || !moved.armed())
        return 7;
    moved.cancel();
    if (moved.armed() || moved.poll(count) || count)
        return 8;
    errno = ERANGE;
    {
        hpclang::cooperative_timer cleanup;
        if (cleanup.start(microseconds{1000000}))
            return 9;
        errno = ERANGE;
    }
    return errno == ERANGE ? 0 : 10;
}
