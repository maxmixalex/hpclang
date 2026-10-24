#include "win-focus.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>

static struct hpclang_win_focus focus;
static struct hpclang_win_focus_node nodes[4];
static int trace[128], used;
static void record(int value)
{
    assert(used < 128);
    trace[used++] = value;
}
static void installed(void *data)
{
    record(*(int *)data + 10);
}
static void removed(void *data)
{
    record(*(int *)data + 20);
}
static void lost(void *data)
{
    record(*(int *)data + 30);
}
static void gained(void *data)
{
    record(*(int *)data + 40);
}
static void repaint(void *data)
{
    assert(data == &focus);
    record(50);
}
static void expect(const int *values, int count)
{
    assert(used == count);
    for (int i = 0; i < count; ++i)
        assert(trace[i] == values[i]);
    used = 0;
}
static void reentrant(void *data)
{
    gained(data);
    assert(hpclang_win_focus_has(&focus, &nodes[1]));
    assert(hpclang_win_focus_step(&focus, 0) == -1 && errno == EBUSY);
    assert(hpclang_win_focus_remove(&focus, &nodes[1]) == -1 && errno == EBUSY);
    assert(hpclang_win_focus_add(&focus, &nodes[3]) == -1 && errno == EBUSY);
}
static void permutations(void)
{
    /* All focusability masks and insertion/removal permutations. In
     * particular, repeatedly remove a cursor with only passive survivors. */
    for (unsigned mask = 0; mask < 16; ++mask) {
        for (unsigned order = 0; order < 24; ++order) {
            for (unsigned removal = 0; removal < 24; ++removal) {
                struct hpclang_win_focus registry = {0};
                struct hpclang_win_focus_node items[4] = {0};
                unsigned live = 0;
                unsigned code = order;
                for (unsigned n = 4; n; --n) {
                    unsigned choice = code % n;
                    code /= n;
                    unsigned i = 0;
                    for (; i < 4; ++i) {
                        if (live & (1u << i))
                            continue;
                        if (!choice--)
                            break;
                    }
                    items[i].focusable = !!(mask & (1u << i));
                    assert(!hpclang_win_focus_add(&registry, &items[i]));
                    live |= 1u << i;
                }
                code = removal;
                for (unsigned n = 4; n; --n) {
                    for (unsigned backwards = 0; backwards < 2; ++backwards) {
                        for (unsigned j = 0; j < 5; ++j) {
                            assert(!hpclang_win_focus_step(&registry, (int)backwards));
                            struct hpclang_win_focus_node *cursor = registry.current;
                            unsigned count = 0, focused = 0;
                            do {
                                assert(cursor->owner == &registry);
                                assert(cursor->next->prev == cursor);
                                assert(cursor->prev->next == cursor);
                                focused += !!cursor->focused;
                                assert(++count <= n);
                                cursor = cursor->next;
                            } while (cursor != registry.current);
                            assert(count == n && focused <= 1);
                            if (live & mask)
                                assert(registry.current->focused && focused == 1);
                        }
                    }
                    unsigned choice = code % n;
                    code /= n;
                    unsigned i = 0;
                    for (; i < 4; ++i) {
                        if (!(live & (1u << i)))
                            continue;
                        if (!choice--)
                            break;
                    }
                    assert(!hpclang_win_focus_remove(&registry, &items[i]));
                    live &= ~(1u << i);
                    assert(!items[i].owner && !items[i].next && !items[i].prev);
                    if (n == 1)
                        assert(!registry.current);
                }
            }
        }
    }
}
int main(void)
{
    int ids[] = {0, 1, 2, 3};
    focus.repaint = repaint;
    focus.data = &focus;
    for (int i = 0; i < 4; ++i) {
        nodes[i].data = &ids[i];
        nodes[i].install = installed;
        nodes[i].deinstall = removed;
        nodes[i].lost = lost;
        nodes[i].gained = gained;
    }
    nodes[1].focusable = nodes[2].focusable = 1;
    assert(!hpclang_win_focus_step(&focus, 0));
    assert(!hpclang_win_focus_add(&focus, &nodes[0]));
    expect((int[]){10, 50}, 2);
    assert(hpclang_win_focus_has(&focus, &nodes[0]) && !nodes[0].focused);
    nodes[1].gained = reentrant;
    assert(!hpclang_win_focus_add(&focus, &nodes[1]));
    expect((int[]){41, 11, 50}, 3);
    nodes[1].gained = gained;
    assert(!hpclang_win_focus_add(&focus, &nodes[2]));
    expect((int[]){12, 50}, 2);
    assert(hpclang_win_focus_add(&focus, &nodes[1]) == -1 && errno == EEXIST);
    assert(!hpclang_win_focus_step(&focus, 0));
    expect((int[]){31, 50, 42}, 3);
    assert(!nodes[1].focused && nodes[2].focused);
    assert(!hpclang_win_focus_step(&focus, 1));
    expect((int[]){32, 50, 41}, 3);
    assert(!hpclang_win_focus_remove(&focus, &nodes[2]));
    expect((int[]){22, 50}, 2);
    assert(!hpclang_win_focus_step(&focus, 0));
    expect((int[]){31, 41}, 2);
    /* Removing the last focusable widget must not leave a dangling cursor. */
    assert(!hpclang_win_focus_remove(&focus, &nodes[1]));
    expect((int[]){31, 41, 21, 50}, 4);
    assert(focus.current == &nodes[0] && !nodes[1].owner && !nodes[1].focused);
    assert(!hpclang_win_focus_step(&focus, 0));
    expect((int[]){30, 40}, 2);
    assert(!hpclang_win_focus_remove(&focus, &nodes[0]));
    expect((int[]){20, 50}, 2);
    assert(!focus.current);
    assert(!hpclang_win_focus_remove(&focus, &nodes[0]));
    assert(!hpclang_win_focus_add(&focus, &nodes[1]));
    expect((int[]){41, 11, 50}, 3);
    struct hpclang_win_focus other = {0};
    assert(hpclang_win_focus_remove(&other, &nodes[1]) == -1 && errno == EINVAL);
    assert(!hpclang_win_focus_remove(&focus, &nodes[1]));
    expect((int[]){21, 50}, 2);
    assert(hpclang_win_focus_add(NULL, &nodes[0]) == -1 && errno == EINVAL);
    assert(hpclang_win_focus_remove(&focus, NULL) == -1 && errno == EINVAL);
    assert(hpclang_win_focus_step(NULL, 0) == -1 && errno == EINVAL);
    assert(!hpclang_win_focus_has(&focus, NULL));
    permutations();
    return 0;
}
