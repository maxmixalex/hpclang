#ifndef HPCLANG_WIN_CHOICES_H
#define HPCLANG_WIN_CHOICES_H
#include <stddef.h>
struct hpclang_win_choice
{
    char *option, *label;
};
struct hpclang_win_choices
{
    struct hpclang_win_choice *items;
    size_t count, position;
};
/* Descriptors own the array; strings remain borrowed. Initialize with {0}.
 * Keep historical numeric selection, including a past-end position that may
 * become selected after append. Removal shifts option and label together.
 */
void hpclang_win_choices_destroy(struct hpclang_win_choices *);
int hpclang_win_choices_add(struct hpclang_win_choices *, char *option, char *label);
int hpclang_win_choices_remove(struct hpclang_win_choices *, const char *option);
void hpclang_win_choices_set_index(struct hpclang_win_choices *, int index);
void hpclang_win_choices_set_choice(struct hpclang_win_choices *, const char *option);
int hpclang_win_choices_index(const struct hpclang_win_choices *);
char *hpclang_win_choices_choice(const struct hpclang_win_choices *);
void hpclang_win_choices_next(struct hpclang_win_choices *);
#endif
