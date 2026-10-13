#include "ui-internal.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

int hpclang_ui_fail(struct hpclang_ui *ui, int error)
{
    if (ui && ui->building && !ui->error)
        ui->error = error;
    errno = error;
    return -1;
}

size_t hpclang_ui_storage_size(struct hpclang_ui_config config)
{
    if (!config.nodes || config.nodes == UI_ROOT) {
        errno = EINVAL;
        return 0;
    }
    size_t per_node = 2 * sizeof(struct node) + sizeof(struct ui_effect);
    if ((SIZE_MAX - sizeof(struct hpclang_ui)) / config.nodes < per_node) {
        errno = EOVERFLOW;
        return 0;
    }
    size_t size = sizeof(struct hpclang_ui) + (size_t)config.nodes * per_node;
    if (config.text_bytes > (SIZE_MAX - size) / 3) {
        errno = EOVERFLOW;
        return 0;
    }
    return size + 3 * config.text_bytes;
}

struct hpclang_ui *hpclang_ui_init(void *storage, size_t bytes, struct hpclang_ui_config config)
{
    size_t needed = hpclang_ui_storage_size(config);
    if (!needed)
        return NULL;
    if (!storage || (uintptr_t)storage % _Alignof(max_align_t) || bytes < needed) {
        errno = EINVAL;
        return NULL;
    }
    struct hpclang_ui *ui = storage;
    memset(ui, 0, needed);
    ui->capacity = config;
    ui->frames[0].nodes = (struct node *)(ui + 1);
    ui->frames[1].nodes = ui->frames[0].nodes + config.nodes;
    ui->effects = (struct ui_effect *)(ui->frames[1].nodes + config.nodes);
    ui->frames[0].text = (char *)(ui->effects + config.nodes);
    ui->frames[1].text = ui->frames[0].text + config.text_bytes;
    ui->edit_text = ui->frames[1].text + config.text_bytes;
    return ui;
}

int hpclang_ui_begin(struct hpclang_ui *ui)
{
    if (!ui || ui->closed)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building || ui->painting || ui->dispatching)
        return hpclang_ui_fail(NULL, EBUSY);
    if (ui->epoch == UINT32_MAX)
        return hpclang_ui_fail(NULL, EOVERFLOW);
    ++ui->epoch;
    struct frame *next = &ui->frames[ui->current ^ 1u];
    next->count = 0;
    next->text_used = 0;
    next->viewport = (struct hpclang_ui_rect){0};
    next->laid_out = 0;
    ui->pending_id = ui->last_id;
    ui->parent = UI_ROOT;
    ui->error = 0;
    ui->building = 1;
    ui->arranged = 0;
    return 0;
}

void hpclang_ui_abort(struct hpclang_ui *ui)
{
    if (ui && (ui->measuring || ui->painting || ui->dispatching)) {
        hpclang_ui_fail(ui, EBUSY);
        return;
    }
    if (ui)
        ui->building = 0;
}

int hpclang_ui_commit(struct hpclang_ui *ui)
{
    if (ui && (ui->measuring || ui->painting || ui->dispatching))
        return hpclang_ui_fail(ui, EBUSY);
    if (!ui || !ui->building)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->parent != UI_ROOT && !ui->error)
        ui->error = EINVAL;
    ui->building = 0;
    if (ui->error)
        return hpclang_ui_fail(NULL, ui->error);
    uint32_t old_focus = ui->focused;
    hpclang_ui_reconcile_focus(ui);
    if (hpclang_ui_scroll_reveal(ui, &ui->frames[ui->current ^ 1u], ui->focused, 0)) {
        ui->focused = old_focus;
        return -1;
    }
    hpclang_ui_damage_commit(ui, old_focus);
    ui->current ^= 1u;
    ui->last_id = ui->pending_id;
    ui->proposal_owner = 0;
    hpclang_ui_effect_reconcile(ui);
    return 0;
}

size_t hpclang_ui_node_count(const struct hpclang_ui *ui)
{
    return ui ? ui->frames[ui->current].count : 0;
}

const struct hpclang_ui_node *hpclang_ui_node_at(const struct hpclang_ui *ui, size_t index)
{
    if (!ui || index >= ui->frames[ui->current].count) {
        errno = EINVAL;
        return NULL;
    }
    return &ui->frames[ui->current].nodes[index].view;
}

int hpclang_ui_add(struct hpclang_ui *ui, uint32_t key, enum hpclang_ui_kind kind, uint16_t style,
                   const char *text)
{
    if (!ui || !ui->building)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->error)
        return hpclang_ui_fail(ui, ui->error);
    if (ui->measuring || ui->arranged)
        return hpclang_ui_fail(ui, EBUSY);
    struct frame *next = &ui->frames[ui->current ^ 1u];
    const struct frame *old = &ui->frames[ui->current];
    if (next->count == ui->capacity.nodes)
        return hpclang_ui_fail(ui, ENOSPC);
    uint32_t parent = ui->parent == UI_ROOT ? 0 : next->nodes[ui->parent].view.id;
    for (uint32_t i = 0; i < next->count; ++i)
        if (next->nodes[i].view.parent == parent && next->nodes[i].view.key == key)
            return hpclang_ui_fail(ui, EEXIST);
    uint32_t id = 0;
    int32_t scroll_y = 0;
    for (uint32_t i = 0; i < old->count; ++i) {
        const struct hpclang_ui_node *candidate = &old->nodes[i].view;
        if (candidate->parent == parent && candidate->key == key && candidate->kind == kind) {
            id = candidate->id;
            scroll_y = candidate->scroll_y;
            break;
        }
    }
    if (!id && ui->pending_id == UINT32_MAX)
        return hpclang_ui_fail(ui, EOVERFLOW);
    const char *owned = NULL;
    if (text) {
        size_t available = ui->capacity.text_bytes - next->text_used;
        size_t length = 0;
        while (length < available && text[length])
            ++length;
        if (length == available)
            return hpclang_ui_fail(ui, ENOSPC);
        char *destination = next->text + next->text_used;
        memcpy(destination, text, length + 1);
        next->text_used += length + 1;
        owned = destination;
    }
    if (!id)
        id = ++ui->pending_id;
    next->nodes[next->count++] = (struct node){.view = {.id = id,
                                                        .parent = parent,
                                                        .key = key,
                                                        .kind = kind,
                                                        .style = style,
                                                        .text = owned,
                                                        .scroll_y = scroll_y},
                                               .parent_index = ui->parent};
    return 0;
}

struct hpclang_ui_scope hpclang_ui_scope_begin(struct hpclang_ui *ui, uint32_t key,
                                               enum hpclang_ui_kind kind, uint16_t style)
{
    struct hpclang_ui_scope result = {0};
    if (kind < HPCLANG_UI_COMPONENT || kind > HPCLANG_UI_ROW) {
        hpclang_ui_fail(ui, EINVAL);
        return result;
    }
    if (hpclang_ui_add(ui, key, kind, style, NULL))
        return result;
    ui->parent = ui->frames[ui->current ^ 1u].count - 1;
    return (struct hpclang_ui_scope){ui, ui->epoch, ui->parent, 1};
}

struct hpclang_ui_scope hpclang_ui_scroll_begin(struct hpclang_ui *ui, uint32_t key, uint16_t style,
                                                int32_t height)
{
    if (height <= 0) {
        hpclang_ui_fail(ui, EINVAL);
        return (struct hpclang_ui_scope){0};
    }
    if (hpclang_ui_add(ui, key, HPCLANG_UI_SCROLL, style, NULL))
        return (struct hpclang_ui_scope){0};
    struct frame *frame = &ui->frames[ui->current ^ 1u];
    ui->parent = frame->count - 1;
    frame->nodes[ui->parent].view.preferred_height = height;
    return (struct hpclang_ui_scope){ui, ui->epoch, ui->parent, 1};
}

void hpclang_ui_scope_close(struct hpclang_ui_scope *scope)
{
    if (!scope || !scope->open)
        return;
    scope->open = 0;
    struct hpclang_ui *ui = scope->ui;
    if (!ui || !ui->building || ui->epoch != scope->epoch)
        return;
    if (ui->parent != scope->node) {
        hpclang_ui_fail(ui, EINVAL);
        return;
    }
    ui->parent = ui->frames[ui->current ^ 1u].nodes[scope->node].parent_index;
}

int hpclang_ui_text(struct hpclang_ui *ui, uint32_t key, uint16_t style, const char *text)
{
    if (!text)
        return hpclang_ui_fail(ui, EINVAL);
    return hpclang_ui_add(ui, key, HPCLANG_UI_TEXT, style, text);
}

int hpclang_ui_button(struct hpclang_ui *ui, uint32_t key, uint16_t style, const char *text,
                      struct hpclang_ui_action action, int enabled)
{
    if (!text || (enabled != 0 && enabled != 1))
        return hpclang_ui_fail(ui, EINVAL);
    if (hpclang_ui_add(ui, key, HPCLANG_UI_BUTTON, style, text))
        return -1;
    struct frame *next = &ui->frames[ui->current ^ 1u];
    next->nodes[next->count - 1].view.action = action;
    next->nodes[next->count - 1].view.enabled = enabled;
    return 0;
}

int hpclang_ui_toggle(struct hpclang_ui *ui, uint32_t key, uint16_t style, const char *text,
                      int checked, struct hpclang_ui_action action, int enabled)
{
    if (!text || (checked != 0 && checked != 1) || (enabled != 0 && enabled != 1))
        return hpclang_ui_fail(ui, EINVAL);
    if (hpclang_ui_add(ui, key, HPCLANG_UI_TOGGLE, style, text))
        return -1;
    struct hpclang_ui_node *node =
        &ui->frames[ui->current ^ 1u].nodes[ui->frames[ui->current ^ 1u].count - 1].view;
    node->action = action;
    node->enabled = enabled;
    node->value = (uint32_t)checked;
    return 0;
}

int hpclang_ui_canvas(struct hpclang_ui *ui, uint32_t key, uint16_t style,
                      hpclang_ui_canvas_paint_fn paint, const void *state, uint32_t revision,
                      int32_t preferred_height)
{
    if (!paint || preferred_height < 0)
        return hpclang_ui_fail(ui, EINVAL);
    if (hpclang_ui_add(ui, key, HPCLANG_UI_CANVAS, style, NULL))
        return -1;
    struct frame *next = &ui->frames[ui->current ^ 1u];
    struct hpclang_ui_node *node = &next->nodes[next->count - 1].view;
    node->paint = paint;
    node->canvas_state = state;
    node->revision = revision;
    node->preferred_height = preferred_height;
    return 0;
}
