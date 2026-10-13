#include <hpclang/ui.hpp>
#include <hpclang/ui-surface.h>
#include <hpclang/device.h>
#include <hpclang/time.hpp>
#include <cstdio>
#include <cstdlib>

extern "C" const hpclang_font counter_font;

static int counter(hpclang_ui *ui, uint32_t key, unsigned value)
{
    hpclang::ui::scope component{ui, key, HPCLANG_UI_COMPONENT};
    if (!component)
        return -1;
    char label[32];
    std::snprintf(label, sizeof(label), "Count: %u", value);
    return hpclang_ui_text(ui, 1, 0, label);
}

static int view(hpclang_ui *ui, hpclang_ui_surface *renderer, hpclang_ui_rect viewport,
                unsigned value)
{
    hpclang::ui::declaration frame{ui};
    if (!frame)
        return -1;
    {
        hpclang::ui::scope column{ui, 1, HPCLANG_UI_COLUMN};
        if (!column || counter(ui, 1, value) || hpclang_ui_button(ui, 2, 0, "Add one", {1, 0}, 1) ||
            hpclang_ui_button(ui, 3, 0, "Reset", {2, 0}, 1) ||
            hpclang_ui_button(ui, 4, 0, "Quit", {3, 0}, 1))
            return -1;
    }
    if (hpclang_ui_layout(ui, viewport, hpclang_ui_surface_measure, renderer))
        return -1;
    return frame.commit();
}

static int run(hpclang_ui *ui, hpclang_ui_surface *renderer,
               const struct hpclang_device_info *device)
{
    hpclang_text_mode mode;
    if (hpclang_text_mode_init(&mode, device->model))
        return 1;
    hpclang_keyboard_events_reset();
    unsigned value = 0;
    bool changed = true, publish = false;
    while (true) {
        if (changed) {
            if (view(ui, renderer,
                     {0, 0, static_cast<int32_t>(device->display_width),
                      static_cast<int32_t>(device->display_height)},
                     value))
                return 1;
            changed = false;
        }
        const int painted = hpclang_ui_surface_render(renderer, ui);
        if (painted < 0)
            return 1;
        publish |= painted > 0;
        if (publish) {
            if (hpclang_ui_surface_copy(renderer, hpclang_display_surface()))
                return 1;
            if (!hpclang_display_present(100000))
                publish = false;
            else if (errno != ETIMEDOUT)
                return 1;
        }
        hpclang_keyboard_event key;
        const int available = hpclang_keyboard_event_poll(&key);
        if (available < 0)
            return 1;
        if (available) {
            hpclang_ui_event event;
            const int emitted = hpclang_ui_key(ui, &mode, &key, &event);
            if (emitted < 0)
                return 1;
            if (key.key == HPCLANG_KEY_ON && key.type == HPCLANG_KEYBOARD_EVENT_DOWN)
                return 0;
            if (emitted > 0 && event.kind == HPCLANG_UI_ACTIVATE) {
                if (event.action.type == 3)
                    return 0;
                value = event.action.type == 1 ? value + 1 : 0;
                changed = true;
            }
        }
        hpclang::delay(hpclang::microseconds{20000});
    }
}

int main()
{
    const auto *device = hpclang_device_info();
    const hpclang_ui_config capacity{8, 96, 0};
    const auto bytes = hpclang_ui_storage_size(capacity);
    void *storage = std::malloc(bytes);
    auto *ui = storage ? hpclang_ui_init(storage, bytes, capacity) : nullptr;
    const hpclang_ui_surface_style style{&counter_font, 255, 0, 128, 255, 1, 1, 1, 0, 0, 0};
    auto *renderer =
        hpclang_ui_surface_create(device->display_width, device->display_height, 2, &style, 1, 0);
    int result = 1;
    if (ui && renderer && hpclang_display_begin(2) == 0) {
        result = run(ui, renderer, device);
        hpclang_display_end();
    }
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    std::free(storage);
    return result;
}
