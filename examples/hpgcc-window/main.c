#include <winevent.h>
#include <wintimer.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include <hpgraphics.h>

static int frame, row = 12;
static int paint(void *event, void *context)
{
    (void)event;
    (void)context;
    hpg_set_color(NULL, 255);
    hpg_fill_rect(4 + frame * 3, row, 13 + frame * 3, row + 9);
    return 0;
}
static int key_typed(void *event, void *context)
{
    (void)context;
    const win_keytyped_t *key = (const win_keytyped_t *)event;
    if (key->keycode == KB_ON) {
        win_event_quit();
        return 1;
    }
    if (key->ascii >= '0' && key->ascii <= '9') {
        row = 12 + (key->ascii - '0') * 3;
        win_repaint();
        return 1;
    }
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
    /* Minimal event/timer/key source adapter client, not a widget demonstration. */
    win_add_event_handler(7, 0, paint, NULL);
    win_add_event_handler(WIN_KEY_TYPED, 0, key_typed, NULL);
    win_add_timer(advance, NULL, 50, 0);
    win_repaint();
    win_event_loop();
    int failed = hpclang_win_error() != 0;
    hpg_cleanup();
    return failed;
}
