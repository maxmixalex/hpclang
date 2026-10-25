#undef realloc
#include <stdlib.h>
#include "win-choices.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>

static int fail_allocation;
void *hpclang_test_realloc(void *pointer, size_t size)
{
    return fail_allocation ? NULL : realloc(pointer, size);
}
int main(void)
{
    char a[] = "A", b[] = "B", c[] = "C";
    char first[] = "first", second[] = "second", third[] = "third";
    struct hpclang_win_choices choices = {0};
    assert(hpclang_win_choices_index(&choices) == -1 && !hpclang_win_choices_choice(&choices));
    hpclang_win_choices_next(&choices);
    assert(choices.position == 0);
    assert(!hpclang_win_choices_add(&choices, a, first));
    assert(hpclang_win_choices_index(&choices) == 0 && hpclang_win_choices_choice(&choices) == a);
    hpclang_win_choices_set_choice(&choices, NULL);
    assert(hpclang_win_choices_index(&choices) == -1);
    assert(!hpclang_win_choices_add(&choices, b, second));
    assert(hpclang_win_choices_choice(&choices) == b); /* past-end becomes the new slot */
    hpclang_win_choices_set_choice(&choices, "missing");
    assert(!hpclang_win_choices_choice(&choices));
    struct hpclang_win_choice *old = choices.items;
    size_t old_position = choices.position;
    fail_allocation = 1;
    assert(hpclang_win_choices_add(&choices, c, third) == -1 && errno == ENOMEM);
    assert(choices.items == old && choices.count == 2 && choices.position == old_position);
    assert(choices.items[0].option == a && choices.items[0].label == first);
    fail_allocation = 0;
    assert(!hpclang_win_choices_add(&choices, c, third));
    assert(hpclang_win_choices_choice(&choices) == c);
    hpclang_win_choices_set_index(&choices, 0);
    assert(hpclang_win_choices_remove(&choices, a) == 1);
    assert(hpclang_win_choices_choice(&choices) == b && choices.items[0].label == second);
    assert(choices.items[1].option == c && choices.items[1].label == third);
    assert(hpclang_win_choices_remove(&choices, "absent") == 0 && choices.count == 2);
    hpclang_win_choices_set_index(&choices, 1);
    assert(hpclang_win_choices_remove(&choices, c) == 1);
    assert(hpclang_win_choices_choice(&choices) == b); /* removing last selection wraps */
    hpclang_win_choices_set_index(&choices, INT_MAX);
    assert(hpclang_win_choices_index(&choices) == -1);
    hpclang_win_choices_next(&choices);
    assert(hpclang_win_choices_choice(&choices) == b);
    hpclang_win_choices_set_index(&choices, -1);
    assert(hpclang_win_choices_index(&choices) == -1);
    assert(hpclang_win_choices_add(&choices, NULL, first) == -1 && errno == EINVAL);
    assert(hpclang_win_choices_remove(&choices, NULL) == -1 && errno == EINVAL);
    assert(hpclang_win_choices_remove(&choices, b) == 1);
    assert(!choices.count && !hpclang_win_choices_choice(&choices));
    hpclang_win_choices_destroy(&choices);
    assert(!choices.items && !choices.count && !choices.position);
    /* Overflow fails before an allocation or dereference of fabricated capacity. */
    choices.count = INT_MAX;
    assert(hpclang_win_choices_add(&choices, a, first) == -1 && errno == EOVERFLOW);
    choices.count = 0;
    assert(!hpclang_win_choices_add(&choices, a, first));
    assert(!hpclang_win_choices_add(&choices, a, second));
    hpclang_win_choices_set_choice(&choices, "A");
    assert(hpclang_win_choices_index(&choices) == 0);
    assert(hpclang_win_choices_remove(&choices, "A") == 1 && choices.items[0].label == second);
    hpclang_win_choices_destroy(&choices);
    return 0;
}
