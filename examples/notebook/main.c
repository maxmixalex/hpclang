#include "app.h"
#include <hpclang/device.h>
#include <hpclang/time.h>
#include <errno.h>
#include <stdlib.h>

extern const struct hpclang_font notebook_font;

int main(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_ui_config capacity = NOTEBOOK_UI_CONFIG;
    size_t bytes = hpclang_ui_storage_size(capacity);
    void *storage = malloc(bytes);
    struct hpclang_ui *ui = storage ? hpclang_ui_init(storage, bytes, capacity) : NULL;
    const struct hpclang_ui_surface_style style = {
        .font = &notebook_font, .foreground = 255, .disabled = 85, .accent = 255, .gap = 2};
    struct hpclang_ui_surface *renderer =
        hpclang_ui_surface_create(device->display_width, device->display_height, 2, &style, 1, 0);
    int result = 1, display = 0;
    if (!ui || !renderer || hpclang_display_begin(2))
        goto done;
    display = 1;
    struct notebook note = {0};
    notebook_init(&note);
    struct hpclang_text_mode mode;
    if (hpclang_text_mode_init(&mode, device->model))
        goto done;
    hpclang_keyboard_events_reset();
    int declare = 1, publish = 0, quit = 0;
    while (!quit) {
        if (declare && notebook_view(ui, renderer, &note, (int)device->display_width,
                                     (int)device->display_height))
            goto done;
        declare = 0;
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
                declare = 1;
                quit = notebook_update(&note, &event);
            }
            if (key.key == HPCLANG_KEY_ON && key.type == HPCLANG_KEYBOARD_EVENT_DOWN && !emitted)
                quit = 1;
        }
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
