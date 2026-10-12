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
    free(storage);
    capacity();
    return 0;
}
