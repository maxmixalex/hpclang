#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <hpclang/display.hpp>
#include <hpclang/font.h>
#include <hpclang/object.hpp>
#include <hpclang/time.hpp>

extern "C" const struct hpclang_font hpclang_e2e_ui_font;

static const char hpclang_e2e_ui_probe[] = HPCLANG_UI_TEXT("Hp");

volatile int hpclang_e2e_cxx_ctor_probe;
extern "C" void hpclang_e2e_exit_event(unsigned event);

struct hpclang_e2e_probe
{
    hpclang_e2e_probe()
    {
        hpclang_e2e_cxx_ctor_probe = 42;
    }
    ~hpclang_e2e_probe()
    {
        hpclang_e2e_exit_event(4u);
    }
};
static hpclang_e2e_probe hpclang_e2e_global_probe;

template <typename T>
concept hpclang_addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template <hpclang_addable T> constexpr T hpclang_add(T a, T b)
{
    return a + b;
}

consteval int hpclang_compile_time_answer()
{
    return 42;
}

extern "C" int hpclang_e2e_cpp_test(void)
{
    static_assert(__cplusplus > 202002L);
    static_assert(hpclang_compile_time_answer() == 42);
    static_assert(hpclang_add(20, 22) == 42);

    hpclang::owned_object snapshot;
    if (snapshot.receive([](auto **out) {
            return hpclang_object_create_integer_i64(INT64_C(-123456789), out);
        }) != 0)
        return 9;
    hpclang::owned_object moved{std::move(snapshot)};
    std::int64_t decoded = 0;
    if (snapshot || hpclang_object_integer_i64(moved.get(), &decoded) != 0 || decoded != -123456789)
        return 9;
    if (moved.receive([](auto **out) { return hpclang_object_create_string(nullptr, 1, out); }) !=
            -1 ||
        errno != EFAULT || hpclang_object_integer_i64(moved.get(), &decoded) != 0 ||
        decoded != -123456789)
        return 9;
    moved.reset();

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

    if (hpclang_e2e_cxx_ctor_probe != 42 || name != "hpclang" || answer.value_or(0) != 42 ||
        sum != 100 || dynamic.size() != 4)
        return 6;

    auto surface = hpclang::display_surface();
    if (surface.width() != 131u || (surface.height() != 64u && surface.height() != 80u) ||
        surface.levels() != 2u)
        return 1;

    auto before = hpclang::monotonic_time();
    hpclang::delay(std::chrono::microseconds{1000});
    auto after = hpclang::monotonic_time();

    if (after < before || after - before < std::chrono::microseconds{1000})
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

    hpclang::owned_surface image{24u, 16u};
    if (!image)
        return 4;

    hpclang::owned_surface owner{std::move(image)};
    if (image || !owner)
        return 5;
    auto *owned_raw = owner.get();
    auto owned = owner.view();

    /*
     * This font is produced by hpclang_target_font(STATIC_AUTO) from the
     * HPCLANG_UI_TEXT("Hp") marker above. Touch metrics and rasterize it on an
     * off-screen surface so the calculator matrix proves the generated .S
     * glyph data, relocations, descriptor and bitmap renderer all work at
     * runtime without changing the visible E2E evidence screen.
     */
    if (hpclang_font_height(&hpclang_e2e_ui_font) == 0u ||
        hpclang_font_codepoint_advance(&hpclang_e2e_ui_font, 'H') == 0u ||
        hpclang_font_codepoint_advance(&hpclang_e2e_ui_font, 'p') == 0u) {
        return 9;
    }

    owned.clear(0u);
    hpclang_surface_draw_text(owned_raw, &hpclang_e2e_ui_font, 0, 0, hpclang_e2e_ui_probe, 255u);
    bool generated_font_ink = false;
    for (int y = 0; y < 16 && !generated_font_ink; ++y) {
        for (int x = 0; x < 24; ++x) {
            if (owned.get_pixel(x, y) != 0u) {
                generated_font_ink = true;
                break;
            }
        }
    }
    if (!generated_font_ink) {
        return 9;
    }

    owned.clear(0u);
    owned.draw_line(0, 0, 1, 1, 255u);
    if (owned.get_pixel(0, 0) != 255u || owned.get_pixel(1, 1) != 255u) {
        return 5;
    }

    owned.blit_from(owned, 1, 0, 0, 0, 1, 1);
    if (owned.get_pixel(1, 0) != 255u) {
        return 5;
    }

    owned.clear(0u);
    owned.draw_circle(5, 5, 3, 255u);
    if (owned.get_pixel(5, 2) != 255u || owned.get_pixel(5, 5) != 0u) {
        return 7;
    }

    constexpr std::array<hpclang_point, 3> triangle{{
        {1, 1},
        {9, 1},
        {5, 9},
    }};
    owned.clear(0u);
    owned.fill_polygon(triangle, 255u);
    if (owned.get_pixel(5, 4) != 255u || owned.get_pixel(0, 10) != 0u) {
        return 7;
    }
    owner.reset();

    hpclang::owned_surface gray_image{4u, 1u, 4u};
    if (!gray_image)
        return 8;
    auto gray = gray_image.view();
    gray.set_pixel(0, 0, 0u);
    gray.set_pixel(1, 0, 64u);
    gray.set_pixel(2, 0, 128u);
    gray.set_pixel(3, 0, 255u);
    if (gray.levels() != 4u || gray.get_pixel(0, 0) != 0u || gray.get_pixel(1, 0) != 85u ||
        gray.get_pixel(2, 0) != 170u || gray.get_pixel(3, 0) != 255u) {
        return 8;
    }

    return 0;
}
