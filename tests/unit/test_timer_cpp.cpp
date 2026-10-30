#include <hpclang/timer.hpp>
#include <cassert>
#include <type_traits>

static uint64_t now;
static unsigned reads;
static bool clock_ok = true;
extern "C" int hpclang_time_read_us(uint64_t *value)
{
    ++reads;
    if (!clock_ok) {
        errno = EIO;
        return -1;
    }
    *value = now;
    return 0;
}

using hpclang::cooperative_timer;
using hpclang::microseconds;
static_assert(!std::is_copy_constructible_v<cooperative_timer>);
static_assert(!std::is_copy_assignable_v<cooperative_timer>);
static_assert(std::is_nothrow_move_constructible_v<cooperative_timer>);
static_assert(std::is_nothrow_move_assignable_v<cooperative_timer>);
static_assert(std::is_nothrow_destructible_v<cooperative_timer>);

int main()
{
    uint64_t count = 99;
    cooperative_timer source;
    assert(!source.armed() && source.poll(count) == 0 && count == 0 && reads == 0);
    now = 100;
    assert(source.start(microseconds{20}, microseconds{10}) == 0);
    cooperative_timer owner(std::move(source));
    clock_ok = false;
    unsigned before = reads;
    assert(!source.armed() && source.poll(count) == 0 && count == 0 && reads == before);
    count = 99;
    assert(owner.poll(count) == -1 && errno == EIO && count == 99 && owner.armed());
    assert(owner.start(microseconds{1}) == -1 && errno == EIO);
    before = reads;
    assert(owner.start(microseconds{-1}) == -1 && errno == EINVAL && reads == before);
    assert(owner.start(microseconds{1}, microseconds{-1}) == -1 && errno == EINVAL);
    clock_ok = true;
    now = UINT64_MAX - 5;
    assert(owner.start(microseconds{10}) == -1 && errno == EOVERFLOW);
    now = 157;
    assert(owner.poll(count) == 0 && count == 4); // 120, 130, 140, 150: failures retained schedule.
    cooperative_timer destination;
    assert(destination.start(microseconds{0}) == 0);
    destination = std::move(owner);
    assert(!owner.armed());
    assert(destination.poll(count) == 0 && count == 0); // Old immediate schedule was discarded.
    auto *alias = &destination;
    destination = std::move(*alias);
    now = 160;
    assert(destination.poll(count) == 0 && count == 1);
    destination.cancel();
    destination.cancel();
    clock_ok = false;
    before = reads;
    assert(destination.poll(count) == 0 && count == 0 && !destination.armed() && reads == before);
    clock_ok = true;
    assert(destination.start(microseconds{0}) == 0 && destination.armed());
    assert(destination.poll(count) == 0 && count == 1 && !destination.armed());
    assert(destination.poll(count) == 0 && count == 0);
    now = 0;
    assert(destination.start(microseconds{0}, microseconds{1}) == 0);
    now = UINT64_MAX;
    count = 99;
    assert(destination.poll(count) == -1 && errno == EOVERFLOW && count == 99);
    now = 1;
    assert(destination.poll(count) == 0 && count == 2);
    errno = ERANGE;
    {
        cooperative_timer cleanup(std::move(destination));
        assert(cleanup.armed());
    }
    assert(errno == ERANGE && !destination.armed());
}
