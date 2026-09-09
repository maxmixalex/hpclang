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
    if (surface.get_pixel(-1, -1) != 0u)
        return 3;

    surface.set_pixel(-1, -1, 255u);
    surface.draw_rect(-10, -10, 2, 2, 255u);
    surface.fill_rect(-10, -10, 2, 2, 255u);

    hpclang_surface *owned_raw = hpclang_surface_create_mono(2u, 2u);
    if (owned_raw == nullptr)
        return 4;

    hpclang::surface_ref owned{owned_raw};
    owned.set_pixel(0, 0, 255u);
    owned.blit_from(owned, 1, 0, 0, 0, 1, 1);
    if (owned.get_pixel(1, 0) != 255u) {
        hpclang_surface_destroy(owned_raw);
        return 5;
    }
    hpclang_surface_destroy(owned_raw);
    return 0;
}
