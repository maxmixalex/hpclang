#include "win-repaint.h"
#include <errno.h>

static int valid(const struct hpclang_win_repaint *repaint)
{
    if (!repaint || !repaint->loop || !repaint->loop->events || repaint->event < 0 ||
        !repaint->begin || !repaint->present) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}
int hpclang_win_repaint_init(struct hpclang_win_repaint *repaint, struct hpclang_win_loop *loop,
                             int event, hpclang_win_frame_fn begin, hpclang_win_frame_fn present,
                             void *context)
{
    if (!repaint || !loop || !loop->events || event < 0 || !begin || !present) {
        errno = EINVAL;
        return -1;
    }
    *repaint = (struct hpclang_win_repaint){loop, event, 0, begin, present, context};
    return 0;
}
int hpclang_win_repaint_request(struct hpclang_win_repaint *repaint)
{
    if (!valid(repaint))
        return -1;
    repaint->pending = 1;
    return 0;
}
int hpclang_win_repaint_idle(void *event, void *data)
{
    (void)event;
    struct hpclang_win_repaint *repaint = data;
    if (!valid(repaint))
        return -1;
    if (repaint->loop->failure)
        return hpclang_win_loop_fail(repaint->loop, repaint->loop->failure);
    if (!repaint->pending)
        return 0;
    repaint->pending = 0;
    if (repaint->begin(repaint->context))
        return hpclang_win_loop_fail(repaint->loop, errno);
    if (repaint->loop->failure)
        return hpclang_win_loop_fail(repaint->loop, repaint->loop->failure);
    int result;
    if (hpclang_win_events_post(repaint->loop->events, repaint->event, NULL, &result))
        return hpclang_win_loop_fail(repaint->loop, errno);
    if (repaint->loop->failure)
        return hpclang_win_loop_fail(repaint->loop, repaint->loop->failure);
    if (repaint->present(repaint->context))
        return hpclang_win_loop_fail(repaint->loop, errno);
    if (repaint->loop->failure)
        return hpclang_win_loop_fail(repaint->loop, repaint->loop->failure);
    return 0;
}
