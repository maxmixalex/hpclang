#include <winbox.h>
#include <wincore.h>
#include "win-internal.h"
#include "win-box-axis.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

struct entry
{
    struct entry *next;
    win_widget_t *child;
    int row, col, width, height, align_x, align_y;
    double xweight, yweight;
};
struct box
{
    struct entry *first, *last;
    size_t count;
    int xpad, ypad;
};
static int fail(int error)
{
    hpclang_win_fail(error);
    return -1;
}
static void destroy(win_widget_t *widget)
{
    struct box *box = win_wcore_get_data(widget);
    while (box->first) {
        struct entry *entry = box->first;
        box->first = entry->next;
        free(entry);
    }
    free(box);
}
static struct box *data(win_widget_t *widget)
{
    return hpclang_win_widget_data(widget, destroy);
}
void win_box_add_with(win_widget_t *widget, win_widget_t *child, int row, int col, double xweight,
                      double yweight, int pwidth, int pheight, int align_x, int align_y)
{
    struct box *box = data(widget);
    if (!box || hpclang_win_error())
        return;
    if (hpclang_win_widget_callbacks) {
        hpclang_win_fail(EBUSY);
        return;
    }
    if (row < 0 || col < 0 || row == INT_MAX || col == INT_MAX || !isfinite(xweight) ||
        !isfinite(yweight) || xweight < 0 || yweight < 0 || pwidth < -1 || pheight < -1 ||
        align_x < -2 || align_x > 1 || align_y < -2 || align_y > 1) {
        hpclang_win_fail(EINVAL);
        return;
    }
    if (box->count == SIZE_MAX / sizeof(struct entry)) {
        hpclang_win_fail(EOVERFLOW);
        return;
    }
    struct entry *entry = malloc(sizeof(*entry));
    if (!entry) {
        hpclang_win_fail(ENOMEM);
        return;
    }
    if (child && hpclang_win_widget_bind(widget, child)) {
        free(entry);
        return;
    }
    if (child && hpclang_win_widget_installed(widget)) {
        win_add_widget(child);
        if (hpclang_win_error()) {
            hpclang_win_widget_unbind(widget, child);
            free(entry);
            return;
        }
    }
    *entry =
        (struct entry){NULL, child, row, col, pwidth, pheight, align_x, align_y, xweight, yweight};
    if (box->last)
        box->last->next = entry;
    else
        box->first = entry;
    box->last = entry;
    ++box->count;
}
void win_box_add_at(win_widget_t *widget, win_widget_t *child, int row, int col)
{
    win_box_add_with(widget, child, row, col, 1, 1, -1, -1, WIN_BOX_CENTER, WIN_BOX_CENTER);
}
void win_box_add(win_widget_t *widget, win_widget_t *child)
{
    win_box_add_at(widget, child, 0, 0);
}
void win_box_remove(win_widget_t *widget, win_widget_t *child)
{
    struct box *box = data(widget);
    if (!box)
        return;
    if (hpclang_win_widget_callbacks) {
        hpclang_win_fail(EBUSY);
        return;
    }
    struct entry *previous = NULL;
    for (struct entry *entry = box->first; entry; entry = entry->next) {
        if (entry->child != child) {
            previous = entry;
            continue;
        }
        if (child) {
            win_remove_widget(child);
            if (hpclang_win_widget_unbind(widget, child))
                return;
        }
        if (previous)
            previous->next = entry->next;
        else
            box->first = entry->next;
        if (box->last == entry)
            box->last = previous;
        --box->count;
        free(entry);
        return;
    }
}
struct measured
{
    struct entry *entry;
    int width, height;
    struct hpclang_win_box_extent x, y;
};
struct grid
{
    struct hpclang_win_box_cell *rows, *cols;
    struct measured *items;
    size_t nrows, ncols;
};
static void release(struct grid *grid)
{
    free(grid->rows);
    free(grid->cols);
    free(grid->items);
}
static int measure(struct box *box, struct grid *grid)
{
    for (struct entry *entry = box->first; entry; entry = entry->next) {
        if (grid->nrows <= (size_t)entry->row)
            grid->nrows = (size_t)entry->row + 1;
        if (grid->ncols <= (size_t)entry->col)
            grid->ncols = (size_t)entry->col + 1;
    }
    if (!box->count)
        return 0;
    /* A nonempty entry list must have at least one row and column. Keep the
     * allocation bound explicit before indexing the measured grid. */
    if (!grid->nrows || !grid->ncols)
        return fail(EINVAL);
    if (grid->nrows > SIZE_MAX / sizeof(*grid->rows) ||
        grid->ncols > SIZE_MAX / sizeof(*grid->cols) ||
        box->count > SIZE_MAX / sizeof(*grid->items))
        return fail(EOVERFLOW);
    grid->rows = calloc(grid->nrows, sizeof(*grid->rows));
    grid->cols = calloc(grid->ncols, sizeof(*grid->cols));
    grid->items = calloc(box->count, sizeof(*grid->items));
    if (!grid->rows || !grid->cols || !grid->items)
        return fail(ENOMEM);
    size_t i = 0;
    for (struct entry *entry = box->first; entry; entry = entry->next, ++i) {
        int w = 0, h = 0;
        if (entry->child && (entry->width == -1 || entry->height == -1))
            win_wcore_pref_size(entry->child, &w, &h);
        if (hpclang_win_error())
            return -1;
        if (entry->width != -1)
            w = entry->width;
        if (entry->height != -1)
            h = entry->height;
        if (w < 0 || h < 0)
            return fail(EINVAL);
        grid->items[i] = (struct measured){.entry = entry, .width = w, .height = h};
        struct hpclang_win_box_cell *row = &grid->rows[entry->row], *col = &grid->cols[entry->col];
        if (w > col->preferred)
            col->preferred = w;
        if (h > row->preferred)
            row->preferred = h;
        if (entry->xweight > col->weight)
            col->weight = entry->xweight;
        if (entry->yweight > row->weight)
            row->weight = entry->yweight;
    }
    return 0;
}
static int natural(struct hpclang_win_box_cell *cells, size_t count, int padding, int *size)
{
    int64_t total = count ? (int64_t)(count - 1) * padding : 0;
    for (size_t i = 0; i < count; ++i) {
        total += cells[i].preferred;
        if (total > INT_MAX)
            return fail(EOVERFLOW);
    }
    *size = (int)total;
    return 0;
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    struct box *box = data(widget);
    struct grid grid = {0};
    int w = 0, h = 0;
    if (!measure(box, &grid) && !natural(grid.cols, grid.ncols, box->xpad, &w) &&
        !natural(grid.rows, grid.nrows, box->ypad, &h)) {
        *width = w;
        *height = h;
    }
    release(&grid);
}
void win_box_layout(win_widget_t *widget)
{
    struct box *box = data(widget);
    if (!box || hpclang_win_error())
        return;
    if (hpclang_win_widget_callbacks) {
        hpclang_win_fail(EBUSY);
        return;
    }
    struct grid grid = {0};
    struct hpclang_win_box_extent *rows = NULL, *cols = NULL;
    if (measure(box, &grid) || !box->count)
        goto done;
    if (!grid.items || !grid.rows || !grid.cols) {
        hpclang_win_fail(EINVAL);
        goto done;
    }
    if (grid.nrows > SIZE_MAX / sizeof(*rows) || grid.ncols > SIZE_MAX / sizeof(*cols)) {
        hpclang_win_fail(EOVERFLOW);
        goto done;
    }
    rows = malloc(grid.nrows * sizeof(*rows));
    cols = malloc(grid.ncols * sizeof(*cols));
    if (!rows || !cols) {
        hpclang_win_fail(ENOMEM);
        goto done;
    }
    int x, y, w, h;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_size(widget, &w, &h);
    if (hpclang_win_box_axis(grid.rows, grid.nrows, box->ypad, h, y, rows) ||
        hpclang_win_box_axis(grid.cols, grid.ncols, box->xpad, w, x, cols)) {
        hpclang_win_fail(errno);
        goto done;
    }
    for (size_t i = 0; i < box->count; ++i) {
        struct measured *item = &grid.items[i];
        if (!item->entry) {
            hpclang_win_fail(EINVAL);
            goto done;
        }
        if (hpclang_win_box_align(cols[item->entry->col], item->width, item->entry->align_x,
                                  &item->x) ||
            hpclang_win_box_align(rows[item->entry->row], item->height, item->entry->align_y,
                                  &item->y)) {
            hpclang_win_fail(errno);
            goto done;
        }
    }
    for (size_t i = 0; i < box->count; ++i) {
        struct measured *item = &grid.items[i];
        if (item->entry->child)
            hpclang_win_widget_geometry(item->entry->child, item->x.position, item->y.position,
                                        item->x.size, item->y.size);
    }
done:
    free(rows);
    free(cols);
    release(&grid);
}
static void lifecycle(win_widget_t *widget)
{
    (void)widget;
}
win_widget_t *win_box_new_padding(int xpad, int ypad)
{
    if (xpad < 0 || ypad < 0) {
        hpclang_win_fail(EINVAL);
        return NULL;
    }
    struct box *box = calloc(1, sizeof(*box));
    if (!box) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    box->xpad = xpad;
    box->ypad = ypad;
    win_widget_t *widget = win_wcore_alloc(lifecycle, lifecycle, destroy, preferred, 0, box);
    if (!widget)
        free(box);
    return widget;
}
win_widget_t *win_box_new(void)
{
    return win_box_new_padding(0, 0);
}
