#ifndef HPCLANG_WIN_REPAINT_H
#define HPCLANG_WIN_REPAINT_H
#include "win-loop.h"

typedef int (*hpclang_win_frame_fn)(void *);
struct hpclang_win_repaint
{
    struct hpclang_win_loop *loop;
    int event, pending;
    hpclang_win_frame_fn begin, present;
    void *context;
};
/* Fresh/inactive context; borrowed loop, hooks and context must outlive all
 * dispatches. Hooks return 0 or -1/errno. begin prepares/clears the drawing
 * target; present publishes it. The target's owner retains cleanup ownership.
 * No frame is requested implicitly. Request coalesces until idle delivery.
 */
int hpclang_win_repaint_init(struct hpclang_win_repaint *, struct hpclang_win_loop *, int event,
                             hpclang_win_frame_fn begin, hpclang_win_frame_fn present, void *);
int hpclang_win_repaint_request(struct hpclang_win_repaint *);
/* Register at idle priority 2000000001, between keyboard and timers. Clear the
 * request before begin and synchronous event dispatch, then present even if
 * a handler consumes repaint or requests ordinary quit. Requests from callbacks
 * survive for the next frame. Nested delivery preserves the same sequencing;
 * hooks must support it if the application explicitly reenters idle.
 * Failure latches the loop error, consumes idle and skips subsequent stages.
 * No automatic replay of partially executed drawing callbacks is attempted.
 */
int hpclang_win_repaint_idle(void *, void *);
#endif
