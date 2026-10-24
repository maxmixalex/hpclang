#include "win-loop.h"
#include <errno.h>

static int valid(const struct hpclang_win_loop *loop)
{
    if (!loop || !loop->events || loop->idle_event < 0) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}
int hpclang_win_loop_init(struct hpclang_win_loop *loop, struct hpclang_win_events *events,
                          int idle_event)
{
    if (!loop || !events || idle_event < 0) {
        errno = EINVAL;
        return -1;
    }
    *loop = (struct hpclang_win_loop){events, idle_event, 0};
    return 0;
}
int hpclang_win_loop_quit(struct hpclang_win_loop *loop)
{
    if (!valid(loop))
        return -1;
    loop->stopped = 1;
    return 0;
}
int hpclang_win_loop_run(struct hpclang_win_loop *loop, size_t budget)
{
    if (!valid(loop))
        return -1;
    loop->stopped = 0;
    for (size_t i = 0; i < budget; ++i) {
        int result;
        if (hpclang_win_events_post(loop->events, loop->idle_event, NULL, &result))
            return -1;
        if (loop->stopped)
            return 0;
    }
    return 1;
}
