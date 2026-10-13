#include "ui-internal.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

static int valid(struct hpclang_ui_rect rect)
{
    return rect.width >= 0 && rect.height >= 0 && (int64_t)rect.x + rect.width <= INT32_MAX &&
           (int64_t)rect.y + rect.height <= INT32_MAX;
}

static int equal(struct hpclang_ui_rect a, struct hpclang_ui_rect b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

static struct hpclang_ui_rect clipped(struct hpclang_ui_rect rect, int64_t left, int64_t top,
                                      int64_t right, int64_t bottom)
{
    if (left < rect.x)
        left = rect.x;
    if (top < rect.y)
        top = rect.y;
    if (right > (int64_t)rect.x + rect.width)
        right = (int64_t)rect.x + rect.width;
    if (bottom > (int64_t)rect.y + rect.height)
        bottom = (int64_t)rect.y + rect.height;
    if (right <= left || bottom <= top)
        return (struct hpclang_ui_rect){0};
    return (struct hpclang_ui_rect){(int32_t)left, (int32_t)top, (int32_t)(right - left),
                                    (int32_t)(bottom - top)};
}

void hpclang_ui_damage(struct hpclang_ui *ui, struct hpclang_ui_rect rect)
{
    if (!rect.width || !rect.height)
        return;
    int64_t right = (int64_t)rect.x + rect.width, bottom = (int64_t)rect.y + rect.height;
    if (!ui->dirty || rect.x < ui->damage_left)
        ui->damage_left = rect.x;
    if (!ui->dirty || rect.y < ui->damage_top)
        ui->damage_top = rect.y;
    if (!ui->dirty || right > ui->damage_right)
        ui->damage_right = right;
    if (!ui->dirty || bottom > ui->damage_bottom)
        ui->damage_bottom = bottom;
    ui->dirty = 1;
}

static const struct hpclang_ui_node *find(const struct frame *frame, uint32_t id)
{
    for (uint32_t i = 0; i < frame->count; ++i)
        if (frame->nodes[i].view.id == id)
            return &frame->nodes[i].view;
    return NULL;
}

void hpclang_ui_damage_focus(struct hpclang_ui *ui, uint32_t old_focus)
{
    if (old_focus == ui->focused)
        return;
    const struct frame *frame = &ui->frames[ui->current];
    const struct hpclang_ui_node *old = find(frame, old_focus), *next = find(frame, ui->focused);
    if (old)
        hpclang_ui_damage(ui, old->clip);
    if (next)
        hpclang_ui_damage(ui, next->clip);
}

static int changed(const struct hpclang_ui_node *a, const struct hpclang_ui_node *b)
{
    return !equal(a->bounds, b->bounds) || !equal(a->clip, b->clip) || a->style != b->style ||
           a->enabled != b->enabled || a->cursor != b->cursor || a->anchor != b->anchor ||
           a->paint != b->paint || a->canvas_state != b->canvas_state ||
           a->revision != b->revision || (!!a->text != !!b->text) ||
           (a->text && strcmp(a->text, b->text));
}

void hpclang_ui_damage_commit(struct hpclang_ui *ui, uint32_t old_focus)
{
    const struct frame *old = &ui->frames[ui->current], *next = &ui->frames[ui->current ^ 1u];
    if (!equal(old->viewport, next->viewport)) {
        hpclang_ui_damage(ui, old->viewport);
        hpclang_ui_damage(ui, next->viewport);
    }
    for (uint32_t i = 0; i < old->count; ++i) {
        const struct hpclang_ui_node *a = &old->nodes[i].view, *b = find(next, a->id);
        if (!b || changed(a, b) || ((a->id == old_focus) != (a->id == ui->focused))) {
            hpclang_ui_damage(ui, a->clip);
            if (b)
                hpclang_ui_damage(ui, b->clip);
        }
    }
    for (uint32_t i = 0; i < next->count; ++i)
        if (!find(old, next->nodes[i].view.id))
            hpclang_ui_damage(ui, next->nodes[i].view.clip);
}

int hpclang_ui_invalidate(struct hpclang_ui *ui, struct hpclang_ui_rect rect)
{
    if (!ui || ui->closed || !valid(rect))
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building || ui->painting || ui->dispatching)
        return hpclang_ui_fail(NULL, EBUSY);
    hpclang_ui_damage(ui, rect);
    return 0;
}

static int callback_error(int result, int error)
{
    return result == -1 ? (error ? error : EIO) : EINVAL;
}

int hpclang_ui_paint(struct hpclang_ui *ui, struct hpclang_ui_rect surface,
                     const struct hpclang_ui_painter *painter)
{
    if (!ui || ui->closed || !painter || !painter->begin || !painter->node || !painter->end ||
        !valid(surface))
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building || ui->painting || ui->dispatching)
        return hpclang_ui_fail(NULL, EBUSY);
    if (ui->frames[ui->current].count && !ui->frames[ui->current].laid_out)
        return hpclang_ui_fail(NULL, EINVAL);
    if (!ui->dirty)
        return 0;
    struct hpclang_ui_rect damage =
        clipped(surface, ui->damage_left, ui->damage_top, ui->damage_right, ui->damage_bottom);
    if (!damage.width || !damage.height) {
        ui->dirty = 0;
        return 0;
    }
    struct hpclang_ui_painter ops = *painter;
    ui->painting = 1;
    int saved_errno = errno;
    errno = 0;
    int result = ops.begin(ops.context, damage);
    int error = result ? callback_error(result, errno) : 0;
    if (!error) {
        const struct frame *frame = &ui->frames[ui->current];
        for (uint32_t i = 0; i < frame->count; ++i) {
            const struct hpclang_ui_node *node = &frame->nodes[i].view;
            struct hpclang_ui_rect clip =
                clipped(node->clip, damage.x, damage.y, (int64_t)damage.x + damage.width,
                        (int64_t)damage.y + damage.height);
            if (!clip.width || !clip.height || node->kind == HPCLANG_UI_COMPONENT ||
                node->kind == HPCLANG_UI_KEY)
                continue;
            errno = 0;
            result = node->kind == HPCLANG_UI_CANVAS
                         ? node->paint(ops.canvas_context, node->canvas_state, node->bounds, clip)
                         : ops.node(ops.context, node, clip, node->id == ui->focused);
            if (result) {
                error = callback_error(result, errno);
                break;
            }
        }
        errno = 0;
        result = ops.end(ops.context, !error);
        if (!error && result)
            error = callback_error(result, errno);
    }
    ui->painting = 0;
    if (error)
        return hpclang_ui_fail(NULL, error);
    ui->dirty = 0;
    errno = saved_errno;
    return 1;
}
