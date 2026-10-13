#ifndef HPCLANG_NOTEBOOK_APP_H
#define HPCLANG_NOTEBOOK_APP_H
#include "store.h"
#include <hpclang/ui-surface.h>
#define NOTEBOOK_UI_CONFIG {24, 768, 0}

enum action
{
    EDIT = 1,
    SAVE,
    LOAD,
    CLOCK,
    EXIT,
    BACKUP,
    RESTORE,
    EXPORT
};
struct notebook
{
    char text[NOTEBOOK_CAPACITY];
    char status[48];
    char clock[68];
};

void notebook_init(struct notebook *note);
int notebook_update(struct notebook *note, const struct hpclang_ui_event *event);
int notebook_view(struct hpclang_ui *ui, struct hpclang_ui_surface *renderer,
                  const struct notebook *note, int width, int height);
#endif
