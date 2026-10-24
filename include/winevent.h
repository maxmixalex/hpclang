#ifndef HPCLANG_WINEVENT_H
#define HPCLANG_WINEVENT_H
#include <stddef.h>
#define WIN_IDLE_EVENT 0
#ifdef __cplusplus
extern "C"
{
#endif
    typedef int (*win_eventhandler)(void *, void *);
    /* Foreground source adapter. Callbacks/data are borrowed through synchronous
     * snapshots, including after removal. Higher priority/newer ties run first.
     * Initialization is lazy. The current bounded service provides 64 handler
     * slots (three reserved for idle drivers), 256 simultaneous snapshot entries
     * and 32 timers. Exhaustion reports ENOSPC, never silently drops a registration.
     * System IDs 0..7 are reserved; user event registration starts at 8. Physical
     * key events 1/2 and repaint 7 are generated; typed-key/modifier events 3..6
     * and widget adapters are not implemented yet. No complete win.h is claimed.
     */
    void win_init(void);
    int win_register_event(void);
    int win_post_event(int event, void *event_data);
    void win_add_event_handler(int event, int priority, win_eventhandler, void *app_data);
    void win_remove_event_handler(int event, win_eventhandler, void *app_data);
    void win_event_loop(void);
    void win_event_quit(void);
    /* Request repaint event 7 on a subsequent idle pass; HPG owns its display. */
    void win_repaint(void);
    /* Checked extensions. Legacy void-call errors latch the first errno; a
     * callback's nonzero result alone does not latch an error. Inspect error after
     * win_post_event to distinguish a callback's -1 from failed dispatch.
     * Step: 0 quit, 1 budget exhausted, -1/errno. Reset rejects active callbacks
     * with EBUSY; otherwise drops registrations/timers and clears the error. It
     * neither releases a shared HPG display nor destroys borrowed callback data.
     */
    int hpclang_win_error(void);
    int hpclang_win_step(size_t idle_budget);
    int hpclang_win_reset(void);
#ifdef __cplusplus
}
#endif
#endif
