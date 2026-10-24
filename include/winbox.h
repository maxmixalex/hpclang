#ifndef HPCLANG_WINBOX_H
#define HPCLANG_WINBOX_H
#include <winwidget.h>
#define WIN_BOX_LEADING -1
#define WIN_BOX_CENTER 0
#define WIN_BOX_TRAILING 1
#define WIN_BOX_FILL -2
#ifdef __cplusplus
extern "C"
{
#endif
    /* Boxes borrow children, which must be removed from the box before destruction.
     * Destroying a detached box releases its bindings, not its children. Cycles,
     * duplicate parents and adding already installed children are rejected.
     * Installing a box installs its tree in insertion order, rolling back on error;
     * removal visits children in reverse order. Membership changes on an installed
     * box immediately install/remove the affected subtree. Layout is explicit and
     * transactional; it does not recursively lay out nested boxes. NULL children
     * are layout placeholders. Weights/padding must be finite and nonnegative;
     * preferred sizes are -1 (query child) or nonnegative. No implicit packing.
     */
    win_widget_t *win_box_new(void);
    win_widget_t *win_box_new_padding(int xpad, int ypad);
    void win_box_add(win_widget_t *, win_widget_t *child);
    void win_box_add_at(win_widget_t *, win_widget_t *child, int row, int col);
    void win_box_add_with(win_widget_t *, win_widget_t *child, int row, int col, double xweight,
                          double yweight, int pwidth, int pheight, int align_x, int align_y);
    void win_box_remove(win_widget_t *, win_widget_t *child);
    void win_box_layout(win_widget_t *);
#ifdef __cplusplus
}
#endif
#endif
