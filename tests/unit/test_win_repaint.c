#include "win-repaint.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

static struct hpclang_win_events events;
static struct hpclang_win_handler handlers[4];
static struct hpclang_win_call snapshots[8];
static struct hpclang_win_loop loop;
static struct hpclang_win_repaint repaint;
static char trace[32];
static size_t used;
static int request_again, nested, quit, consume, failure_stage;
static void record(char c)
{
    assert(used + 1 < sizeof(trace));
    trace[used++] = c;
    trace[used] = 0;
}
static int begin(void *context)
{
    assert(context == &repaint);
    record('C');
    if (failure_stage == 1) {
        errno = EIO;
        return -1;
    }
    return 0;
}
static int present(void *context)
{
    assert(context == &repaint);
    record('F');
    if (failure_stage == 3) {
        errno = EIO;
        return -1;
    }
    return 0;
}
static int paint(void *event, void *context)
{
    assert(!event && context == &repaint);
    record('P');
    if (request_again) {
        request_again = 0;
        assert(!hpclang_win_repaint_request(&repaint));
        if (nested)
            assert(!hpclang_win_repaint_idle(NULL, &repaint));
    }
    if (quit)
        assert(!hpclang_win_loop_quit(&loop));
    if (failure_stage == 2)
        return hpclang_win_loop_fail(&loop, EIO);
    return consume;
}
static int second(void *event, void *context)
{
    assert(!event && context == &repaint);
    record('S');
    return 0;
}
static void init(void)
{
    used = 0;
    trace[0] = 0;
    request_again = nested = quit = consume = failure_stage = 0;
    assert(!hpclang_win_events_init(&events, handlers, 4, snapshots, 8));
    assert(!hpclang_win_loop_init(&loop, &events, 0));
    assert(!hpclang_win_repaint_init(&repaint, &loop, 1, begin, present, &repaint));
    assert(!hpclang_win_events_add(&events, 0, 2000000001, hpclang_win_repaint_idle, &repaint));
    assert(!hpclang_win_events_add(&events, 1, 1, paint, &repaint));
}
int main(void)
{
    init();
    assert(hpclang_win_loop_run(&loop, 2) == 1 && !used);
    assert(!hpclang_win_repaint_request(&repaint));
    assert(!hpclang_win_repaint_request(&repaint));
    request_again = 1;
    assert(hpclang_win_loop_run(&loop, 1) == 1 && !strcmp(trace, "CPF"));
    assert(repaint.pending);
    assert(hpclang_win_loop_run(&loop, 2) == 1 && !strcmp(trace, "CPFCPF"));
    assert(!repaint.pending);

    init();
    nested = request_again = 1;
    assert(!hpclang_win_repaint_request(&repaint));
    assert(hpclang_win_loop_run(&loop, 1) == 1 && !strcmp(trace, "CPCPFF"));
    assert(!events.depth && !events.used && !repaint.pending);

    init();
    assert(!hpclang_win_events_add(&events, 1, 0, second, &repaint));
    consume = -7;
    quit = 1;
    assert(!hpclang_win_repaint_request(&repaint));
    assert(!hpclang_win_loop_run(&loop, 8) && !strcmp(trace, "CPF"));

    for (int stage = 1; stage <= 3; ++stage) {
        init();
        failure_stage = stage;
        assert(!hpclang_win_repaint_request(&repaint));
        assert(hpclang_win_loop_run(&loop, 8) == -1 && errno == EIO);
        assert(!strcmp(trace, stage == 1 ? "C" : stage == 2 ? "CP" : "CPF"));
        size_t before = used;
        assert(hpclang_win_loop_run(&loop, 8) == -1 && errno == EIO && used == before);
    }
    init();
    events.snapshot_capacity = 1; /* Idle fits, nested repaint does not. */
    assert(!hpclang_win_repaint_request(&repaint));
    assert(hpclang_win_loop_run(&loop, 1) == -1 && errno == ENOSPC);
    assert(!strcmp(trace, "C") && !events.depth && !events.used);
    assert(hpclang_win_repaint_request(NULL) == -1 && errno == EINVAL);
    return 0;
}
