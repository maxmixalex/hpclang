#include "../../../examples/declarative-ui-cpp/app.hpp"
#include <hpclang/ui.hpp>
#include <hpclang/display.hpp>
#include <hpclang/device.h>
#include <cstdlib>

extern "C" const hpclang_font demo_font;

static unsigned exercise(hpclang_ui *ui, hpclang_ui_surface *renderer, uint32_t width,
                         uint32_t height)
{
    const hpclang_ui_rect viewport{0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)};
    if (counter_view(ui, renderer, viewport, 0) || hpclang_ui_surface_render(renderer, ui) != 1)
        return 0x1902;
    hpclang::owned_surface image{width, height};
    if (!image || hpclang_ui_surface_copy(renderer, image.get()))
        return 0x1903;
    const auto focused = hpclang_ui_focused(ui);
    hpclang_ui_event event;
    if (!focused || hpclang_ui_activate(ui, &event) != 1 || event.action.type != 1)
        return 0x1904;
    {
        hpclang::ui::declaration abandoned{ui};
        if (!abandoned)
            return 0x1905;
        hpclang::ui::scope column{ui, 88, HPCLANG_UI_COLUMN};
        if (!column || hpclang_ui_text(ui, 1, 0, "discard"))
            return 0x1906;
    }
    if (hpclang_ui_focused(ui) != focused || counter_view(ui, renderer, viewport, 9) ||
        hpclang_ui_focused(ui) != focused || hpclang_ui_surface_render(renderer, ui) != 1)
        return 0x1907;
    hpclang::owned_surface changed{width, height};
    if (!changed || hpclang_ui_surface_copy(renderer, changed.get()))
        return 0x1908;
    unsigned differences = 0;
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            differences += image.view().get_pixel(x, y) != changed.view().get_pixel(x, y);
    auto moved = std::move(changed);
    if (!differences || changed || !moved)
        return 0x1909;
    if (hpclang_ui_focus_step(ui, 1) || hpclang_ui_activate(ui, &event) != 1 ||
        event.action.type != 2)
        return 0x190a;
    return 0;
}

extern "C" unsigned hpclang_cpp_ui_test(void)
{
    const auto *device = hpclang_device_info();
    const hpclang_ui_config capacity{8, 96, 0};
    const auto bytes = hpclang_ui_storage_size(capacity);
    void *storage = std::malloc(bytes);
    auto *ui = storage ? hpclang_ui_init(storage, bytes, capacity) : nullptr;
    const hpclang_ui_surface_style style{&demo_font, 255, 0, 128, 255, 1, 1, 1, 0, 0, 0};
    auto *renderer =
        hpclang_ui_surface_create(device->display_width, device->display_height, 2, &style, 1, 0);
    unsigned result = ui && renderer
                          ? exercise(ui, renderer, device->display_width, device->display_height)
                          : 0x1901;
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    std::free(storage);
    return result;
}
