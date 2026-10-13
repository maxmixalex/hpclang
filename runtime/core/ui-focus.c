#include "ui-internal.h"
#include <errno.h>

static int eligible(const struct hpclang_ui_node *node)
{
    return node->kind == HPCLANG_UI_BUTTON && node->enabled;
}

static uint32_t find(const struct frame *frame, uint32_t id)
{
    for (uint32_t i = 0; i < frame->count; ++i)
        if (frame->nodes[i].view.id == id && eligible(&frame->nodes[i].view))
            return i;
    return UI_ROOT;
}

void hpclang_ui_reconcile_focus(struct hpclang_ui *ui)
{
    const struct frame *old = &ui->frames[ui->current];
    const struct frame *next = &ui->frames[ui->current ^ 1u];
    if (ui->focus_cleared)
        return;
    if (find(next, ui->focused) != UI_ROOT)
        return;
    uint32_t previous = find(old, ui->focused);
    if (previous != UI_ROOT) {
        for (uint32_t i = previous + 1; i < old->count; ++i) {
            uint32_t id = old->nodes[i].view.id;
            if (find(next, id) != UI_ROOT) {
                ui->focused = id;
                return;
            }
        }
        for (uint32_t i = previous; i > 0; --i) {
            uint32_t id = old->nodes[i - 1].view.id;
            if (find(next, id) != UI_ROOT) {
                ui->focused = id;
                return;
            }
        }
    }
    ui->focused = 0;
    for (uint32_t i = 0; i < next->count; ++i)
        if (eligible(&next->nodes[i].view)) {
            ui->focused = next->nodes[i].view.id;
            return;
        }
}

uint32_t hpclang_ui_focused(const struct hpclang_ui *ui)
{
    return ui ? ui->focused : 0;
}

int hpclang_ui_focus(struct hpclang_ui *ui, uint32_t id)
{
    if (!ui)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building)
        return hpclang_ui_fail(NULL, EBUSY);
    if (id && find(&ui->frames[ui->current], id) == UI_ROOT)
        return hpclang_ui_fail(NULL, ENOENT);
    ui->focused = id;
    ui->focus_cleared = id == 0;
    return 0;
}

int hpclang_ui_focus_step(struct hpclang_ui *ui, int direction)
{
    if (!ui || (direction != -1 && direction != 1))
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building)
        return hpclang_ui_fail(NULL, EBUSY);
    const struct frame *frame = &ui->frames[ui->current];
    uint32_t index = find(frame, ui->focused);
    ui->focus_cleared = 0;
    for (uint32_t visited = 0; visited < frame->count; ++visited) {
        if (direction > 0)
            index = index == UI_ROOT || index + 1 == frame->count ? 0 : index + 1;
        else
            index = index == UI_ROOT || !index ? frame->count - 1 : index - 1;
        if (eligible(&frame->nodes[index].view)) {
            ui->focused = frame->nodes[index].view.id;
            return 0;
        }
    }
    ui->focused = 0;
    return 0;
}

int hpclang_ui_activate(struct hpclang_ui *ui, struct hpclang_ui_event *event)
{
    if (!ui || !event)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building)
        return hpclang_ui_fail(NULL, EBUSY);
    const struct frame *frame = &ui->frames[ui->current];
    uint32_t index = find(frame, ui->focused);
    if (index == UI_ROOT)
        return 0;
    const struct hpclang_ui_node *node = &frame->nodes[index].view;
    *event = (struct hpclang_ui_event){node->id, node->action};
    return 1;
}
