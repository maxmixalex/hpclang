#include <hpclang/ui.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static uint32_t token(const struct hpclang_ui *ui, uint32_t parent, uint32_t key)
{
    for (size_t i = 0; i < hpclang_ui_node_count(ui); ++i) {
        const struct hpclang_ui_node *node = hpclang_ui_node_at(ui, i);
        if (node->parent == parent && node->key == key)
            return node->id;
    }
    return 0;
}

static void component(struct hpclang_ui *ui, uint32_t key, const uint32_t *items, size_t count)
{
    HPCLANG_UI_WITH(ui, key, HPCLANG_UI_COMPONENT, 0)
    {
        assert(hpclang_ui_text(ui, 0, 0, "status") == 0);
        for (size_t i = 0; i < count; ++i) {
            HPCLANG_UI_WITH(ui, items[i], HPCLANG_UI_KEY, 0)
            {
                assert(hpclang_ui_text(ui, 1, 0, "item") == 0);
            }
        }
    }
}

static void identity(struct hpclang_ui *ui)
{
    uint32_t initial[] = {10, 20};
    assert(hpclang_ui_begin(ui) == 0);
    component(ui, 1, initial, 2);
    component(ui, 2, initial, 2);
    assert(hpclang_ui_commit(ui) == 0);
    uint32_t a = token(ui, 0, 1), b = token(ui, 0, 2);
    uint32_t item = token(ui, a, 20), leaf = token(ui, item, 1);
    assert(a && b && a != b && item != token(ui, b, 20));
    uint32_t reversed[] = {30, 20, 10};
    assert(hpclang_ui_begin(ui) == 0);
    component(ui, 2, reversed, 3);
    component(ui, 1, reversed, 3);
    assert(hpclang_ui_commit(ui) == 0);
    assert(token(ui, 0, 1) == a && token(ui, 0, 2) == b);
    assert(token(ui, a, 20) == item && token(ui, item, 1) == leaf);

    /* Removal retires the lifetime. Reappearing paths receive new tokens. */
    assert(hpclang_ui_begin(ui) == 0);
    component(ui, 2, initial, 2);
    assert(hpclang_ui_commit(ui) == 0);
    assert(hpclang_ui_begin(ui) == 0);
    component(ui, 1, initial, 2);
    component(ui, 2, initial, 2);
    assert(hpclang_ui_commit(ui) == 0);
    assert(token(ui, 0, 1) != a && token(ui, 0, 2) == b);
    a = token(ui, 0, 1);
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, "new kind") == 0);
    assert(hpclang_ui_commit(ui) == 0);
    assert(token(ui, 0, 1) != a);
}

static void interrupted(struct hpclang_ui *ui, int mode)
{
    HPCLANG_UI_WITH(ui, 1, HPCLANG_UI_COLUMN, 0)
    {
        HPCLANG_UI_WITH(ui, 2, HPCLANG_UI_ROW, 0)
        {
            assert(hpclang_ui_text(ui, 3, 0, "nested") == 0);
            if (mode == 0)
                break;
            if (mode == 1)
                continue;
            if (mode == 2)
                return;
            goto outside;
        }
    }
outside:
    return;
}

static void failures_and_scopes(struct hpclang_ui *ui)
{
    for (int mode = 0; mode < 4; ++mode) {
        assert(hpclang_ui_begin(ui) == 0);
        interrupted(ui, mode);
        assert(hpclang_ui_text(ui, 4, 0, "root") == 0);
        assert(hpclang_ui_commit(ui) == 0);
        assert(hpclang_ui_node_count(ui) == 4);
        assert(token(ui, 0, 4));
    }
    const struct hpclang_ui_node *saved = hpclang_ui_node_at(ui, 2);
    uint32_t saved_id = saved->id;
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, "one") == 0);
    assert(hpclang_ui_text(ui, 1, 0, "duplicate") == -1 && errno == EEXIST);
    assert(hpclang_ui_commit(ui) == -1 && errno == EEXIST);
    assert(saved->id == saved_id && !strcmp(saved->text, "nested"));
    assert(hpclang_ui_node_count(ui) == 4);

    assert(hpclang_ui_begin(ui) == 0);
    struct hpclang_ui_scope outer = hpclang_ui_scope_begin(ui, 1, HPCLANG_UI_COLUMN, 0);
    struct hpclang_ui_scope inner = hpclang_ui_scope_begin(ui, 2, HPCLANG_UI_ROW, 0);
    hpclang_ui_scope_close(&outer);
    hpclang_ui_scope_close(&inner);
    assert(hpclang_ui_commit(ui) == -1 && errno == EINVAL);
    assert(hpclang_ui_node_count(ui) == 4);

    assert(hpclang_ui_begin(ui) == 0);
    outer = hpclang_ui_scope_begin(ui, 1, HPCLANG_UI_COLUMN, 0);
    assert(hpclang_ui_commit(ui) == -1 && errno == EINVAL);
    assert(hpclang_ui_begin(ui) == 0);
    inner = hpclang_ui_scope_begin(ui, 2, HPCLANG_UI_ROW, 0);
    hpclang_ui_scope_close(&outer); /* Stale cleanup must not close the new frame. */
    hpclang_ui_scope_close(&inner);
    assert(hpclang_ui_commit(ui) == 0);
}

static int measure(void *context, const struct hpclang_ui_node *node,
                   struct hpclang_ui_metrics *metrics)
{
    (void)context;
    if (node->kind == HPCLANG_UI_ROW)
        *metrics = (struct hpclang_ui_metrics){.padding_x = 2, .padding_y = 1, .gap = 3, .grow = 1};
    else if (node->kind == HPCLANG_UI_COLUMN)
        *metrics = (struct hpclang_ui_metrics){.gap = 2};
    else if (node->key == 10)
        *metrics = (struct hpclang_ui_metrics){.width = 20, .height = 12, .grow = 1};
    else
        *metrics = (struct hpclang_ui_metrics){.width = 30, .height = 30, .grow = 2};
    return 0;
}

static void rectangle(struct hpclang_ui_rect rect, int x, int y, int width, int height)
{
    assert(rect.x == x && rect.y == y && rect.width == width && rect.height == height);
}

static int bad_measure(void *context, const struct hpclang_ui_node *node,
                       struct hpclang_ui_metrics *metrics)
{
    (void)node;
    if (context)
        hpclang_ui_abort(context);
    else
        metrics->height = -1;
    return 0;
}

static void layout(struct hpclang_ui *ui)
{
    assert(hpclang_ui_begin(ui) == 0);
    HPCLANG_UI_WITH(ui, 1, HPCLANG_UI_ROW, 0)
    {
        HPCLANG_UI_WITH(ui, 2, HPCLANG_UI_COMPONENT, 0)
        {
            assert(hpclang_ui_text(ui, 10, 0, "A") == 0);
        }
        assert(hpclang_ui_text(ui, 20, 0, "B") == 0);
    }
    assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){10, 20, 100, 40}, measure, NULL) == 0);
    assert(hpclang_ui_commit(ui) == 0);
    rectangle(hpclang_ui_node_at(ui, 0)->bounds, 10, 20, 100, 40);
    rectangle(hpclang_ui_node_at(ui, 2)->bounds, 12, 21, 34, 38);
    rectangle(hpclang_ui_node_at(ui, 3)->bounds, 49, 21, 59, 38);
    rectangle(hpclang_ui_node_at(ui, 3)->clip, 49, 21, 59, 38);

    assert(hpclang_ui_begin(ui) == 0);
    HPCLANG_UI_WITH(ui, 1, HPCLANG_UI_COLUMN, 0)
    {
        assert(hpclang_ui_text(ui, 20, 0, "first") == 0);
        assert(hpclang_ui_text(ui, 30, 0, "second") == 0);
    }
    assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, 80, 20}, measure, NULL) == 0);
    assert(hpclang_ui_commit(ui) == 0);
    rectangle(hpclang_ui_node_at(ui, 0)->bounds, 0, 0, 80, 62);
    rectangle(hpclang_ui_node_at(ui, 1)->clip, 0, 0, 80, 20);
    rectangle(hpclang_ui_node_at(ui, 2)->bounds, 0, 32, 80, 30);
    rectangle(hpclang_ui_node_at(ui, 2)->clip, 0, 32, 80, 0);

    for (int reenter = 0; reenter < 2; ++reenter) {
        assert(hpclang_ui_begin(ui) == 0);
        assert(hpclang_ui_text(ui, 1, 0, "invalid") == 0);
        assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, 80, 20}, bad_measure,
                                 reenter ? ui : NULL) == -1);
        assert(errno == (reenter ? EBUSY : EINVAL));
        assert(hpclang_ui_commit(ui) == -1);
        rectangle(hpclang_ui_node_at(ui, 0)->bounds, 0, 0, 80, 62);
    }
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, "overflow") == 0);
    assert(hpclang_ui_layout(ui, (struct hpclang_ui_rect){INT32_MAX, 0, 1, 20}, measure, NULL) ==
           -1);
    assert(hpclang_ui_commit(ui) == -1);
    assert(hpclang_ui_node_count(ui) == 3);
}

static void buttons(struct hpclang_ui *ui, const uint32_t *keys, size_t count, uint32_t disabled)
{
    assert(hpclang_ui_begin(ui) == 0);
    for (size_t i = 0; i < count; ++i)
        assert(hpclang_ui_button(ui, keys[i], 0, "action", (struct hpclang_ui_action){7, keys[i]},
                                 keys[i] != disabled) == 0);
    assert(hpclang_ui_commit(ui) == 0);
}

static void focus(struct hpclang_ui *ui)
{
    const uint32_t initial[] = {10, 20, 30};
    buttons(ui, initial, 3, 0);
    uint32_t a = token(ui, 0, 10), b = token(ui, 0, 20), c = token(ui, 0, 30);
    assert(hpclang_ui_focused(ui) == a);
    assert(hpclang_ui_focus_step(ui, 1) == 0 && hpclang_ui_focused(ui) == b);
    struct hpclang_ui_event event;
    assert(hpclang_ui_activate(ui, &event) == 1);
    assert(event.source == b && event.action.type == 7 && event.action.arg == 20);
    const uint32_t reversed[] = {30, 20, 10};
    buttons(ui, reversed, 3, 0);
    assert(hpclang_ui_focused(ui) == b);
    const uint32_t filtered[] = {30, 10};
    buttons(ui, filtered, 2, 0);
    assert(hpclang_ui_focused(ui) == a); /* Next survivor in the old reversed view. */
    buttons(ui, filtered, 2, 10);
    assert(hpclang_ui_focused(ui) == c);
    assert(hpclang_ui_focus(ui, a) == -1 && errno == ENOENT);
    assert(hpclang_ui_focus_step(ui, -1) == 0 && hpclang_ui_focused(ui) == c);
    assert(hpclang_ui_focus(ui, 0) == 0);
    buttons(ui, initial, 3, 0);
    assert(hpclang_ui_focused(ui) == 0);
    event.source = 123;
    assert(hpclang_ui_activate(ui, &event) == 0 && event.source == 123);
    assert(hpclang_ui_focus_step(ui, -1) == 0 && hpclang_ui_focused(ui) == c);
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_activate(ui, &event) == -1 && errno == EBUSY);
    assert(hpclang_ui_button(ui, 1, 0, "bad", (struct hpclang_ui_action){0}, 2) == -1);
    assert(hpclang_ui_commit(ui) == -1 && hpclang_ui_focused(ui) == c);
}

static void input_view(struct hpclang_ui *ui, const char *text)
{
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text_input(ui, 77, 0, text, 16, (struct hpclang_ui_action){9, 17}, 1) == 0);
    assert(hpclang_ui_commit(ui) == 0);
}

static void controlled_input(struct hpclang_ui *ui)
{
    input_view(ui, "A\xc3\xa9Z");
    assert(hpclang_ui_select(ui, 1, 3) == 0);
    struct hpclang_ui_event event = {0};
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, 'B', &event) == 1);
    assert(event.kind == HPCLANG_UI_CHANGE && !strcmp(event.text, "ABZ"));
    assert(event.action.type == 9 && event.action.arg == 17 && event.cursor == 2);
    assert(!strcmp(hpclang_ui_node_at(ui, 0)->text, "A\xc3\xa9Z"));
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, 'Q', &event) == -1 && errno == EBUSY);
    input_view(ui, event.text);
    assert(!strcmp(hpclang_ui_node_at(ui, 0)->text, "ABZ"));
    assert(hpclang_ui_node_at(ui, 0)->cursor == 2);
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, 'Q', &event) == 1);
    assert(!strcmp(event.text, "ABQZ"));
    input_view(ui, "ABZ"); /* Rejection preserves the previous cursor. */
    assert(hpclang_ui_node_at(ui, 0)->cursor == 2);
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, 'Q', &event) == 1);
    input_view(ui, "A\xe7\x95\x8c"); /* Transformation clamps to a scalar boundary. */
    assert(hpclang_ui_node_at(ui, 0)->cursor == 1);
    assert(hpclang_ui_select(ui, 2, 2) == -1 && errno == EINVAL);
    assert(hpclang_ui_select(ui, 0, 4) == 0);
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_BACKSPACE, 0, &event) == 1);
    assert(event.text[0] == 0 && event.cursor == 0);
    assert(hpclang_ui_begin(ui) == 0);
    hpclang_ui_abort(ui);
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_RIGHT, 0, &event) == -1 && errno == EBUSY);
    input_view(ui, "");
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, 0x1f600, &event) == 1);
    assert(strlen(event.text) == 4 && event.cursor == 4);
    input_view(ui, event.text);
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_LEFT, 0, &event) == 1);
    assert(event.kind == HPCLANG_UI_SELECTION && event.cursor == 0);
    assert(hpclang_ui_activate(ui, &event) == 1 && event.kind == HPCLANG_UI_ACCEPT);
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_CANCEL, 0, &event) == 1);
    assert(event.kind == HPCLANG_UI_CANCEL);
    const char *invalid[] = {"\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xc3", "a\nb"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(hpclang_ui_begin(ui) == 0);
        assert(hpclang_ui_text_input(ui, 77, 0, invalid[i], 16, (struct hpclang_ui_action){0}, 1) ==
               -1);
        assert(hpclang_ui_commit(ui) == -1);
        assert(strlen(hpclang_ui_node_at(ui, 0)->text) == 4);
    }
    input_view(ui, "123456789012345");
    assert(hpclang_ui_select(ui, 15, 15) == 0);
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, 'X', &event) == -1 && errno == ENOSPC);
    assert(hpclang_ui_node_at(ui, 0)->cursor == 15);
}

static int physical(struct hpclang_ui *ui, struct hpclang_text_mode *mode, uint16_t key,
                    struct hpclang_ui_event *event)
{
    struct hpclang_keyboard_event input = {.key = key, .type = HPCLANG_KEYBOARD_EVENT_DOWN};
    int result = hpclang_ui_key(ui, mode, &input, event);
    if (result == 1 && event->kind == HPCLANG_UI_CHANGE)
        input_view(ui, event->text);
    input.type = HPCLANG_KEYBOARD_EVENT_UP;
    assert(hpclang_ui_key(ui, mode, &input, event) == 0);
    return result;
}

static void physical_input(struct hpclang_ui *ui)
{
    struct hpclang_ui_event event;
    struct hpclang_text_mode mode;
    for (uint32_t model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        input_view(ui, "");
        assert(hpclang_text_mode_init(&mode, model) == 0);
        assert(physical(ui, &mode, HPCLANG_KEY_ALPHA, &event) == 0);
        uint16_t a = model <= HPCLANG_MODEL_HP40GS ? HPCLANG_KEY_HP39_VARS : HPCLANG_KEY_F1;
        struct hpclang_keyboard_event input = {.key = a, .type = HPCLANG_KEYBOARD_EVENT_DOWN};
        assert(hpclang_ui_key(ui, &mode, &input, &event) == 1);
        assert(!strcmp(event.text, "A"));
        struct hpclang_text_mode saved = mode;
        input.type = HPCLANG_KEYBOARD_EVENT_UP;
        assert(hpclang_ui_key(ui, &mode, &input, &event) == -1 && errno == EBUSY);
        assert(!memcmp(&saved, &mode, sizeof(mode)));
        input_view(ui, event.text);
        assert(hpclang_ui_key(ui, &mode, &input, &event) == 0);
        assert(physical(ui, &mode, HPCLANG_KEY_2, &event) == 1);
        assert(!strcmp(hpclang_ui_node_at(ui, 0)->text, "A2"));
        assert(physical(ui, &mode, HPCLANG_KEY_ENTER, &event) == 1);
        assert(event.kind == HPCLANG_UI_ACCEPT);
    }
    input_view(ui, "123456789012345");
    assert(hpclang_ui_select(ui, 0, 15) == 0);
    assert(hpclang_text_mode_init(&mode, HPCLANG_MODEL_HP50G) == 0);
    assert(physical(ui, &mode, HPCLANG_KEY_LEFT_SHIFT, &event) == 0);
    assert(physical(ui, &mode, HPCLANG_KEY_MULTIPLY, &event) == 1);
    assert(!strcmp(hpclang_ui_node_at(ui, 0)->text, "[]"));
    assert(hpclang_ui_node_at(ui, 0)->cursor == 1);
    assert(physical(ui, &mode, HPCLANG_KEY_2, &event) == 1);
    assert(!strcmp(hpclang_ui_node_at(ui, 0)->text, "[2]"));
    input_view(ui, "123456789012345");
    assert(hpclang_ui_select(ui, 15, 15) == 0);
    assert(physical(ui, &mode, HPCLANG_KEY_LEFT_SHIFT, &event) == 0);
    assert(physical(ui, &mode, HPCLANG_KEY_MULTIPLY, &event) == -1);
    assert(!strcmp(hpclang_ui_node_at(ui, 0)->text, "123456789012345"));
    struct hpclang_text_mode_status status;
    assert(hpclang_text_mode_get(&mode, &status) == 0 && status.plane == HPCLANG_TEXT_PLAIN);
}

static void capacity(void)
{
    struct hpclang_ui_config config = {2, 8};
    size_t bytes = hpclang_ui_storage_size(config);
    void *storage = malloc(bytes);
    assert(storage);
    struct hpclang_ui *ui = hpclang_ui_init(storage, bytes, config);
    assert(ui);
    char text[] = "owned";
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, text) == 0);
    text[0] = 'X';
    assert(hpclang_ui_commit(ui) == 0);
    const struct hpclang_ui_node *saved = hpclang_ui_node_at(ui, 0);
    assert(!strcmp(saved->text, "owned"));
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, "12345678") == -1 && errno == ENOSPC);
    assert(hpclang_ui_commit(ui) == -1 && !strcmp(saved->text, "owned"));
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_text(ui, 1, 0, "") == 0);
    assert(hpclang_ui_text(ui, 2, 0, "") == 0);
    assert(hpclang_ui_text(ui, 3, 0, "") == -1 && errno == ENOSPC);
    assert(hpclang_ui_commit(ui) == -1 && !strcmp(saved->text, "owned"));
    assert(hpclang_ui_begin(ui) == 0);
    assert(hpclang_ui_begin(ui) == -1 && errno == EBUSY);
    hpclang_ui_abort(ui);
    assert(hpclang_ui_node_count(ui) == 1);
    assert(!hpclang_ui_storage_size((struct hpclang_ui_config){0, 10}));
    assert(!hpclang_ui_storage_size((struct hpclang_ui_config){1, SIZE_MAX}));
    assert(!hpclang_ui_init(storage, bytes - 1, config));
    assert(!strcmp(saved->text, "owned"));
    free(storage);
}

int main(void)
{
    struct hpclang_ui_config config = {32, 256};
    size_t bytes = hpclang_ui_storage_size(config);
    void *storage = malloc(bytes);
    assert(storage);
    struct hpclang_ui *ui = hpclang_ui_init(storage, bytes, config);
    assert(ui);
    identity(ui);
    failures_and_scopes(ui);
    layout(ui);
    focus(ui);
    controlled_input(ui);
    physical_input(ui);
    free(storage);
    capacity();
    return 0;
}
