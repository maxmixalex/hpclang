#include <array>
#include <chrono>
#include <concepts>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <hpclang/display.hpp>
#include <hpclang/time.hpp>

volatile int hpclang_e2e_cxx_ctor_probe;

struct hpclang_e2e_probe {
    hpclang_e2e_probe() { hpclang_e2e_cxx_ctor_probe = 42; }
};
static hpclang_e2e_probe hpclang_e2e_global_probe;

template <typename T>
concept hpclang_addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template <hpclang_addable T>
constexpr T hpclang_add(T a, T b) { return a + b; }

consteval int hpclang_compile_time_answer() { return 42; }

extern "C" int hpclang_e2e_cpp_test(void)
{
    static_assert(__cplusplus > 202002L);
    static_assert(hpclang_compile_time_answer() == 42);
    static_assert(hpclang_add(20, 22) == 42);

    constexpr std::array<int, 4> expected{10, 20, 30, 40};
    std::span<const int> view(expected);
    std::string value = "hp";
    value += "clang";
    std::vector<int> dynamic{10, 20, 30, 40};
    std::optional<int> answer = hpclang_add(dynamic[1], 22);
    std::string_view name(value);

    int sum = 0;
    for (int item : view)
        sum += item;

    if (hpclang_e2e_cxx_ctor_probe != 42 ||
        name != "hpclang" || answer.value_or(0) != 42 ||
        sum != 100 || dynamic.size() != 4)
        return 6;

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
    owned.clear(0u);
    owned.draw_line(0, 0, 1, 1, 255u);
    if (owned.get_pixel(0, 0) != 255u ||
        owned.get_pixel(1, 1) != 255u) {
        hpclang_surface_destroy(owned_raw);
        return 5;
    }

    owned.blit_from(owned, 1, 0, 0, 0, 1, 1);
    if (owned.get_pixel(1, 0) != 255u) {
        hpclang_surface_destroy(owned_raw);
        return 5;
    }
    hpclang_surface_destroy(owned_raw);
    return 0;
}
