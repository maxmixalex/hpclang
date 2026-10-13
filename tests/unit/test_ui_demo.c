#include "../../examples/declarative-ui/app.h"
#include <hpclang/font-package.h>
#include <hpclang/bootstrap.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return NULL;
}
static const uint8_t bits[] = {0xe0, 0xa0, 0xe0};
static const struct hpclang_bitmap_glyph glyph = {
    .codepoint = '?', .bitmap = bits, .width = 3, .height = 3, .advance = 4, .stride = 1};
static const struct hpclang_bitmap_glyph *const glyphs[] = {&glyph};
static const struct hpclang_bitmap_font_data font_data = {.glyphs = glyphs,
                                                          .glyph_count = 1,
                                                          .fallback_codepoint = '?',
                                                          .ascent = 3,
                                                          .line_height = 4,
                                                          .nominal_advance = 4};
static const struct hpclang_font font = HPCLANG_BITMAP_FONT(font_data);

static uint32_t control(struct demo_app *app, enum demo_action action, uint32_t arg)
{
    for (size_t i = 0; i < hpclang_ui_node_count(app->ui); ++i) {
        const struct hpclang_ui_node *node = hpclang_ui_node_at(app->ui, i);
        if (node->action.type == (uint32_t)action && node->action.arg == arg)
            return node->id;
    }
    return 0;
}
static void activate(struct demo_app *app, enum demo_action action, uint32_t arg)
{
    uint32_t id = control(app, action, arg);
    assert(id && hpclang_ui_focus(app->ui, id) == 0);
    struct hpclang_ui_event event;
    assert(hpclang_ui_activate(app->ui, &event) == 1);
    assert(demo_update(app, &event) == 1);
}
static void settle(struct demo_app *app)
{
    for (unsigned i = 0; i < 19; ++i)
        assert(demo_poll(app) == 0);
    assert(demo_poll(app) == 1);
}
int main(void)
{
    struct hpclang_ui_config config = {DEMO_UI_NODES, DEMO_UI_TEXT_BYTES};
    size_t bytes = hpclang_ui_storage_size(config);
    void *storage = malloc(bytes);
    struct hpclang_ui *ui = hpclang_ui_init(storage, bytes, config);
    struct hpclang_ui_surface_style styles[2] = {
        {.font = &font, .foreground = 255, .accent = 255, .gap = 2},
        {.font = &font, .foreground = 255, .accent = 255, .padding_x = 2, .padding_y = 1}};
    struct hpclang_ui_surface *r = hpclang_ui_surface_create(131, 64, 2, styles, 2, 0);
    assert(ui && r);
    struct demo_app app;
    demo_init(&app, ui);
    assert(demo_view(&app, r, 131, 64) == 0);
    assert(hpclang_ui_surface_render(r, ui) == 1);
    activate(&app, DEMO_CONNECT, 0);
    assert(demo_view(&app, r, 131, 64) == 0);
    activate(&app, DEMO_SCAN, 0);
    assert(demo_view(&app, r, 131, 64) == 0 && app.model.scan == DEMO_LOADING);
    settle(&app);
    assert(app.model.scan == DEMO_ERROR && app.cleanups == 1);
    assert(demo_view(&app, r, 131, 64) == 0);
    activate(&app, DEMO_SCAN, 0);
    assert(demo_view(&app, r, 131, 64) == 0);
    settle(&app);
    assert(app.model.scan == DEMO_READY && app.model.count == 3 && app.cleanups == 2);
    assert(demo_view(&app, r, 131, 64) == 0);
    uint32_t id = app.model.devices[1].id;
    uint32_t lifetime = control(&app, DEMO_TOGGLE, id);
    activate(&app, DEMO_TOGGLE, id);
    assert(app.model.devices[1].enabled == 0);
    assert(demo_view(&app, r, 131, 64) == 0);
    activate(&app, DEMO_REVERSE, 0);
    assert(demo_view(&app, r, 131, 64) == 0 && control(&app, DEMO_TOGGLE, id) == lifetime);
    for (unsigned i = 3; i < 8; ++i) {
        activate(&app, DEMO_ADD, 0);
        assert(demo_view(&app, r, 131, 64) == 0 && control(&app, DEMO_TOGGLE, id) == lifetime);
    }
    assert(app.model.count == 8);
    assert(hpclang_ui_surface_render(r, ui) == 1);
    uint32_t filter = control(&app, DEMO_FILTER, 0);
    assert(hpclang_ui_focus(ui, filter) == 0);
    struct hpclang_ui_event event;
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, '8', &event) == 1);
    assert(demo_update(&app, &event) == 1);
    assert(demo_view(&app, r, 131, 64) == 0 && !strcmp(app.model.filter, "8"));
    assert(!control(&app, DEMO_TOGGLE, id));
    uint32_t revision = app.model.revision;
    activate(&app, DEMO_WAVE, 0);
    assert(demo_view(&app, r, 131, 64) == 0 && app.model.revision == revision + 1);
    assert(hpclang_ui_surface_render(r, ui) == 1);
    activate(&app, DEMO_SCAN, 0);
    assert(demo_view(&app, r, 131, 64) == 0);
    assert(demo_poll(&app) == 0);
    activate(&app, DEMO_CANCEL, 0);
    assert(app.model.scan == DEMO_IDLE && app.cleanups == 3 && demo_poll(&app) == 0);
    assert(demo_view(&app, r, 131, 64) == 0);
    activate(&app, DEMO_EXIT, 0);
    assert(app.model.quit);
    assert(hpclang_ui_shutdown(ui) == 0);
    hpclang_ui_surface_destroy(r);
    free(storage);
    return 0;
}
