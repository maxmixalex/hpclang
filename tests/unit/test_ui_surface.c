#include <hpclang/ui-surface.h>
#include <hpclang/font-package.h>
#include <hpclang/bootstrap.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return NULL;
}
static const uint8_t bitmap[] = {0xe0, 0xa0, 0xe0};
static const struct hpclang_bitmap_glyph glyph = {
    .codepoint = '?', .bitmap = bitmap, .width = 3, .height = 3, .advance = 4, .stride = 1};
static const uint8_t x_bitmap[] = {0xa0, 0x40, 0xa0};
static const struct hpclang_bitmap_glyph space_glyph = {.codepoint = ' ', .advance = 4};
static const struct hpclang_bitmap_glyph x_glyph = {
    .codepoint = 'x', .bitmap = x_bitmap, .width = 3, .height = 3, .advance = 4, .stride = 1};
static const struct hpclang_bitmap_glyph *const glyphs[] = {&space_glyph, &glyph, &x_glyph};
static const struct hpclang_bitmap_font_data data = {.glyphs = glyphs,
                                                     .glyph_count = 3,
                                                     .fallback_codepoint = '?',
                                                     .ascent = 3,
                                                     .line_height = 4,
                                                     .nominal_advance = 4};
static const struct hpclang_font font = HPCLANG_BITMAP_FONT(data);
struct canvas_state
{
    unsigned calls;
    int error;
    uint8_t ink;
};
static int canvas(void *context, const void *state, struct hpclang_ui_rect bounds,
                  struct hpclang_ui_rect clip)
{
    (void)bounds;
    (void)clip;
    struct canvas_state *s = (struct canvas_state *)state;
    ++s->calls;
    /* Deliberately clear the full borrowed surface: backend must enforce clip. */
    hpclang_surface_clear(context, s->ink);
    if (s->error) {
        errno = EIO;
        return -1;
    }
    return 0;
}
static void view(struct hpclang_ui *ui, struct hpclang_ui_surface *r, struct canvas_state *s,
                 const char *text, uint32_t revision)
{
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, text) == 0);
    assert(hpclang_ui_canvas(ui, 2, 0, canvas, s, revision, 6) == 0);
    assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, 16, 16}, hpclang_ui_surface_measure,
                             r) == 0);
    assert(hpclang_ui_commit(ui) == 0);
}
static void run(uint32_t levels)
{
    struct hpclang_ui_surface_style style = {.font = &font,
                                             .foreground = 255,
                                             .background = 0,
                                             .disabled = 85,
                                             .accent = 255,
                                             .padding_x = 1,
                                             .padding_y = 1};
    struct hpclang_ui_surface *r = hpclang_ui_surface_create(16, 16, levels, &style, 1, 0);
    assert(r);
    struct hpclang_ui_config config = {8, 64, 0};
    size_t bytes = hpclang_ui_storage_size(config);
    void *storage = malloc(bytes);
    struct hpclang_ui *ui = hpclang_ui_init(storage, bytes, config);
    struct hpclang_surface *target = hpclang_surface_create(16, 16, levels);
    assert(ui && target);
    struct canvas_state s = {.ink = 255};
    view(ui, r, &s, "A", 1);
    assert(hpclang_ui_surface_render(r, ui) == 1 && s.calls == 1);
    assert(hpclang_ui_surface_copy(r, target) == 0);
    assert(hpclang_surface_get_pixel(target, 1, 1) == 255);
    assert(hpclang_surface_get_pixel(target, 0, 0) == 0);
    assert(hpclang_surface_get_pixel(target, 0, 6) == 255);
    assert(hpclang_surface_get_pixel(target, 0, 12) == 0);
    assert(hpclang_ui_surface_render(r, ui) == 0 && s.calls == 1);
    view(ui, r, &s, "B", 1);
    assert(hpclang_ui_surface_render(r, ui) == 1 && s.calls == 1);
    s.ink = 0;
    s.error = 1;
    view(ui, r, &s, "B", 2);
    assert(hpclang_ui_surface_render(r, ui) == -1 && errno == EIO);
    assert(hpclang_ui_surface_copy(r, target) == 0);
    assert(hpclang_surface_get_pixel(target, 0, 6) == 255);
    s.error = 0;
    assert(hpclang_ui_surface_render(r, ui) == 1);
    assert(hpclang_ui_surface_copy(r, target) == 0);
    assert(hpclang_surface_get_pixel(target, 0, 6) == 0);
    /* Simulate an unrelated/old LCD back page after page exchange. */
    hpclang_surface_clear(target, 255);
    assert(hpclang_ui_surface_copy(r, target) == 0);
    assert(hpclang_surface_get_pixel(target, 0, 0) == 0);
    assert(hpclang_surface_get_pixel(target, 1, 1) == 255);
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text_input(ui, 1, 0, "AB", 16, (struct hpclang_ui_action){1, 0}, 1) == 0);
    assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, 16, 16}, hpclang_ui_surface_measure,
                             r) == 0);
    assert(hpclang_ui_commit(ui) == 0);
    assert(hpclang_ui_select(ui, 0, 1) == 0);
    assert(hpclang_ui_surface_render(r, ui) == 1);
    assert(hpclang_ui_surface_copy(r, target) == 0);
    assert(hpclang_surface_get_pixel(target, 1, 1) == 0);   /* Selected black glyph inverted. */
    assert(hpclang_surface_get_pixel(target, 4, 1) == 255); /* Selected space inverted. */
    assert(hpclang_surface_get_pixel(target, 0, 0) == 255); /* Focus border. */
    /* Same application tree with an inverted, separately owned theme. */
    style.background = 255;
    style.foreground = 0;
    style.accent = 0;
    struct hpclang_ui_surface *other = hpclang_ui_surface_create(16, 16, levels, &style, 1, 255);
    assert(other);
    assert(hpclang_ui_surface_render(other, ui) == 1);
    assert(hpclang_ui_surface_copy(other, target) == 0);
    assert(hpclang_surface_get_pixel(target, 0, 0) == 0);
    assert(hpclang_surface_get_pixel(target, 1, 1) == 255);
    hpclang_ui_surface_destroy(other);
    assert(hpclang_ui_begin(ui) == 0);
    HPCLANG_UI_SCOPE(hpclang_ui_scroll_begin(ui, 20, 0, 12))
    {
        for (unsigned i = 0; i < 3; ++i)
            assert(hpclang_ui_button(ui, i, 0, "A", (struct hpclang_ui_action){1, i}, 1) == 0);
    }
    assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, 16, 16}, hpclang_ui_surface_measure,
                             r) == 0);
    assert(hpclang_ui_commit(ui) == 0);
    assert(hpclang_ui_focus(ui, hpclang_ui_node_at(ui, 3)->id) == 0);
    assert(hpclang_ui_node_at(ui, 0)->scroll_y == 8);
    assert(hpclang_ui_surface_render(r, ui) == 1);
    assert(hpclang_ui_surface_copy(r, target) == 0);
    assert(hpclang_surface_get_pixel(target, 1, 5) == 255); /* Revealed focus border. */
    assert(hpclang_surface_get_pixel(target, 1, 12) == 0);  /* Viewport confines children. */
    for (unsigned checked = 0; checked < 2; ++checked) {
        assert(hpclang_ui_begin(ui) == 0);
        assert(hpclang_ui_toggle(ui, 90, 0, "A", checked, (struct hpclang_ui_action){1, 0}, 1) ==
               0);
        assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, 16, 16},
                                 hpclang_ui_surface_measure, r) == 0);
        assert(hpclang_ui_commit(ui) == 0);
        assert(hpclang_ui_surface_render(r, ui) == 1);
        assert(hpclang_ui_surface_copy(r, target) == 0);
        assert(hpclang_surface_get_pixel(target, 6, 2) == (checked ? 255 : 0));
        if (!checked) {
            struct hpclang_ui_event event;
            assert(hpclang_ui_activate(ui, &event) == 1);
            assert(event.kind == HPCLANG_UI_CHANGE && event.value == 1);
        }
    }
    assert(hpclang_ui_shutdown(ui) == 0);
    hpclang_ui_surface_destroy(r);
    hpclang_surface_destroy(target);
    free(storage);
}
int main(void)
{
    uint32_t w = 99, h = 99;
    assert(hpclang_font_measure_text_n(&font, "A\xce\xa9\nB", 5, &w, &h) == 0);
    assert(w == 8 && h == 8);
    assert(hpclang_font_measure_text_n(&font, "\xce\xa9", 1, &w, &h) == 0 && w == 4);
    assert(hpclang_font_measure_text_n(NULL, "A", 1, &w, &h) == -1 && w == 4);
    run(2);
    run(4);
    run(16);
    return 0;
}
