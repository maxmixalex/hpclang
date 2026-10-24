#include <winevent.h>
#include <wintimer.h>
#include <hpgraphics.h>

static int frame;
static int paint(void *event, void *context)
{
    (void)event;
    (void)context;
    hpg_set_color(NULL, 255);
    hpg_fill_rect(4 + frame * 3, 12, 13 + frame * 3, 21);
    return 0;
}
static int advance(void *context)
{
    (void)context;
    if (++frame == 20) {
        win_event_quit();
        return 1;
    }
    win_repaint();
    return 0;
}
int main(void)
{
    /* Minimal event/timer source adapter client, not a widget demonstration. */
    win_add_event_handler(7, 0, paint, NULL);
    win_add_timer(advance, NULL, 50, 0);
    win_repaint();
    win_event_loop();
    int failed = hpclang_win_error() != 0;
    hpg_cleanup();
    return failed;
}
