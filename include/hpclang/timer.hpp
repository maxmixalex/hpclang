#ifndef HPCLANG_TIMER_HPP
#define HPCLANG_TIMER_HPP

#include <cerrno>
#include <utility>
#include <hpclang/time.hpp>
#include <hpclang/timer.h>

namespace hpclang {

/* C++17, allocation-free cooperative schedule. Poll from the foreground loop;
 * there is no interrupt, callback or background thread. Moving transfers the
 * schedule and disarms the source. Replacement discards the destination's old
 * schedule. Clock/overflow failures retain the schedule and poll output.
 * Durations use signed chrono microseconds; negative values are EINVAL.
 * The C API remains available for the full unsigned duration range. */
class cooperative_timer {
  public:
    cooperative_timer() noexcept = default;
    cooperative_timer(const cooperative_timer &) = delete;
    cooperative_timer &operator=(const cooperative_timer &) = delete;
    cooperative_timer(cooperative_timer &&other) noexcept : timer_(std::exchange(other.timer_, {}))
    {
    }
    cooperative_timer &operator=(cooperative_timer &&other) noexcept
    {
        if (this != &other)
            timer_ = std::exchange(other.timer_, {});
        return *this;
    }
    ~cooperative_timer()
    {
        cancel();
    }
    [[nodiscard]] int start(microseconds delay, microseconds period = microseconds{0}) noexcept
    {
        if (delay.count() < 0 || period.count() < 0) {
            errno = EINVAL;
            return -1;
        }
        return ::hpclang_timer_start(&timer_, static_cast<uint64_t>(delay.count()),
                                     static_cast<uint64_t>(period.count()));
    }
    [[nodiscard]] int poll(uint64_t &count) noexcept
    {
        return ::hpclang_timer_poll(&timer_, &count);
    }
    void cancel() noexcept
    {
        ::hpclang_timer_cancel(&timer_);
    }
    /* A due one-shot stays armed until polled; this does not sample the clock. */
    [[nodiscard]] bool armed() const noexcept
    {
        return timer_.armed != 0;
    }

  private:
    hpclang_timer timer_{};
};

} // namespace hpclang
#endif
