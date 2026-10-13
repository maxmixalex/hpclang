#include "app.h"
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

extern const struct hpclang_font demo_font;
volatile uint32_t hpclang_e2e_result;
volatile uint32_t hpclang_ui_witness[5];
static unsigned key_failure;

static int frame(unsigned id)
{
    hpclang_e2e_result = 0x46520000u | id;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != (0x464b0000u | id))
        if (hpclang_time_monotonic_us() - start > 2000000)
            return -1;
    return 0;
}
static int view(struct demo_app *app, struct hpclang_ui_surface *r)
{
    const struct hpclang_device_info *d = hpclang_device_info();
    return demo_view(app, r, (int)d->display_width, (int)d->display_height);
}
static int press(struct demo_app *app, struct hpclang_ui_surface *r, struct hpclang_text_mode *mode,
                 unsigned scan)
{
    for (unsigned edge = 0; edge < 2; ++edge) {
        unsigned down = !edge, argument = scan | (down << 8);
        hpclang_e2e_result = 0x4b490000u | argument;
        uint64_t start = hpclang_time_monotonic_us();
        while (hpclang_e2e_result != (0x4b4a0000u | argument))
            if (hpclang_time_monotonic_us() - start > 2000000) {
                key_failure = 0x10000u | argument;
                return -1;
            }
        struct hpclang_keyboard_event key;
        int got;
        do {
            got = hpclang_keyboard_event_poll(&key);
            if (got < 0 || hpclang_time_monotonic_us() - start > 2000000) {
                key_failure = 0x20000u | argument;
                return -1;
            }
        } while (!got);
        if (key.scan_code != scan ||
            key.type != (down ? HPCLANG_KEYBOARD_EVENT_DOWN : HPCLANG_KEYBOARD_EVENT_UP)) {
            key_failure = 0x30000u | ((unsigned)key.type << 8) | key.scan_code;
            return -1;
        }
        struct hpclang_ui_event event;
        int result = hpclang_ui_key(app->ui, mode, &key, &event);
        if (result < 0)
            return -1;
        if (result > 0) {
            result = demo_update(app, &event);
            if (result < 0 || (result && view(app, r)))
                return -1;
        }
    }
    return 0;
}
static int activate(struct demo_app *app, struct hpclang_ui_surface *r,
                    struct hpclang_text_mode *mode, enum demo_action action)
{
    for (size_t i = 0; i < hpclang_ui_node_count(app->ui); ++i) {
        const struct hpclang_ui_node *node = hpclang_ui_node_at(app->ui, i);
        if (node->action.type == (uint32_t)action) {
            if (hpclang_ui_focus(app->ui, node->id))
                return -1;
            return press(app, r, mode, 6);
        }
    }
    return -1;
}
static int capture(struct demo_app *app, struct hpclang_ui_surface *r, unsigned id)
{
    if (view(app, r) || hpclang_ui_surface_render(r, app->ui) < 0 ||
        hpclang_ui_surface_copy(r, hpclang_display_surface()) || hpclang_display_present(100000))
        return -1;
    return frame(id);
}
static unsigned checks(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    struct hpclang_ui_config config = {DEMO_UI_NODES, DEMO_UI_TEXT_BYTES, DEMO_UI_EFFECTS};
    size_t bytes = hpclang_ui_storage_size(config);
    hpclang_ui_witness[0] = (uint32_t)bytes;
    hpclang_ui_witness[1] = hpclang_boot_info()->rw_bytes;
    void *storage = malloc(bytes);
    if (!storage)
        return 1;
    struct hpclang_ui *ui = hpclang_ui_init(storage, bytes, config);
    struct hpclang_ui_surface *r =
        demo_renderer(device->display_width, device->display_height, 2, &demo_font, 0);
    unsigned failure = 2;
    if (!ui || !r)
        goto done;
    struct demo_app app;
    demo_init(&app, ui);
    failure = 3;
    if (demo_view(&app, r, (int)device->display_width, (int)device->display_height))
        goto done;
    hpclang_ui_witness[2] = (uint32_t)hpclang_ui_node_count(ui);
    failure = 4;
    if (hpclang_display_begin(2))
        goto done;
    uint64_t start = hpclang_time_monotonic_us();
    failure = 5;
    if (hpclang_ui_surface_render(r, ui) != 1)
        goto done;
    hpclang_ui_witness[3] = (uint32_t)(hpclang_time_monotonic_us() - start);
    failure = 6;
    if (hpclang_ui_surface_copy(r, hpclang_display_surface()) || hpclang_display_present(100000))
        goto done;
    failure = 7;
    if (frame(1))
        goto done;
    failure = 8;
    if (hpclang_ui_surface_render(r, ui) != 0)
        goto done;
    struct hpclang_text_mode mode;
    failure = 9;
    if (hpclang_text_mode_init(&mode, device->model))
        goto done;
    /* Stock can enter the application while its launch key is still held.
     * Hand off the physical keyboard only after that real key-up, rather than
     * confusing it with the first scripted transition or dropping events. */
    start = hpclang_time_monotonic_us();
    for (;;) {
        struct hpclang_keyboard_state keys;
        hpclang_keyboard_read(&keys);
        if (!hpclang_keyboard_any_is_down(&keys))
            break;
        if (hpclang_time_monotonic_us() - start > 2000000)
            goto done;
    }
    hpclang_keyboard_events_reset();
    failure = 10;
    if (press(&app, r, &mode, 6) || !app.model.connected)
        goto done;
    /* Replacing Connect retires its lifetime and falls forward to the filter.
     * Traverse down and back before typing, exercising real navigation. */
    failure = 0x1101;
    if (press(&app, r, &mode, 44))
        goto done;
    failure = 0x1102;
    if (press(&app, r, &mode, 42))
        goto done;
    failure = 0x1103;
    if (press(&app, r, &mode, 17))
        goto done;
    failure = 0x1104;
    if (app.model.filter[0] != '8')
        goto done;
    failure = 12;
    if (hpclang_ui_select(ui, 0, 1) || press(&app, r, &mode, 0) || app.model.filter[0])
        goto done;
    failure = 13;
    if (activate(&app, r, &mode, DEMO_SCAN) || app.model.scan != DEMO_LOADING ||
        capture(&app, r, 2))
        goto done;
    failure = 14;
    for (unsigned i = 0; i < 20; ++i)
        if (demo_poll(&app) < 0)
            goto done;
    if (app.model.scan != DEMO_ERROR || capture(&app, r, 3))
        goto done;
    failure = 15;
    if (activate(&app, r, &mode, DEMO_SCAN))
        goto done;
    for (unsigned i = 0; i < 20; ++i)
        if (demo_poll(&app) < 0)
            goto done;
    if (app.model.scan != DEMO_READY || app.model.count != 3 || capture(&app, r, 4))
        goto done;
    failure = 0x1501;
    if (activate(&app, r, &mode, DEMO_TOGGLE) || app.model.devices[0].enabled)
        goto done;
    failure = 16;
    for (unsigned i = 3; i < 8; ++i)
        if (activate(&app, r, &mode, DEMO_ADD))
            goto done;
    if (app.model.count != 8 || activate(&app, r, &mode, DEMO_REVERSE) || capture(&app, r, 5))
        goto done;
    hpclang_ui_witness[2] = (uint32_t)hpclang_ui_node_count(ui);
    failure = 17;
    if (activate(&app, r, &mode, DEMO_WAVE) || capture(&app, r, 6))
        goto done;
    failure = 0x1701;
    uint32_t focused = hpclang_ui_focused(ui);
    hpclang_ui_surface_destroy(r);
    r = demo_renderer(device->display_width, device->display_height, 2, &demo_font, 1);
    if (!r || capture(&app, r, 7) || hpclang_ui_focused(ui) != focused)
        goto done;
    failure = 0x1702;
    const struct hpclang_font *font = hpclang_font_default();
    if (!hpclang_font_available(font))
        font = &demo_font;
    hpclang_ui_surface_destroy(r);
    r = demo_renderer(device->display_width, device->display_height, 2, font, 1);
    /* The checkpoint identifies the selected font's line height. The host
     * independently checks the resulting canvas geometry and waveform. */
    if (!r || capture(&app, r, 100 + hpclang_font_height(font)) ||
        hpclang_ui_focused(ui) != focused)
        goto done;
    failure = 18;
    if (activate(&app, r, &mode, DEMO_SCAN) || demo_poll(&app) < 0 ||
        activate(&app, r, &mode, DEMO_CANCEL) || app.model.scan != DEMO_IDLE || app.cleanups != 3)
        goto done;
    failure = 0;
done:
    hpclang_ui_witness[4] = (uint32_t)errno;
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(r);
    hpclang_display_end();
    free(storage);
    return failure;
}
int main(void)
{
    unsigned failure = checks();
    struct hpclang_surface *screen = hpclang_display_surface();
    hpclang_surface_clear(screen, 0);
    char line[48];
    snprintf(line, sizeof(line), "POOL %lu RW %lu", (unsigned long)hpclang_ui_witness[0],
             (unsigned long)hpclang_ui_witness[1]);
    hpclang_surface_draw_text(screen, &demo_font, 0, 0, line, 255);
    snprintf(line, sizeof(line), "N %lu US %lu", (unsigned long)hpclang_ui_witness[2],
             (unsigned long)hpclang_ui_witness[3]);
    hpclang_surface_draw_text(screen, &demo_font, 0, 8, line, 255);
    snprintf(line, sizeof(line), "ERR %lu KEY %lx", (unsigned long)hpclang_ui_witness[4],
             (unsigned long)key_failure);
    hpclang_surface_draw_text(screen, &demo_font, 0, 16, line, 255);
    hpclang_surface_draw_text(screen, &demo_font, 20, 32, failure ? "UI FAIL" : "UI PASS", 255);
    hpclang_e2e_result = failure ? 0x46410000u | failure : 0x50415353u;
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hpclang_delay_us(300000);
    return 0;
}
