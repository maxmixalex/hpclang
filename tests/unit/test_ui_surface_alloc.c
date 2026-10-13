#include <hpclang/ui-surface.h>
#include <hpclang/font-package.h>
#include <hpclang/bootstrap.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);
static int remaining = -1;
static unsigned live;
static int reject(void)
{
    if (!remaining)
        return 1;
    if (remaining > 0)
        --remaining;
    return 0;
}
void *__wrap_malloc(size_t size)
{
    if (reject())
        return NULL;
    void *p = __real_malloc(size);
    if (p)
        ++live;
    return p;
}
void *__wrap_calloc(size_t count, size_t size)
{
    if (reject())
        return NULL;
    void *p = __real_calloc(count, size);
    if (p)
        ++live;
    return p;
}
void __wrap_free(void *p)
{
    if (p) {
        assert(live);
        --live;
    }
    __real_free(p);
}
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return NULL;
}
static const uint8_t ink[] = {0x80};
static const struct hpclang_bitmap_glyph glyph = {
    .codepoint = '?', .bitmap = ink, .width = 1, .height = 1, .advance = 2, .stride = 1};
static const struct hpclang_bitmap_glyph *const glyphs[] = {&glyph};
static const struct hpclang_bitmap_font_data data = {.glyphs = glyphs,
                                                     .glyph_count = 1,
                                                     .fallback_codepoint = '?',
                                                     .ascent = 1,
                                                     .line_height = 2,
                                                     .nominal_advance = 2};
static const struct hpclang_font font = HPCLANG_BITMAP_FONT(data);

int main(void)
{
    struct hpclang_ui_surface_style style = {.font = &font, .foreground = 255};
    struct hpclang_ui_surface *healthy = hpclang_ui_surface_create(8, 8, 2, &style, 1, 0);
    struct hpclang_surface *target = hpclang_surface_create(8, 8, 2);
    struct hpclang_ui_config config = {2, 16};
    size_t bytes = hpclang_ui_storage_size(config);
    void *storage = malloc(bytes);
    struct hpclang_ui *ui = hpclang_ui_init(storage, bytes, config);
    assert(healthy && target && ui);
    unsigned baseline = live;
    int recovered = 0;
    for (int point = 0; point < 32; ++point) {
        remaining = point;
        struct hpclang_ui_surface *attempt = hpclang_ui_surface_create(8, 8, 16, &style, 1, 0);
        remaining = -1;
        if (attempt) {
            hpclang_ui_surface_destroy(attempt);
            assert(live == baseline);
            recovered = 1;
            break;
        }
        assert(errno == ENOMEM && live == baseline);
        assert(hpclang_ui_surface_copy(healthy, target) == 0);
        assert(hpclang_surface_get_pixel(target, 0, 0) == 0);
    }
    assert(recovered);
    remaining = 0;
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, "A") == 0);
    assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, 8, 8}, hpclang_ui_surface_measure,
                             healthy) == 0);
    assert(hpclang_ui_commit(ui) == 0);
    assert(hpclang_ui_surface_render(healthy, ui) == 1);
    assert(hpclang_ui_surface_copy(healthy, target) == 0);
    assert(hpclang_surface_get_pixel(target, 0, 0) == 255);
    assert(hpclang_ui_surface_render(healthy, ui) == 0);
    assert(live == baseline);
    remaining = -1;
    assert(hpclang_ui_shutdown(ui) == 0);
    hpclang_ui_surface_destroy(healthy);
    hpclang_surface_destroy(target);
    free(storage);
    assert(live == 0);
    return 0;
}
