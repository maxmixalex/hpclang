#ifndef HPCLANG_TIME_HPP
#define HPCLANG_TIME_HPP

#include <chrono>
#include <cstdint>

#include <hpclang/time.h>

namespace hpclang {

using microseconds = std::chrono::microseconds;

[[nodiscard]] inline microseconds monotonic_time() noexcept
{
    return microseconds{static_cast<microseconds::rep>(::hpclang_time_monotonic_us())};
}

inline void delay(microseconds duration) noexcept
{
    if (duration.count() > 0)
        ::hpclang_delay_us(static_cast<uint64_t>(duration.count()));
}

} // namespace hpclang

#endif
