#include <hpclang/ui.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#define UI_ROOT UINT32_MAX

struct node
{
    struct hpclang_ui_node view;
    uint32_t parent_index;
};

struct frame
{
    struct node *nodes;
    char *text;
    uint32_t count;
    size_t text_used;
};

struct hpclang_ui
{
    struct hpclang_ui_config capacity;
    struct frame frames[2];
    uint32_t parent, epoch, last_id, pending_id;
    unsigned current;
    int building, error;
};

static int fail(struct hpclang_ui *ui, int error)
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
    if ((SIZE_MAX - sizeof(struct hpclang_ui)) / config.nodes < 2 * sizeof(struct node)) {
        errno = EOVERFLOW;
        return 0;
    }
    size_t size = sizeof(struct hpclang_ui) + (size_t)config.nodes * 2 * sizeof(struct node);
    if (config.text_bytes > (SIZE_MAX - size) / 2) {
        errno = EOVERFLOW;
        return 0;
    }
    return size + 2 * config.text_bytes;
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
    ui->frames[0].text = (char *)(ui->frames[1].nodes + config.nodes);
    ui->frames[1].text = ui->frames[0].text + config.text_bytes;
    return ui;
}

int hpclang_ui_begin(struct hpclang_ui *ui)
{
    if (!ui)
        return fail(NULL, EINVAL);
    if (ui->building)
        return fail(NULL, EBUSY);
    if (ui->epoch == UINT32_MAX)
        return fail(NULL, EOVERFLOW);
    ++ui->epoch;
    struct frame *next = &ui->frames[ui->current ^ 1u];
    next->count = 0;
    next->text_used = 0;
    ui->pending_id = ui->last_id;
    ui->parent = UI_ROOT;
    ui->error = 0;
    ui->building = 1;
    return 0;
}

void hpclang_ui_abort(struct hpclang_ui *ui)
{
    if (ui)
        ui->building = 0;
}

int hpclang_ui_commit(struct hpclang_ui *ui)
{
    if (!ui || !ui->building)
        return fail(NULL, EINVAL);
    if (ui->parent != UI_ROOT && !ui->error)
        ui->error = EINVAL;
    ui->building = 0;
    if (ui->error)
        return fail(NULL, ui->error);
    ui->current ^= 1u;
    ui->last_id = ui->pending_id;
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

static int add(struct hpclang_ui *ui, uint32_t key, enum hpclang_ui_kind kind, uint16_t style,
               const char *text)
{
    if (!ui || !ui->building)
        return fail(NULL, EINVAL);
    if (ui->error)
        return fail(ui, ui->error);
    struct frame *next = &ui->frames[ui->current ^ 1u];
    const struct frame *old = &ui->frames[ui->current];
    if (next->count == ui->capacity.nodes)
        return fail(ui, ENOSPC);
    uint32_t parent = ui->parent == UI_ROOT ? 0 : next->nodes[ui->parent].view.id;
    for (uint32_t i = 0; i < next->count; ++i)
        if (next->nodes[i].view.parent == parent && next->nodes[i].view.key == key)
            return fail(ui, EEXIST);
    uint32_t id = 0;
    for (uint32_t i = 0; i < old->count; ++i) {
        const struct hpclang_ui_node *candidate = &old->nodes[i].view;
        if (candidate->parent == parent && candidate->key == key && candidate->kind == kind) {
            id = candidate->id;
            break;
        }
    }
    if (!id && ui->pending_id == UINT32_MAX)
        return fail(ui, EOVERFLOW);
    const char *owned = NULL;
    if (text) {
        size_t available = ui->capacity.text_bytes - next->text_used;
        size_t length = 0;
        while (length < available && text[length])
            ++length;
        if (length == available)
            return fail(ui, ENOSPC);
        char *destination = next->text + next->text_used;
        memcpy(destination, text, length + 1);
        next->text_used += length + 1;
        owned = destination;
    }
    if (!id)
        id = ++ui->pending_id;
    next->nodes[next->count++] =
        (struct node){.view = {id, parent, key, kind, style, owned}, .parent_index = ui->parent};
    return 0;
}

struct hpclang_ui_scope hpclang_ui_scope_begin(struct hpclang_ui *ui, uint32_t key,
                                               enum hpclang_ui_kind kind, uint16_t style)
{
    struct hpclang_ui_scope result = {0};
    if (kind < HPCLANG_UI_COMPONENT || kind > HPCLANG_UI_ROW) {
        fail(ui, EINVAL);
        return result;
    }
    if (add(ui, key, kind, style, NULL))
        return result;
    ui->parent = ui->frames[ui->current ^ 1u].count - 1;
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
        fail(ui, EINVAL);
        return;
    }
    ui->parent = ui->frames[ui->current ^ 1u].nodes[scope->node].parent_index;
}

int hpclang_ui_text(struct hpclang_ui *ui, uint32_t key, uint16_t style, const char *text)
{
    if (!text)
        return fail(ui, EINVAL);
    return add(ui, key, HPCLANG_UI_TEXT, style, text);
}
