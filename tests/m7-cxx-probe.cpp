#include <chrono>

#include <hpclang/display.hpp>
#include <hpclang/time.hpp>

extern "C" int hpclang_m7_cxx_probe(void)
{
    auto surface = hpclang::display_surface();
    if (surface.width() != 131u ||
        (surface.height() != 64u && surface.height() != 80u) ||
        surface.levels() != 2u)
        return 1;

    auto before = hpclang::monotonic_time();
    hpclang::delay(std::chrono::microseconds{1000});
    auto after = hpclang::monotonic_time();

    if (after < before ||
        after - before < std::chrono::microseconds{1000})
        return 2;

    /*
     * Exercise the C++ surface wrapper without changing the visible result:
     * fully clipped drawing must be a no-op.
     */
    surface.set_pixel(-1, -1, 255u);
    surface.fill_rect(-10, -10, 2, 2, 255u);
    return 0;
}
