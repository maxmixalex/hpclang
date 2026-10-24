#include "win-focus.h"
#include <errno.h>
#include <stddef.h>

static int fail(int error)
{
    errno = error;
    return -1;
}
static void repaint(struct hpclang_win_focus *focus)
{
    if (focus->repaint)
        focus->repaint(focus->data);
}
static void step(struct hpclang_win_focus *focus, int backwards)
{
    struct hpclang_win_focus_node *old = focus->current;
    if (!old)
        return;
    if (old->lost)
        old->lost(old->data);
    struct hpclang_win_focus_node *next = backwards ? old->prev : old->next;
    while (next != old) {
        if (next->focusable) {
            old->focused = 0;
            next->focused = 1;
            focus->current = next;
            repaint(focus);
            break;
        }
        next = backwards ? next->prev : next->next;
    }
    if (focus->current->gained)
        focus->current->gained(focus->current->data);
}
int hpclang_win_focus_step(struct hpclang_win_focus *focus, int backwards)
{
    if (!focus)
        return fail(EINVAL);
    if (focus->busy)
        return fail(EBUSY);
    focus->busy = 1;
    step(focus, backwards);
    focus->busy = 0;
    return 0;
}
int hpclang_win_focus_add(struct hpclang_win_focus *focus, struct hpclang_win_focus_node *node)
{
    if (!focus || !node)
        return fail(EINVAL);
    if (focus->busy)
        return fail(EBUSY);
    if (node->owner)
        return fail(EEXIST);
    focus->busy = 1;
    node->owner = focus;
    node->focused = 0;
    if (!focus->current) {
        node->next = node;
        node->prev = node;
        focus->current = node;
    } else {
        node->next = focus->current;
        node->prev = focus->current->prev;
        node->prev->next = node;
        node->next->prev = node;
    }
    if (!focus->current->focused && node->focusable) {
        focus->current = node;
        node->focused = 1;
        if (node->gained)
            node->gained(node->data);
    }
    if (node->install)
        node->install(node->data);
    repaint(focus);
    focus->busy = 0;
    return 0;
}
int hpclang_win_focus_remove(struct hpclang_win_focus *focus, struct hpclang_win_focus_node *node)
{
    if (!focus || !node)
        return fail(EINVAL);
    if (focus->busy)
        return fail(EBUSY);
    if (!node->owner)
        return 0;
    if (node->owner != focus)
        return fail(EINVAL);
    focus->busy = 1;
    if (node->next == node) {
        focus->current = NULL;
    } else {
        if (focus->current == node) {
            step(focus, 0);
            /* Historical traversal can stay on the removed node when all
             * survivors are nonfocusable. Keep a valid ring cursor instead. */
            if (focus->current == node)
                focus->current = node->next;
        }
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    node->owner = NULL;
    node->next = node->prev = NULL;
    node->focused = 0;
    if (node->deinstall)
        node->deinstall(node->data);
    repaint(focus);
    focus->busy = 0;
    return 0;
}
int hpclang_win_focus_has(const struct hpclang_win_focus *focus,
                          const struct hpclang_win_focus_node *node)
{
    return focus && node && focus->current == node;
}
