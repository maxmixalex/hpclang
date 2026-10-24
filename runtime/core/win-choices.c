#include "win-choices.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void hpclang_win_choices_destroy(struct hpclang_win_choices *choices)
{
    free(choices->items);
    *choices = (struct hpclang_win_choices){0};
}
int hpclang_win_choices_add(struct hpclang_win_choices *choices, char *option, char *label)
{
    if (!option || !label) {
        errno = EINVAL;
        return -1;
    }
    if (choices->count >= INT_MAX || choices->count >= SIZE_MAX / sizeof(*choices->items)) {
        errno = EOVERFLOW;
        return -1;
    }
    struct hpclang_win_choice *replacement =
        realloc(choices->items, (choices->count + 1) * sizeof(*replacement));
    if (!replacement) {
        errno = ENOMEM;
        return -1;
    }
    replacement[choices->count] = (struct hpclang_win_choice){option, label};
    choices->items = replacement;
    ++choices->count;
    return 0;
}
int hpclang_win_choices_remove(struct hpclang_win_choices *choices, const char *option)
{
    if (!option) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < choices->count; ++i)
        if (!strcmp(option, choices->items[i].option)) {
            memmove(choices->items + i, choices->items + i + 1,
                    (choices->count - i - 1) * sizeof(*choices->items));
            --choices->count;
            if (choices->position == choices->count)
                choices->position = 0;
            return 1;
        }
    return 0;
}
void hpclang_win_choices_set_index(struct hpclang_win_choices *choices, int index)
{
    choices->position = index < 0 ? choices->count : (size_t)index;
}
void hpclang_win_choices_set_choice(struct hpclang_win_choices *choices, const char *option)
{
    choices->position = choices->count;
    if (option)
        for (size_t i = 0; i < choices->count; ++i)
            if (!strcmp(option, choices->items[i].option)) {
                choices->position = i;
                break;
            }
}
int hpclang_win_choices_index(const struct hpclang_win_choices *choices)
{
    return choices->position < choices->count ? (int)choices->position : -1;
}
char *hpclang_win_choices_choice(const struct hpclang_win_choices *choices)
{
    return choices->position < choices->count ? choices->items[choices->position].option : NULL;
}
void hpclang_win_choices_next(struct hpclang_win_choices *choices)
{
    if (choices->position >= choices->count || choices->position + 1 >= choices->count)
        choices->position = 0;
    else
        ++choices->position;
}
