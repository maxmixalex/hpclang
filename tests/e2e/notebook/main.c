#include "../../../examples/notebook/app.h"
#include <hpclang/aplet.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

extern const struct hpclang_font notebook_font;
volatile uint32_t hpclang_e2e_result;

static void hold(void)
{
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
}

static int action(struct notebook *note, struct hpclang_ui *ui, unsigned type)
{
    for (size_t i = 0; i < hpclang_ui_node_count(ui); ++i) {
        const struct hpclang_ui_node *node = hpclang_ui_node_at(ui, i);
        if (node->action.type != type)
            continue;
        struct hpclang_ui_event event;
        if (hpclang_ui_focus(ui, node->id) || hpclang_ui_activate(ui, &event) != 1)
            return -1;
        return notebook_update(note, &event);
    }
    return -1;
}

static int cleanup(void)
{
    if (hpclang_device_capability(HPCLANG_CAPABILITY_APLET_DATA) == HPCLANG_CAPABILITY_SUPPORTED)
        return hpclang_aplet_data_clear();
    struct hpclang_object_store *home = hpclang_object_store_home();
    if (!home)
        return -1;
    int result = hpclang_object_store_remove(home, (const uint8_t *)"HPCNOTE", 7);
    hpclang_object_store_close(home);
    return result;
}

int main(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_ui_config config = NOTEBOOK_UI_CONFIG;
    size_t size = hpclang_ui_storage_size(config);
    void *storage = malloc(size);
    struct hpclang_ui *ui = storage ? hpclang_ui_init(storage, size, config) : NULL;
    const struct hpclang_ui_surface_style style = {
        .font = &notebook_font, .foreground = 255, .accent = 255, .gap = 2};
    struct hpclang_ui_surface *renderer =
        hpclang_ui_surface_create(device->display_width, device->display_height, 2, &style, 1, 0);
    unsigned failure = 1, phase = 0;
    int display = 0;
    if (!ui || !renderer || hpclang_display_begin(2))
        goto done;
    display = 1;
    struct notebook note;
    notebook_init(&note);
    if (strstr(note.clock, "error")) {
        failure = 2;
        goto done;
    }
    char previous[NOTEBOOK_CAPACITY];
    if (notebook_load(previous)) {
        if (errno != ENOENT) {
            failure = 3;
            goto done;
        }
        strcpy(note.text, "First saved note");
    } else if (!strcmp(previous, "First saved note")) {
        phase = 1;
        strcpy(note.text, "Second saved note");
    } else if (!strcmp(previous, "Second saved note")) {
        phase = 2;
    } else {
        failure = 4;
        goto done;
    }
    if (notebook_view(ui, renderer, &note, (int)device->display_width,
                      (int)device->display_height)) {
        failure = 5;
        goto done;
    }
    if (phase < 2) {
        if (action(&note, ui, SAVE) || notebook_load(previous) || strcmp(previous, note.text)) {
            failure = 6;
            goto done;
        }
    } else if (cleanup()) {
        failure = 7;
        goto done;
    } else {
        strcpy(note.status, "Persistence verified");
    }
    if (notebook_view(ui, renderer, &note, (int)device->display_width,
                      (int)device->display_height) ||
        hpclang_ui_surface_render(renderer, ui) < 0 ||
        hpclang_ui_surface_copy(renderer, hpclang_display_surface()) ||
        hpclang_display_present(100000)) {
        failure = 8;
        goto done;
    }
    hpclang_e2e_result = UINT32_C(0x4f53b000) | phase;
    hold();
    failure = 0;
done:
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hold();
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    if (display)
        hpclang_display_end();
    free(storage);
    return 0;
}
