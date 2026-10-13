#include "app.h"
#include <hpclang/device.h>
#include <hpclang/time.h>
#include <errno.h>
#include <stdlib.h>

extern const struct hpclang_font demo_font;

int main(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_ui_config capacity = {64, 1024};
    size_t bytes = hpclang_ui_storage_size(capacity);
    void *storage = malloc(bytes);
    struct hpclang_ui *ui = storage ? hpclang_ui_init(storage, bytes, capacity) : NULL;
    struct hpclang_ui_surface_style styles[2] = {{.font = &demo_font,
                                                  .foreground = 255,
                                                  .background = 0,
                                                  .disabled = 85,
                                                  .accent = 255,
                                                  .gap = 2},
                                                 {.font = &demo_font,
                                                  .foreground = 255,
                                                  .background = 0,
                                                  .disabled = 85,
                                                  .accent = 255,
                                                  .padding_x = 2,
                                                  .padding_y = 1}};
    struct hpclang_ui_surface *renderer =
        hpclang_ui_surface_create(device->display_width, device->display_height, 2, styles, 2, 0);
    int result = 1, display = 0;
    if (!ui || !renderer)
        goto done;
    if (hpclang_display_begin(2))
        goto done;
    display = 1;
    struct demo_app app;
    demo_init(&app, ui);
    struct hpclang_text_mode mode;
    if (hpclang_text_mode_init(&mode, device->model))
        goto done;
    hpclang_keyboard_events_reset();
    int declare = 1, publish = 0;
    while (!app.model.quit) {
        if (declare) {
            if (demo_view(&app, renderer, (int)device->display_width, (int)device->display_height))
                goto done;
            declare = 0;
        }
        int painted = hpclang_ui_surface_render(renderer, ui);
        if (painted < 0)
            goto done;
        publish |= painted > 0;
        if (publish) {
            if (hpclang_ui_surface_copy(renderer, hpclang_display_surface()))
                goto done;
            if (!hpclang_display_present(100000))
                publish = 0;
            else if (errno != ETIMEDOUT)
                goto done;
        }
        struct hpclang_keyboard_event key;
        int available = hpclang_keyboard_event_poll(&key);
        if (available < 0)
            goto done;
        if (available) {
            struct hpclang_ui_event event;
            int emitted = hpclang_ui_key(ui, &mode, &key, &event);
            if (emitted < 0 && errno != ENOSPC)
                goto done;
            if (emitted > 0) {
                int changed = demo_update(&app, &event);
                if (changed < 0)
                    goto done;
                declare |= changed > 0;
            }
            if (key.key == HPCLANG_KEY_ON && key.type == HPCLANG_KEYBOARD_EVENT_DOWN &&
                emitted == 0)
                app.model.quit = 1;
        }
        int changed = demo_poll(&app);
        if (changed < 0)
            goto done;
        declare |= changed > 0;
        hpclang_delay_us(20000);
    }
    result = 0;
done:
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    if (display)
        hpclang_display_end();
    free(storage);
    return result;
}
