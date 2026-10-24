#ifndef HPCLANG_WIN_FOCUS_H
#define HPCLANG_WIN_FOCUS_H

/* Foreground widget membership. Nodes are caller-owned, initially zeroed, and
 * remain alive until removed. Callbacks may query membership/focus; membership
 * changes and traversal from lifecycle callbacks fail with EBUSY. The public
 * widget adapter must keep event snapshot lifetimes separate from membership.
 */
struct hpclang_win_focus;
struct hpclang_win_focus_node
{
    struct hpclang_win_focus_node *next, *prev;
    struct hpclang_win_focus *owner;
    void *data;
    void (*install)(void *), (*deinstall)(void *);
    void (*lost)(void *), (*gained)(void *);
    int focusable, focused;
};
struct hpclang_win_focus
{
    struct hpclang_win_focus_node *current;
    void (*repaint)(void *);
    void *data;
    unsigned busy;
};

int hpclang_win_focus_add(struct hpclang_win_focus *, struct hpclang_win_focus_node *);
int hpclang_win_focus_remove(struct hpclang_win_focus *, struct hpclang_win_focus_node *);
int hpclang_win_focus_step(struct hpclang_win_focus *, int backwards);
/* Historical has_focus reports the current ring node, even if nonfocusable. */
int hpclang_win_focus_has(const struct hpclang_win_focus *, const struct hpclang_win_focus_node *);

#endif
