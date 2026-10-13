#include <hpclang/ui-surface.h>
#include "display-surface.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct hpclang_ui_surface
{
    struct hpclang_surface *accepted, *work;
    struct hpclang_ui_surface_style *styles;
    size_t count;
    uint8_t background;
    int busy;
    struct hpclang_ui *bound;
};

static int fail(int error)
{
    errno = error;
    return -1;
}

struct hpclang_ui_surface *hpclang_ui_surface_create(uint32_t width, uint32_t height,
                                                     uint32_t levels,
                                                     const struct hpclang_ui_surface_style *styles,
                                                     size_t count, uint8_t background)
{
    if (!width || !height || width > INT32_MAX || height > INT32_MAX ||
        (levels != 2 && levels != 4 && levels != 16) || !styles || !count || count > 65536 ||
        count > SIZE_MAX / sizeof(*styles)) {
        fail(EINVAL);
        return NULL;
    }
    for (size_t i = 0; i < count; ++i)
        if (!hpclang_font_available(styles[i].font) || styles[i].minimum_width < 0 ||
            styles[i].minimum_height < 0) {
            fail(EINVAL);
            return NULL;
        }
    int saved = errno;
    struct hpclang_ui_surface *r = calloc(1, sizeof(*r));
    if (!r) {
        fail(ENOMEM);
        return NULL;
    }
    r->styles = malloc(count * sizeof(*styles));
    r->accepted = hpclang_surface_create(width, height, levels);
    r->work = hpclang_surface_create(width, height, levels);
    if (!r->styles || !r->accepted || !r->work) {
        hpclang_ui_surface_destroy(r);
        fail(ENOMEM);
        return NULL;
    }
    memcpy(r->styles, styles, count * sizeof(*styles));
    r->count = count;
    r->background = background;
    hpclang_surface_clear(r->accepted, background);
    errno = saved;
    return r;
}

void hpclang_ui_surface_destroy(struct hpclang_ui_surface *r)
{
    if (!r)
        return;
    if (r->busy) {
        fail(EBUSY);
        return;
    }
    hpclang_surface_destroy(r->accepted);
    hpclang_surface_destroy(r->work);
    free(r->styles);
    free(r);
}

int hpclang_ui_surface_measure(void *context, const struct hpclang_ui_node *node,
                               struct hpclang_ui_metrics *metrics)
{
    struct hpclang_ui_surface *r = context;
    if (!r || !node || !metrics || node->style >= r->count)
        return fail(EINVAL);
    if (r->busy)
        return fail(EBUSY);
    const struct hpclang_ui_surface_style *s = &r->styles[node->style];
    uint32_t width = 0, height = 0;
    if (node->text &&
        hpclang_font_measure_text_n(s->font, node->text, strlen(node->text), &width, &height))
        return -1;
    int container = node->kind == HPCLANG_UI_ROW || node->kind == HPCLANG_UI_COLUMN ||
                    node->kind == HPCLANG_UI_SCROLL;
    int64_t w = width, h = height;
    if (!container) {
        w += 2u * s->padding_x;
        h += 2u * s->padding_y;
    }
    if (w < s->minimum_width)
        w = s->minimum_width;
    if (h < s->minimum_height)
        h = s->minimum_height;
    if (w > INT32_MAX || h > INT32_MAX)
        return fail(EOVERFLOW);
    *metrics = (struct hpclang_ui_metrics){.width = (int32_t)w,
                                           .height = (int32_t)h,
                                           .padding_x = s->padding_x,
                                           .padding_y = s->padding_y,
                                           .gap = s->gap,
                                           .grow = s->grow};
    return 0;
}

static int begin(void *context, struct hpclang_ui_rect damage)
{
    struct hpclang_ui_surface *r = context;
    hpclang_surface_blit(r->work, 0, 0, r->accepted, 0, 0, (int32_t)r->work->width,
                         (int32_t)r->work->height);
    hpclang_surface_fill_rect(r->work, damage.x, damage.y, damage.width, damage.height,
                              r->background);
    return 0;
}

static int node(void *context, const struct hpclang_ui_node *n, struct hpclang_ui_rect clip,
                int focused)
{
    struct hpclang_ui_surface *r = context;
    if (n->style >= r->count)
        return fail(EINVAL);
    const struct hpclang_ui_surface_style *s = &r->styles[n->style];
    struct hpclang_surface lease;
    hpclang_surface_clip(&lease, r->work, clip.x, clip.y, clip.width, clip.height);
    hpclang_surface_clear(&lease, s->background);
    if (n->kind == HPCLANG_UI_BUTTON || n->kind == HPCLANG_UI_INPUT)
        hpclang_surface_draw_rect(&lease, n->bounds.x, n->bounds.y, n->bounds.width,
                                  n->bounds.height, focused ? s->accent : s->disabled);
    if (!n->text)
        return 0;
    int64_t x = (int64_t)n->bounds.x + s->padding_x;
    int64_t y = (int64_t)n->bounds.y + s->padding_y;
    if (x > INT32_MAX || y > INT32_MAX)
        return 0;
    uint32_t cursor = 0, anchor = 0, height = hpclang_font_height(s->font);
    if (n->kind == HPCLANG_UI_INPUT && focused) {
        uint32_t ignored;
        if (hpclang_font_measure_text_n(s->font, n->text, n->cursor, &cursor, &ignored) ||
            hpclang_font_measure_text_n(s->font, n->text, n->anchor, &anchor, &ignored))
            return -1;
        int64_t room = (int64_t)n->bounds.width - 2u * s->padding_x;
        if (room > 0 && cursor >= room)
            x -= cursor - room + 1;
    }
    /* Restrict glyphs/selection to the content box, preserving the border. */
    int32_t inset_x = n->bounds.width < s->padding_x ? n->bounds.width : s->padding_x;
    int32_t inset_y = n->bounds.height < s->padding_y ? n->bounds.height : s->padding_y;
    int64_t content_w = (int64_t)n->bounds.width - 2u * s->padding_x;
    int64_t content_h = (int64_t)n->bounds.height - 2u * s->padding_y;
    hpclang_surface_clip(&lease, &lease, n->bounds.x + inset_x, n->bounds.y + inset_y,
                         content_w > 0 ? (int32_t)content_w : 0,
                         content_h > 0 ? (int32_t)content_h : 0);
    if (lease.clip_left == lease.clip_right || lease.clip_top == lease.clip_bottom)
        return 0;
    if (x < INT32_MIN || height > INT32_MAX)
        return fail(EOVERFLOW);
    uint8_t ink = n->kind == HPCLANG_UI_TEXT || n->enabled ? s->foreground : s->disabled;
    hpclang_surface_draw_text(&lease, s->font, (int32_t)x, (int32_t)y, n->text, ink);
    if (n->kind == HPCLANG_UI_INPUT && focused) {
        /* Invert the selection after glyphs, retaining legibility at every level. */
        int64_t left = x + (cursor < anchor ? cursor : anchor);
        int64_t right = x + (cursor > anchor ? cursor : anchor);
        if (left < lease.clip_left)
            left = lease.clip_left;
        if (right > lease.clip_right)
            right = lease.clip_right;
        for (int64_t py = lease.clip_top; py < lease.clip_bottom && py < y + height; ++py)
            for (int64_t px = left; px < right; ++px)
                hpclang_surface_set_pixel(
                    &lease, (int32_t)px, (int32_t)py,
                    (uint8_t)(255 - hpclang_surface_get_pixel(&lease, (int32_t)px, (int32_t)py)));
        int64_t caret = x + cursor;
        if (caret >= INT32_MIN && caret <= INT32_MAX)
            hpclang_surface_fill_rect(&lease, (int32_t)caret, (int32_t)y, 1, (int32_t)height,
                                      s->accent);
    }
    return 0;
}

static int canvas(void *context, const struct hpclang_ui_node *n, struct hpclang_ui_rect clip)
{
    struct hpclang_ui_surface *r = context;
    struct hpclang_surface lease;
    hpclang_surface_clip(&lease, r->work, clip.x, clip.y, clip.width, clip.height);
    return n->paint(&lease, n->canvas_state, n->bounds, clip);
}

static int end(void *context, int completed)
{
    struct hpclang_ui_surface *r = context;
    if (completed) {
        struct hpclang_surface *previous = r->accepted;
        r->accepted = r->work;
        r->work = previous;
    }
    return 0;
}

int hpclang_ui_surface_render(struct hpclang_ui_surface *r, struct hpclang_ui *ui)
{
    if (!r || !ui)
        return fail(EINVAL);
    if (r->busy)
        return fail(EBUSY);
    struct hpclang_ui_rect area = {0, 0, (int32_t)r->work->width, (int32_t)r->work->height};
    if (r->bound != ui && hpclang_ui_invalidate(ui, area))
        return -1;
    struct hpclang_ui_painter painter = {
        .begin = begin, .node = node, .end = end, .context = r, .canvas = canvas};
    r->busy = 1;
    int result = hpclang_ui_paint(ui, area, &painter);
    r->busy = 0;
    if (result >= 0)
        r->bound = ui;
    return result;
}

int hpclang_ui_surface_copy(struct hpclang_ui_surface *r, struct hpclang_surface *target)
{
    if (!r || !target || target->width != r->accepted->width ||
        target->height != r->accepted->height)
        return fail(EINVAL);
    if (r->busy)
        return fail(EBUSY);
    hpclang_surface_blit(target, 0, 0, r->accepted, 0, 0, (int32_t)target->width,
                         (int32_t)target->height);
    return 0;
}
