#ifndef HPCLANG_WIN_EVENT_REFERENCE_H
#define HPCLANG_WIN_EVENT_REFERENCE_H
#include <hpsys.h>

/* Host-only glue for caller-supplied historical dispatch sources. This is not
 * an installed header or a reproduction of their calculator ABI. All event
 * table rows are preallocated; hardware/window initialization is excluded. */
#define EQ_LEV1SIZE 64
#define EQ_LEV2SIZE 64
typedef int (*win_eventhandler)(void *, void *);
struct win_eventhandler
{
    win_eventhandler handler;
    void *app_data;
    int priority;
    struct win_eventhandler *next;
};
struct win_eventqueue
{
    struct win_eventhandler **handler_chains[EQ_LEV1SIZE];
};
extern struct win_eventqueue _win_sysqueue;
void win_init(void);
void win_add_event_handler(int, int, win_eventhandler, void *);
void win_remove_event_handler(int, win_eventhandler, void *);
int win_post_event(int, void *);
struct win_timer
{
    int (*func)(void *);
    void *app_data;
    int event_num;
    void *event_data;
    ULONGLONG next_time;
    unsigned int interval;
    unsigned periodic : 1;
    struct win_timer *next;
};
extern struct win_timer *_win_timer_list;
extern sys_timer_t _win_sys_timer;
void _win_push_timer(struct win_timer *);
int _win_timer_generator(void *, void *);
int _win_timer_event(void *);
void win_add_timer(int (*)(void *), void *, int, int);
void win_remove_timer(int (*)(void *), void *);
void win_add_timer_event(int, void *, int, int);
void win_remove_timer_event(int, void *);
#endif
