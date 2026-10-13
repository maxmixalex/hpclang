#include "ui-internal.h"
#include <errno.h>
#include <limits.h>

static int transparent(const struct node *node)
{
    return node->view.kind == HPCLANG_UI_COMPONENT || node->view.kind == HPCLANG_UI_KEY;
}

static int valid_rect(struct hpclang_ui_rect rect)
{
    return rect.width >= 0 && rect.height >= 0 && (int64_t)rect.x + rect.width <= INT32_MAX &&
           (int64_t)rect.y + rect.height <= INT32_MAX;
}

static int32_t minimum(int32_t a, int32_t b)
{
    return a < b ? a : b;
}
static int32_t maximum(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static struct hpclang_ui_rect intersect(struct hpclang_ui_rect a, struct hpclang_ui_rect b)
{
    int32_t x = maximum(a.x, b.x), y = maximum(a.y, b.y);
    int32_t right = minimum(a.x + a.width, b.x + b.width);
    int32_t bottom = minimum(a.y + a.height, b.y + b.height);
    return (struct hpclang_ui_rect){x, y, right > x ? right - x : 0, bottom > y ? bottom - y : 0};
}

static struct hpclang_ui_rect content(const struct node *node)
{
    struct hpclang_ui_rect rect = node->view.bounds;
    int32_t x = minimum(rect.width, node->metrics.padding_x);
    int32_t y = minimum(rect.height, node->metrics.padding_y);
    rect.x += x;
    rect.y += y;
    rect.width = maximum(0, rect.width - 2 * node->metrics.padding_x);
    rect.height = maximum(0, rect.height - 2 * node->metrics.padding_y);
    return rect;
}

static int accumulate(struct node *parent, const struct node *child)
{
    int row = parent->view.kind == HPCLANG_UI_ROW;
    int64_t gap = parent->children ? parent->metrics.gap : 0;
    int64_t width = row ? (int64_t)parent->content_width + gap + child->metrics.width
                        : maximum(parent->content_width, child->metrics.width);
    int64_t height = row ? maximum(parent->content_height, child->metrics.height)
                         : (int64_t)parent->content_height + gap + child->metrics.height;
    if (width > INT32_MAX || height > INT32_MAX ||
        child->metrics.grow > UINT32_MAX - parent->weights)
        return -1;
    parent->content_width = (int32_t)width;
    parent->content_height = (int32_t)height;
    parent->weights += child->metrics.grow;
    ++parent->children;
    return 0;
}

static void prepare_children(struct node *node)
{
    struct hpclang_ui_rect box = content(node);
    int row = node->view.kind == HPCLANG_UI_ROW;
    int32_t size = row ? box.width : box.height;
    int32_t used = row ? node->content_width : node->content_height;
    node->extra = size > used ? size - used : 0;
    node->offset = 0;
    node->children = 0;
}

int hpclang_ui_layout(struct hpclang_ui *ui, struct hpclang_ui_rect viewport,
                      hpclang_ui_measure_fn measure, void *context)
{
    if (!ui || !ui->building)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->error)
        return hpclang_ui_fail(ui, ui->error);
    if (ui->measuring)
        return hpclang_ui_fail(ui, EBUSY);
    if (!measure || !valid_rect(viewport) || ui->parent != UI_ROOT)
        return hpclang_ui_fail(ui, EINVAL);
    struct frame *frame = &ui->frames[ui->current ^ 1u];
    struct node root = {.view = {.kind = HPCLANG_UI_COLUMN, .bounds = viewport, .clip = viewport}};
    for (uint32_t i = 0; i < frame->count; ++i) {
        struct node *node = &frame->nodes[i];
        uint32_t parent = node->parent_index;
        if (parent != UI_ROOT && transparent(&frame->nodes[parent]))
            parent = frame->nodes[parent].layout_parent;
        node->layout_parent = parent;
        node->content_width = node->content_height = 0;
        node->children = node->weights = 0;
        node->metrics = (struct hpclang_ui_metrics){0};
        if (transparent(node))
            continue;
        ui->measuring = 1;
        int saved_errno = errno;
        errno = 0;
        int result = measure(context, &node->view, &node->metrics);
        int callback_error = errno;
        errno = saved_errno;
        ui->measuring = 0;
        if (ui->error)
            return hpclang_ui_fail(ui, ui->error);
        if (result)
            return hpclang_ui_fail(ui,
                                   result == -1 ? (callback_error ? callback_error : EIO) : EINVAL);
        if (node->metrics.width < 0 || node->metrics.height < 0)
            return hpclang_ui_fail(ui, EINVAL);
    }
    /* Reverse traversal completes nested container sizes before their parents.
     * Transparent identity scopes do not introduce layout boxes or extra gaps. */
    for (uint32_t i = frame->count; i > 0; --i) {
        struct node *node = &frame->nodes[i - 1];
        if (transparent(node))
            continue;
        if (node->view.kind == HPCLANG_UI_ROW || node->view.kind == HPCLANG_UI_COLUMN) {
            int64_t width = (int64_t)node->content_width + 2 * node->metrics.padding_x;
            int64_t height = (int64_t)node->content_height + 2 * node->metrics.padding_y;
            if (width > INT32_MAX || height > INT32_MAX)
                return hpclang_ui_fail(ui, EOVERFLOW);
            node->metrics.width = maximum(node->metrics.width, (int32_t)width);
            node->metrics.height = maximum(node->metrics.height, (int32_t)height);
        }
        struct node *parent =
            node->layout_parent == UI_ROOT ? &root : &frame->nodes[node->layout_parent];
        if (accumulate(parent, node))
            return hpclang_ui_fail(ui, EOVERFLOW);
    }
    prepare_children(&root);
    for (uint32_t i = 0; i < frame->count; ++i) {
        struct node *node = &frame->nodes[i];
        if (transparent(node)) {
            node->view.bounds = node->view.clip = (struct hpclang_ui_rect){0};
            continue;
        }
        struct node *parent =
            node->layout_parent == UI_ROOT ? &root : &frame->nodes[node->layout_parent];
        struct hpclang_ui_rect box = content(parent);
        int row = parent->view.kind == HPCLANG_UI_ROW;
        int32_t extra =
            parent->weights
                ? (int32_t)((uint64_t)parent->extra * node->metrics.grow / parent->weights)
                : 0;
        parent->extra -= extra;
        parent->weights -= node->metrics.grow;
        int64_t offset = (int64_t)parent->offset + (parent->children ? parent->metrics.gap : 0);
        int64_t size = (int64_t)(row ? node->metrics.width : node->metrics.height) + extra;
        int64_t origin = (row ? box.x : box.y) + offset;
        if (offset + size > INT32_MAX || origin > INT32_MAX || origin + size > INT32_MAX)
            return hpclang_ui_fail(ui, EOVERFLOW);
        node->view.bounds =
            row ? (struct hpclang_ui_rect){(int32_t)origin, box.y, (int32_t)size, box.height}
                : (struct hpclang_ui_rect){box.x, (int32_t)origin, box.width, (int32_t)size};
        node->view.clip = intersect(node->view.bounds, intersect(parent->view.clip, box));
        parent->offset = (int32_t)(offset + size);
        ++parent->children;
        prepare_children(node);
    }
    ui->arranged = 1;
    return 0;
}
