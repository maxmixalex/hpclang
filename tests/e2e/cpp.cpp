#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/display.hpp>
#include <hpclang/font.h>
#include <hpclang/object.h>
#include <hpclang/time.hpp>
#include <saturn.h>

extern "C" const struct hpclang_font hpclang_e2e_ui_font;

static const char hpclang_e2e_ui_probe[] = HPCLANG_UI_TEXT("Hp");

volatile int hpclang_e2e_cxx_ctor_probe;

struct hpclang_e2e_probe
{
    hpclang_e2e_probe()
    {
        hpclang_e2e_cxx_ctor_probe = 42;
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

static bool hpclang_e2e_restore_stack(uint32_t before)
{
    uint32_t current = 0u;
    if (hpclang_stack_depth(&current) != 0 || current < before)
        return false;
    if (current > before && hpclang_stack_drop(current - before) != 0)
        return false;
    current = 0u;
    return hpclang_stack_depth(&current) == 0 && current == before;
}

static int hpclang_e2e_saturn_compat_test()
{
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (device == nullptr)
        return 1;

    const bool rpl_family = device->model == HPCLANG_MODEL_HP48GII ||
                            device->model == HPCLANG_MODEL_HP49GPLUS ||
                            device->model == HPCLANG_MODEL_HP50G;
    const bool live_stock_view = rpl_family && boot != nullptr && boot->saturn_state != 0u;
    if (!live_stock_view)
        return 0;

    uint32_t before = 0u;
    if (hpclang_stack_depth(&before) != 0)
        return 2;

    if (sat_push_zint_llong(21) != TRUE) {
        (void)hpclang_e2e_restore_stack(before);
        return 3;
    }
    uint32_t depth = 0u;
    if (hpclang_stack_depth(&depth) != 0 || depth != before + 1u || sat_pop_zint_llong() != 21 ||
        !hpclang_e2e_restore_stack(before)) {
        (void)hpclang_e2e_restore_stack(before);
        return 3;
    }

    if (sat_push_real(1.5) != TRUE) {
        (void)hpclang_e2e_restore_stack(before);
        return 4;
    }
    double real = sat_pop_real();
    if (real < 1.49999999999 || real > 1.50000000001 || !hpclang_e2e_restore_stack(before)) {
        (void)hpclang_e2e_restore_stack(before);
        return 4;
    }

    if (sat_stack_push_string("compat") != 1) {
        (void)hpclang_e2e_restore_stack(before);
        return 5;
    }
    char string_buffer[16];
    if (sat_pop_str(string_buffer) != string_buffer || std::strcmp(string_buffer, "compat") != 0 ||
        !hpclang_e2e_restore_stack(before)) {
        (void)hpclang_e2e_restore_stack(before);
        return 5;
    }

    /*
     * stdargs intentionally consumes the complete historical RPL stack. Only
     * exercise its successful path when the incoming user stack was empty; a
     * non-empty Stock stack must never be cleared just to make an E2E witness.
     */
    if (before == 0u) {
        if (sat_stack_push_string("'left'") != 1 || sat_stack_push_string("right") != 1) {
            (void)hpclang_e2e_restore_stack(before);
            return 6;
        }

        char **argv = nullptr;
        int argc = sat_stack_pop_stdargs(&argv);
        bool args_ok = argc == 2 && argv != nullptr && std::strcmp(argv[0], "left") == 0 &&
                       std::strcmp(argv[1], "right") == 0;
        if (argv != nullptr) {
            if (argc > 0)
                std::free(argv[0]);
            if (argc > 1)
                std::free(argv[1]);
            std::free(argv);
        }
        if (!args_ok || !hpclang_e2e_restore_stack(before)) {
            (void)hpclang_e2e_restore_stack(before);
            return 6;
        }
    }

    return 0;
}

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

    hpclang_surface *owned_raw = hpclang_surface_create_mono(24u, 16u);
    if (owned_raw == nullptr)
        return 4;

    hpclang::surface_ref owned{owned_raw};

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
        hpclang_surface_destroy(owned_raw);
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
        hpclang_surface_destroy(owned_raw);
        return 9;
    }

    owned.clear(0u);
    owned.draw_line(0, 0, 1, 1, 255u);
    if (owned.get_pixel(0, 0) != 255u || owned.get_pixel(1, 1) != 255u) {
        hpclang_surface_destroy(owned_raw);
        return 5;
    }

    owned.blit_from(owned, 1, 0, 0, 0, 1, 1);
    if (owned.get_pixel(1, 0) != 255u) {
        hpclang_surface_destroy(owned_raw);
        return 5;
    }

    owned.clear(0u);
    owned.draw_circle(5, 5, 3, 255u);
    if (owned.get_pixel(5, 2) != 255u || owned.get_pixel(5, 5) != 0u) {
        hpclang_surface_destroy(owned_raw);
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
        hpclang_surface_destroy(owned_raw);
        return 7;
    }
    hpclang_surface_destroy(owned_raw);

    hpclang_surface *gray_raw = hpclang_surface_create(4u, 1u, 4u);
    if (gray_raw == nullptr)
        return 8;
    hpclang::surface_ref gray{gray_raw};
    gray.set_pixel(0, 0, 0u);
    gray.set_pixel(1, 0, 64u);
    gray.set_pixel(2, 0, 128u);
    gray.set_pixel(3, 0, 255u);
    if (gray.levels() != 4u || gray.get_pixel(0, 0) != 0u || gray.get_pixel(1, 0) != 85u ||
        gray.get_pixel(2, 0) != 170u || gray.get_pixel(3, 0) != 255u) {
        hpclang_surface_destroy(gray_raw);
        return 8;
    }
    hpclang_surface_destroy(gray_raw);

    if (hpclang_e2e_saturn_compat_test() != 0)
        return 10;

    return 0;
}
