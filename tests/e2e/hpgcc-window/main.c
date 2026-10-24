#include <winevent.h>
#include <wintimer.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include <limits.h>
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
static unsigned physical_edges, typed_count, expected_key, expected_down, frame_base;
static int physical(void *event, void *context)
{
    if ((uintptr_t)event != expected_key || (uintptr_t)context != expected_down)
        failure = 7;
    ++physical_edges;
    return 0;
}
static int typed(void *event, void *context)
{
    (void)context;
    const win_keytyped_t *key = event;
    static const struct
    {
        unsigned key;
        char ascii;
        unsigned alpha, left, right;
    } expected[] = {{KB_A, 'A', 1, 0, 0},
                    {KB_1, '1', 0, 0, 0},
                    {KB_A, 'A', 2, 0, 0},
                    {KB_A, 'a', 2, 1, 0},
                    {KB_A, 0, 0, 0, 1}};
    if (typed_count >= sizeof(expected) / sizeof(expected[0])) {
        failure = 8;
        return 17;
    }
    unsigned n = typed_count++;
    if (key->keycode != expected[n].key || key->ascii != expected[n].ascii ||
        key->alpha != expected[n].alpha || key->lshift != expected[n].left ||
        key->rshift != expected[n].right)
        failure = 9;
    return 17;
}
static int physical_tap(unsigned key)
{
    /* Inverse of the historical byte-column matrix layout. The test mailbox
     * changes real emulator GPIO switches; the window idle driver polls them. */
    unsigned scan = key >= 60 ? key - 3 : (key / 8) * 7 + key % 8 - 1;
    for (unsigned edge = 0; edge < 2; ++edge) {
        expected_key = key;
        expected_down = !edge;
        unsigned argument = scan | (expected_down << 8);
        unsigned before = physical_edges;
        uint64_t begun = hpclang_time_monotonic_us();
        hpclang_e2e_result = UINT32_C(0x4b490000) | argument;
        while (hpclang_e2e_result != (UINT32_C(0x4b4a0000) | argument))
            if (hpclang_time_monotonic_us() - begun > 2000000)
                return -1;
        while (physical_edges == before) {
            if (hpclang_win_step(1) < 0 || hpclang_time_monotonic_us() - begun > 2000000)
                return -1;
        }
        if (failure || physical_edges != before + 1)
            return -1;
        /* Repaint the current modifier state into the back page as well, then
         * capture the controller's physical status column independently. */
        if (win_post_event(7, NULL) || hpclang_win_error())
            return -1;
        hpg_flip();
        unsigned id = frame_base + physical_edges;
        hpclang_e2e_result = UINT32_C(0x46520000) | id;
        begun = hpclang_time_monotonic_us();
        while (hpclang_e2e_result != (UINT32_C(0x464b0000) | id))
            if (hpclang_time_monotonic_us() - begun > 2000000)
                return -1;
    }
    return 0;
}
static int keyboard_check(int buffered)
{
    physical_edges = typed_count = 0;
    frame_base = buffered ? 200 : 100;
    hpg_set_mode_gray16(buffered);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, physical, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, physical, NULL);
    win_add_event_handler(WIN_KEY_TYPED, 0, typed, NULL);
    static const unsigned keys[] = {KB_ALPHA,  KB_A, KB_1,     KB_ALPHA,  KB_ALPHA, KB_A,
                                    KB_LSHIFT, KB_A, KB_ALPHA, KB_RSHIFT, KB_A};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        if (physical_tap(keys[i]))
            return -1;
    if (typed_count != 5 || physical_edges != 22 || hpclang_win_error())
        return -1;
    return hpclang_win_reset();
}
int main(void)
{
    errno = 0;
    hpg_set_mode_mono(1);
    if (errno || keyboard_check(0) || keyboard_check(1))
        failure = failure ? failure : 5;
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
