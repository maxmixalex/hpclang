#ifndef HPCLANG_UI_DEMO_APP_H
#define HPCLANG_UI_DEMO_APP_H
#include <hpclang/ui-surface.h>

/* The full eight-device view has 46 nodes. Keep bounded headroom and owned
 * text for that view, without reserving unused capacity on small Stock heaps. */
#define DEMO_UI_NODES 48u
#define DEMO_UI_TEXT_BYTES 512u
#define DEMO_UI_EFFECTS 1u

enum demo_action
{
    DEMO_CONNECT = 1,
    DEMO_DISCONNECT,
    DEMO_FILTER,
    DEMO_SCAN,
    DEMO_CANCEL,
    DEMO_ADD,
    DEMO_REVERSE,
    DEMO_TOGGLE,
    DEMO_WAVE,
    DEMO_EXIT
};
enum demo_scan
{
    DEMO_IDLE,
    DEMO_LOADING,
    DEMO_READY,
    DEMO_ERROR
};
struct demo_device
{
    uint32_t id;
    char name[24];
    int enabled;
};
struct demo_model
{
    int connected, quit;
    char filter[24];
    enum demo_scan scan;
    struct demo_device devices[8];
    unsigned count;
    uint32_t next_id, revision;
    uint16_t samples[32];
};
struct demo_app
{
    struct hpclang_ui *ui;
    struct demo_model model;
    uint32_t request;
    unsigned attempts, polls, cleanups;
};
void demo_init(struct demo_app *app, struct hpclang_ui *ui);
int demo_view(struct demo_app *app, struct hpclang_ui_surface *renderer, int width, int height);
int demo_update(struct demo_app *app, const struct hpclang_ui_event *event);
int demo_poll(struct demo_app *app);
#endif
