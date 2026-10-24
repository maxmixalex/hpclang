#include <winevent.h>
#include <wintimer.h>
#include <hpgraphics.h>
#include <hpclang/time.h>
#include <errno.h>
#include <stdint.h>

volatile uint32_t hpclang_e2e_result;
static unsigned ticks, paints, failure, deliveries;
static uint64_t start;
static int event_id;
static int receive(void *event, void *context)
{
    if (event != &ticks || context != &deliveries)
        failure = 1;
    ++deliveries;
    return 17;
}
static int paint(void *event, void *context)
{
    if (event || context || paints != ticks)
        failure = 2;
    ++paints;
    hpg_set_color(NULL, 255);
    hpg_fill_rect(4 + (int)ticks * 5, 12, 13 + (int)ticks * 5, 21);
    return 0;
}
static int advance(void *context)
{
    if (context != &ticks || win_post_event(event_id, &ticks) != 17)
        failure = 3;
    if (++ticks == 4) {
        win_event_quit();
        return 1;
    }
    win_repaint();
    return 0;
}
static int deadline(void *event, void *context)
{
    (void)event;
    (void)context;
    uint64_t now;
    if (hpclang_time_read_us(&now) || now - start > 2000000) {
        failure = 4;
        win_event_quit();
    }
    return 0;
}
int main(void)
{
    errno = 0;
    hpg_set_mode_mono(1);
    if (errno || hpclang_time_read_us(&start))
        failure = 5;
    event_id = win_register_event();
    win_add_event_handler(event_id, 0, receive, &deliveries);
    win_add_event_handler(7, 0, paint, NULL);
    win_add_event_handler(WIN_IDLE_EVENT, -100, deadline, NULL);
    win_add_timer(advance, &ticks, 30, 0);
    win_repaint();
    if (!failure)
        win_event_loop();
    uint64_t end;
    if (hpclang_win_error() || hpclang_time_read_us(&end) || ticks != 4 || paints != 4 ||
        deliveries != 4 || end - start < 120000 || end - start > 2000000)
        failure = 6;
    hpclang_e2e_result = failure ? UINT32_C(0x46417800) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
    hpg_cleanup();
    return 0;
}
