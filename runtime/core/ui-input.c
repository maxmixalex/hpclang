#include "ui-internal.h"
#include "input-event-plan.h"
#include <errno.h>
#include <string.h>

/* Strict scalar decoding also rejects overlong NUL/control encodings. */
static int text_length(const char *text, size_t capacity, size_t *length)
{
    if (!text || !capacity)
        return 0;
    size_t i = 0;
    while (i < capacity && text[i]) {
        unsigned char first = (unsigned char)text[i++];
        uint32_t value;
        unsigned following;
        uint32_t minimum;
        if (first < 0x80) {
            value = first;
            following = 0;
            minimum = 0;
        } else if (first >= 0xc2 && first <= 0xdf) {
            value = first & 31;
            following = 1;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            value = first & 15;
            following = 2;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            value = first & 7;
            following = 3;
            minimum = 0x10000;
        } else {
            return 0;
        }
        while (following--) {
            if (i == capacity || ((unsigned char)text[i] & 0xc0) != 0x80)
                return 0;
            value = (value << 6) | ((unsigned char)text[i++] & 63);
        }
        if (value < minimum || value < 0x20 || (value >= 0x7f && value <= 0x9f) ||
            (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff)
            return 0;
    }
    if (i == capacity)
        return 0;
    *length = i;
    return 1;
}

static size_t clamp(const char *text, size_t length, size_t offset)
{
    if (offset > length)
        offset = length;
    while (offset && ((unsigned char)text[offset] & 0xc0) == 0x80)
        --offset;
    return offset;
}

int hpclang_ui_text_input(struct hpclang_ui *ui, uint32_t key, uint16_t style, const char *value,
                          size_t capacity, struct hpclang_ui_action action, int enabled)
{
    if (!ui || !ui->building)
        return hpclang_ui_fail(NULL, EINVAL);
    if (capacity > ui->capacity.text_bytes)
        return hpclang_ui_fail(ui, ENOSPC);
    size_t length;
    if ((enabled != 0 && enabled != 1) || !text_length(value, capacity, &length))
        return hpclang_ui_fail(ui, EINVAL);
    if (hpclang_ui_add(ui, key, HPCLANG_UI_INPUT, style, value))
        return -1;
    struct frame *next = &ui->frames[ui->current ^ 1u];
    struct hpclang_ui_node *node = &next->nodes[next->count - 1].view;
    node->action = action;
    node->enabled = enabled;
    node->capacity = capacity;
    node->cursor = node->anchor = length;
    const struct frame *old = &ui->frames[ui->current];
    for (uint32_t i = 0; i < old->count; ++i) {
        const struct hpclang_ui_node *previous = &old->nodes[i].view;
        if (previous->id != node->id)
            continue;
        size_t cursor = previous->cursor, anchor = previous->anchor;
        /* An unchanged value rejects the proposal. An accepted or transformed
         * value uses its proposed selection, clamped to valid new boundaries. */
        if (ui->edit_pending == node->id && strcmp(value, previous->text)) {
            cursor = ui->edit_cursor;
            anchor = ui->edit_anchor;
        }
        node->cursor = clamp(value, length, cursor);
        node->anchor = clamp(value, length, anchor);
        break;
    }
    return 0;
}

static struct hpclang_ui_node *focused_input(struct hpclang_ui *ui)
{
    struct frame *frame = &ui->frames[ui->current];
    for (uint32_t i = 0; i < frame->count; ++i) {
        struct hpclang_ui_node *node = &frame->nodes[i].view;
        if (node->id == ui->focused && node->kind == HPCLANG_UI_INPUT && node->enabled)
            return node;
    }
    return NULL;
}

int hpclang_ui_select(struct hpclang_ui *ui, size_t anchor, size_t cursor)
{
    if (!ui)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building || ui->painting || ui->edit_pending)
        return hpclang_ui_fail(NULL, EBUSY);
    struct hpclang_ui_node *node = focused_input(ui);
    if (!node)
        return hpclang_ui_fail(NULL, ENOENT);
    size_t length = strlen(node->text);
    if (cursor > length || anchor > length || clamp(node->text, length, cursor) != cursor ||
        clamp(node->text, length, anchor) != anchor)
        return hpclang_ui_fail(NULL, EINVAL);
    if (node->cursor != cursor || node->anchor != anchor)
        hpclang_ui_damage(ui, node->clip);
    node->cursor = cursor;
    node->anchor = anchor;
    return 0;
}

static int edit(struct hpclang_ui *ui, const struct hpclang_input_plan *plan,
                struct hpclang_ui_event *event)
{
    enum hpclang_input_action action = plan->action;
    if (!ui || !event || action < HPCLANG_INPUT_INSERT || action > HPCLANG_INPUT_CANCEL)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building || ui->painting || ui->edit_pending)
        return hpclang_ui_fail(NULL, EBUSY);
    struct hpclang_ui_node *node = focused_input(ui);
    if (!node)
        return 0;
    size_t length = strlen(node->text);
    memcpy(ui->edit_text, node->text, length + 1);
    struct hpclang_text_input input = {ui->edit_text, node->capacity, length, node->cursor};
    size_t begin = node->anchor < node->cursor ? node->anchor : node->cursor;
    size_t end = node->anchor > node->cursor ? node->anchor : node->cursor;
    int skip = 0;
    if (begin != end) {
        if (action == HPCLANG_INPUT_INSERT || action == HPCLANG_INPUT_BACKSPACE ||
            action == HPCLANG_INPUT_DELETE) {
            memmove(input.buffer + begin, input.buffer + end, length - end + 1);
            input.length -= end - begin;
            input.cursor = begin;
            skip = action != HPCLANG_INPUT_INSERT;
        } else if (action == HPCLANG_INPUT_LEFT || action == HPCLANG_INPUT_RIGHT) {
            input.cursor = action == HPCLANG_INPUT_LEFT ? begin : end;
            skip = 1;
        }
    }
    int result = skip ? HPCLANG_INPUT_EDITING : hpclang_input_plan_apply(&input, plan);
    if (result < 0)
        return -1;
    enum hpclang_ui_event_kind kind;
    if (result == HPCLANG_INPUT_ACCEPTED)
        kind = HPCLANG_UI_ACCEPT;
    else if (result == HPCLANG_INPUT_CANCELLED)
        kind = HPCLANG_UI_CANCEL;
    else if (strcmp(input.buffer, node->text)) {
        kind = HPCLANG_UI_CHANGE;
        ui->edit_pending = node->id;
        ui->edit_cursor = ui->edit_anchor = input.cursor;
    } else {
        if (input.cursor == node->cursor && node->anchor == input.cursor)
            return 0;
        kind = HPCLANG_UI_SELECTION;
        hpclang_ui_damage(ui, node->clip);
        node->cursor = node->anchor = input.cursor;
    }
    *event = (struct hpclang_ui_event){
        .source = node->id,
        .action = node->action,
        .kind = kind,
        .text = ui->edit_text,
        .cursor = input.cursor,
        .anchor =
            (kind == HPCLANG_UI_ACCEPT || kind == HPCLANG_UI_CANCEL) ? node->anchor : input.cursor};
    return 1;
}

int hpclang_ui_edit(struct hpclang_ui *ui, enum hpclang_input_action action, uint32_t codepoint,
                    struct hpclang_ui_event *event)
{
    const struct hpclang_input_plan plan = {action, codepoint, 0};
    return edit(ui, &plan, event);
}

int hpclang_ui_key(struct hpclang_ui *ui, struct hpclang_text_mode *mode,
                   const struct hpclang_keyboard_event *key, struct hpclang_ui_event *event)
{
    if (!ui || !event)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building || ui->painting || ui->edit_pending)
        return hpclang_ui_fail(NULL, EBUSY);
    struct hpclang_input_plan plan;
    int result = hpclang_input_plan_event(mode, key, &plan);
    if (result < 0)
        return -1;
    if (key->type == HPCLANG_KEYBOARD_EVENT_UP)
        return 0;
    if (key->key == HPCLANG_KEY_UP || key->key == HPCLANG_KEY_DOWN)
        return hpclang_ui_focus_step(ui, key->key == HPCLANG_KEY_UP ? -1 : 1);
    if (!result)
        return 0;
    if (focused_input(ui))
        return edit(ui, &plan, event);
    if (plan.action == HPCLANG_INPUT_ACCEPT)
        return hpclang_ui_activate(ui, event);
    if (plan.action == HPCLANG_INPUT_LEFT || plan.action == HPCLANG_INPUT_RIGHT)
        return hpclang_ui_focus_step(ui, plan.action == HPCLANG_INPUT_LEFT ? -1 : 1);
    return 0;
}
