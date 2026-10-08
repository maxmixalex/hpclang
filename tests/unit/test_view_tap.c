#include <assert.h>
#include <string.h>
#include "view-tap.h"

static void ignored_press_then_dismissal(void)
{
    struct hpclang_view_tap s = {0};
    int held = 0, presses = 0, releases = 0;
    int64_t last_up = 0;
    for (int64_t now = 0; now < 5000000; now += 50000) {
        /* A nonblank startup view precedes the actual dialog. A held softkey
         * flashes its pixels, but the UI ignores the first physical tap. */
        uint64_t frame = now < 500000 ? 99 : held ? 22 : 11;
        if (releases >= 2)
            frame = now < last_up + 200000 ? 0 : 33;
        int result = hpclang_view_tap_step(&s, now, frame, frame != 0, frame == 11, 1, held, !held);
        if (now < 500000)
            assert(result == HPCLANG_VIEW_WAIT);
        if (result == HPCLANG_VIEW_DOWN) {
            assert(!held);
            held = 1;
            ++presses;
        } else if (result == HPCLANG_VIEW_UP) {
            assert(held);
            held = 0;
            ++releases;
            last_up = now;
        } else if (result == HPCLANG_VIEW_DONE) {
            assert(presses == 2 && releases == 2 && !held);
            assert(frame == 33 && now >= last_up + 200000);
            return;
        } else {
            assert(result == HPCLANG_VIEW_WAIT);
        }
    }
    assert(!"dialog was not dismissed");
}

int main(void)
{
    ignored_press_then_dismissal();
    struct hpclang_view_tap s = {0};
    /* Neither a blank screen nor another stable view may receive this tap. */
    for (int64_t now = 0; now < 15000000; now += 50000) {
        int nonblank = now >= 1000000;
        assert(hpclang_view_tap_step(&s, now, nonblank ? 99 : 0, nonblank, 0, 1, 0, 1) ==
               HPCLANG_VIEW_WAIT);
    }
    assert(hpclang_view_tap_step(&s, 15000000, 99, 1, 0, 1, 0, 1) < 0);
    assert(strcmp(s.failure, "dialog-not-dismissed") == 0);
    return 0;
}
