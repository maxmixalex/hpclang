#include <winevent.h>
#include <wintimer.h>
#include <winkeys.h>
#include <winprogress.h>
#include <wintext.h>
#include <winimg.h>
#include <winsoftmenu.h>
#include <wincheckbox.h>
#include <winchoose.h>
#include <string.h>
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
static int checkpoint(unsigned id)
{
    uint64_t begun = hpclang_time_monotonic_us();
    hpclang_e2e_result = UINT32_C(0x46520000) | id;
    while (hpclang_e2e_result != (UINT32_C(0x464b0000) | id))
        if (hpclang_time_monotonic_us() - begun > 2000000)
            return -1;
    return 0;
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
        if (checkpoint(frame_base + physical_edges))
            return -1;
    }
    return 0;
}
static int keyboard_check(int buffered)
{
    physical_edges = typed_count = 0;
    frame_base = buffered ? 200 : 100;
    hpg_set_mode_gray16(buffered);
    /* Stock may enter the payload before its launch-key release. Announce
     * payload entry to disable launch retries, then wait for that real release
     * before installing observers or injecting the first test transition. */
    if (checkpoint(frame_base))
        return -1;
    uint64_t begun = hpclang_time_monotonic_us();
    while (keyb_isAnyKeyPressed())
        if (hpclang_time_monotonic_us() - begun > 2000000)
            return -1;
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
static win_widget_t *controls[4];
static unsigned active_control, control_changes;
static int control_changed(void *event, void *context)
{
    (void)context;
    if (event != controls[active_control] || win_progress_get_value(event) != 60)
        failure = 11;
    ++control_changes;
    return 0;
}
static int control_edge(unsigned key, unsigned down)
{
    unsigned scan = key >= 60 ? key - 3 : (key / 8) * 7 + key % 8 - 1;
    unsigned argument = scan | (down << 8);
    expected_key = key;
    expected_down = down;
    unsigned before = physical_edges;
    uint64_t begun = hpclang_time_monotonic_us();
    hpclang_e2e_result = UINT32_C(0x4b490000) | argument;
    while (hpclang_e2e_result != (UINT32_C(0x4b4a0000) | argument))
        if (hpclang_time_monotonic_us() - begun > 2000000)
            return -1;
    while (physical_edges == before)
        if (hpclang_win_step(1) < 0 || hpclang_time_monotonic_us() - begun > 2000000)
            return -1;
    return failure || physical_edges != before + 1 ? -1 : 0;
}
static int create_controls(int slider, int changed, double value)
{
    for (unsigned i = 0; i < 4; ++i) {
        controls[i] = win_progress_new(slider);
        if (!controls[i])
            return -1;
        win_widget_set_location(controls[i], 5 + (int)i * 30, 15);
        win_widget_set_size(controls[i], 20, 20);
        win_progress_set_orient(controls[i], (int)i);
        win_progress_set_event(controls[i], changed);
        win_progress_set_value(controls[i], value);
        /* This phase checks single presses. Repeat timing has a separate host
         * gate; never let screenshot/USB latency turn a tap into a repeat. */
        win_progress_set_delay(controls[i], 5000, 5000);
        win_add_widget(controls[i]);
    }
    return hpclang_win_error() ? -1 : 0;
}
static int remove_controls(void)
{
    /* Reverse order leaves the current first node until the singleton case. */
    for (unsigned i = 4; i; --i) {
        if (controls[i - 1]) {
            win_remove_widget(controls[i - 1]);
            win_widget_destroy(controls[i - 1]);
            controls[i - 1] = NULL;
        }
    }
    return hpclang_win_error() ? -1 : hpclang_win_reset();
}
static int controls_check(int buffered)
{
    hpg_set_mode_gray16(buffered);
    physical_edges = control_changes = 0;
    int changed = win_register_event();
    win_add_event_handler(changed, 0, control_changed, NULL);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, physical, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, physical, NULL);
    if (create_controls(1, changed, 50))
        return -1;
    static const unsigned increment[] = {KB_RT, KB_UP, KB_LF, KB_DN};
    for (active_control = 0; active_control < 4; ++active_control) {
        if (!win_widget_has_focus(controls[active_control]) ||
            control_edge(increment[active_control], 1) ||
            control_edge(increment[active_control], 0) ||
            win_progress_get_value(controls[active_control]) != 60)
            return -1;
        win_focus_next();
    }
    if (physical_edges != 8 || control_changes != 4 || remove_controls())
        return -1;
    /* Recreate without pumping idle: cursor timers have not fired. These are
     * the same four values just checked through the real input path. */
    hpg_clip_reset(NULL);
    hpg_clear();
    if (create_controls(0, -1, 60) || win_post_event(WIN_REPAINT_EVENT, NULL) ||
        hpclang_win_error())
        return -1;
    hpg_flip();
    if (checkpoint(buffered ? 400 : 300))
        return -1;
    return remove_controls();
}
static int text_tap(unsigned key)
{
    return control_edge(key, 1) || control_edge(key, 0);
}
static int text_check(int buffered)
{
    hpg_set_mode_gray16(buffered);
    physical_edges = 0;
    win_widget_t *entry = win_text_new_entry(2);
    win_widget_t *other = win_text_new_entry(2);
    if (!entry || !other)
        return -1;
    win_add_widget(entry);
    win_add_widget(other);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, physical, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, physical, NULL);
    if (!win_widget_has_focus(entry) || text_tap(KB_1) || text_tap(KB_2) || text_tap(KB_3) ||
        strcmp(win_text_get_text(entry), "12") || text_tap(KB_LF) || text_tap(KB_BKS) ||
        strcmp(win_text_get_text(entry), "2") || text_tap(KB_LSHIFT) || text_tap(KB_BKS) ||
        strcmp(win_text_get_text(entry), "") || text_tap(KB_1) || text_tap(KB_RSHIFT) ||
        text_tap(KB_BKS) || strcmp(win_text_get_text(entry), "") || text_tap(KB_DN) ||
        !win_widget_has_focus(other) || text_tap(KB_2) || strcmp(win_text_get_text(other), "2") ||
        text_tap(KB_UP) || !win_widget_has_focus(entry) || physical_edges != 26)
        return -1;
    win_remove_widget(other);
    win_widget_destroy(other);
    win_remove_widget(entry);
    win_widget_destroy(entry);
    if (hpclang_win_reset())
        return -1;
    /* Fresh controls have no elapsed blink callbacks. Capture deterministic
     * text after the real input phase; cursor timing has a separate host gate. */
    char bits[256 * 3] = {0};
    const unsigned char a[] = {1, 2, 4}, b[] = {7, 5, 7};
    for (unsigned row = 0; row < 3; ++row) {
        bits['A' * 3 + row] = (char)a[row];
        bits['B' * 3 + row] = (char)b[row];
    }
    hpg_font_t *font = hpg_alloc_font(bits, 256, 3, 3);
    win_widget_t *label = win_text_new_label("A\nB");
    entry = win_text_new_entry(2);
    if (!font || !label || !entry)
        return -1;
    win_widget_t *text[] = {label, entry};
    for (unsigned i = 0; i < 2; ++i) {
        win_widget_set_font(text[i], font);
        win_widget_set_location(text[i], 5 + (int)i * 20, 15);
        win_widget_set_size(text[i], 10, 10);
        win_add_widget(text[i]);
    }
    win_text_set_text(entry, "AB");
    hpg_clip_reset(NULL);
    hpg_clear();
    if (win_post_event(WIN_REPAINT_EVENT, NULL) || hpclang_win_error())
        return -1;
    hpg_flip();
    if (checkpoint(buffered ? 600 : 500))
        return -1;
    win_remove_widget(entry);
    win_widget_destroy(entry);
    win_remove_widget(label);
    win_widget_destroy(label);
    hpg_free_font(font);
    return hpclang_win_error() ? -1 : hpclang_win_reset();
}
static int image_check(int buffered)
{
    hpg_set_mode_gray16(buffered);
    hpg_t *image = hpg_alloc_gray16_image(2, 2);
    char bits[] = {0x55, (char)0xaa};
    hpg_pattern_t *pattern = hpg_alloc_pattern(bits, 2, 0);
    if (!image || !pattern)
        return -1;
    hpg_set_color(image, 119);
    hpg_draw_pixel_on(image, 0, 0);
    hpg_set_color(image, 255);
    hpg_draw_pixel_on(image, 1, 1);
    win_widget_t *widget = win_img_new(pattern, image);
    if (!widget)
        return -1;
    int width, height;
    win_widget_get_size(widget, &width, &height);
    if (width != 2 || height != 2)
        return -1;
    win_widget_set_location(widget, 5, 15);
    win_widget_set_size(widget, 9, 6);
    win_add_widget(widget);
    hpg_clip_reset(NULL);
    hpg_clear();
    if (win_post_event(WIN_REPAINT_EVENT, NULL) || hpclang_win_error())
        return -1;
    hpg_flip();
    if (checkpoint(buffered ? 800 : 700))
        return -1;
    win_remove_widget(widget);
    win_widget_destroy(widget);
    if (hpg_get_pixel(image, 0, 0) != 119)
        return -1;
    hpg_free_image(image);
    hpg_free_pattern(pattern);
    return hpclang_win_error() ? -1 : hpclang_win_reset();
}
static unsigned menu_deliveries;
static int menu_selected(void *event, void *context)
{
    const unsigned expected[] = {1, 6, 7, 1, 25};
    const win_softmenu_event_t *selected = event;
    if (context || menu_deliveries >= 5 ||
        (uintptr_t)selected->extra_data != expected[menu_deliveries] || selected->lshift ||
        selected->rshift)
        failure = 15;
    ++menu_deliveries;
    return 17;
}
static int menu_check(int buffered)
{
    hpg_set_mode_gray16(buffered);
    menu_deliveries = physical_edges = 0;
    int event = win_register_event();
    win_add_event_handler(event, 0, menu_selected, NULL);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, physical, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, physical, NULL);
    win_widget_t *menu = win_softmenu_new();
    if (!menu)
        return -1;
    for (unsigned i = 0; i < 30; ++i)
        if (win_softmenu_add_item(menu, "A", event, (void *)(uintptr_t)(i + 1)) != (int)i)
            return -1;
    win_add_widget(menu);
    const unsigned keys[] = {KB_A, KB_F,      KB_L, KB_A, KB_LSHIFT, KB_L,
                             KB_A, KB_LSHIFT, KB_L, KB_A, KB_L};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        if (text_tap(keys[i]))
            return -1;
    win_softmenu_set_enabled(menu, 5, 0);
    if (text_tap(KB_F) || menu_deliveries != 5 || physical_edges != 24)
        return -1;
    win_remove_widget(menu);
    win_widget_destroy(menu);
    char glyphs[256 * 6] = {0};
    for (unsigned row = 0; row < 4; ++row)
        glyphs['A' * 6 + row] = (char)(1u << row);
    hpg_font_t *font = hpg_alloc_font(glyphs, 256, 6, 4);
    menu = win_softmenu_new();
    if (!font || !menu)
        return -1;
    win_widget_set_font(menu, font);
    for (int i = 0; i < 7; ++i)
        win_softmenu_set_item(menu, i, "A", event, NULL);
    win_softmenu_set_disable_colors(menu, 1, 51, 102);
    win_softmenu_set_enabled(menu, 1, 0);
    win_softmenu_set_colors(menu, 6, 17, 238);
    win_add_widget(menu);
    for (unsigned page = 0; page < 2; ++page) {
        hpg_clip_reset(NULL);
        hpg_clear();
        if (win_post_event(WIN_REPAINT_EVENT, NULL) || hpclang_win_error())
            return -1;
        hpg_flip();
        if (checkpoint((buffered ? 1000 : 900) + page) || text_tap(KB_L))
            return -1;
    }
    win_remove_widget(menu);
    win_widget_destroy(menu);
    hpg_free_font(font);
    return hpclang_win_error() ? -1 : hpclang_win_reset();
}
static int checkbox_check(int buffered)
{
    hpg_set_mode_gray16(buffered);
    physical_edges = 0;
    win_widget_t *menu = win_softmenu_new();
    win_widget_t *a = win_checkbox_new("A"), *b = win_checkbox_new("B");
    if (!menu || !a || !b)
        return -1;
    win_checkbox_set_menu(a, menu, 0);
    win_checkbox_set_menu(b, menu, 0);
    win_add_widget(menu);
    win_add_widget(a);
    win_add_widget(b);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, physical, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, physical, NULL);
    if (!win_widget_has_focus(a) || text_tap(KB_W) || !win_checkbox_is_selected(a) ||
        text_tap(KB_DN) || !win_widget_has_focus(b) || text_tap(KB_A) ||
        !win_checkbox_is_selected(b) || text_tap(KB_UP) || !win_widget_has_focus(a) ||
        text_tap(KB_A) || win_checkbox_is_selected(a))
        return -1;
    win_remove_widget(a);
    win_widget_destroy(a);
    if (!win_widget_has_focus(b) || text_tap(KB_A) || win_checkbox_is_selected(b))
        return -1;
    win_checkbox_set_menu(b, menu, 1);
    if (text_tap(KB_B) || !win_checkbox_is_selected(b))
        return -1;
    win_checkbox_set_menu(b, NULL, 0);
    if (text_tap(KB_B) || !win_checkbox_is_selected(b) || physical_edges != 16)
        return -1;
    win_remove_widget(b);
    win_widget_destroy(b);
    win_remove_widget(menu);
    win_widget_destroy(menu);
    if (hpclang_win_reset())
        return -1;
    char glyphs[256 * 3] = {0};
    glyphs['A' * 3] = 1;
    glyphs['A' * 3 + 1] = 2;
    glyphs['A' * 3 + 2] = 4;
    hpg_font_t *font = hpg_alloc_font(glyphs, 256, 3, 3);
    win_widget_t *boxes[] = {win_checkbox_new("A"), win_checkbox_new("")};
    if (!font || !boxes[0] || !boxes[1])
        return -1;
    for (unsigned i = 0; i < 2; ++i) {
        win_widget_set_font(boxes[i], font);
        win_widget_set_location(boxes[i], 5 + (int)i * 20, 15);
        win_widget_set_size(boxes[i], 12, 9);
        win_add_widget(boxes[i]);
    }
    win_checkbox_set_selected(boxes[0], 1);
    for (unsigned focus = 0; focus < 2; ++focus) {
        if (!win_widget_has_focus(boxes[focus]))
            return -1;
        hpg_clip_reset(NULL);
        hpg_clear();
        if (win_post_event(WIN_REPAINT_EVENT, NULL) || hpclang_win_error())
            return -1;
        hpg_flip();
        if (checkpoint((buffered ? 1200 : 1100) + focus))
            return -1;
        win_focus_next();
    }
    for (unsigned i = 2; i; --i) {
        win_remove_widget(boxes[i - 1]);
        win_widget_destroy(boxes[i - 1]);
    }
    hpg_free_font(font);
    return hpclang_win_error() ? -1 : hpclang_win_reset();
}

static int choose_check(int buffered)
{
    hpg_set_mode_gray16(buffered);
    physical_edges = 0;
    win_widget_t *menu = win_softmenu_new(), *a = win_choose_new(), *b = win_choose_new();
    if (!menu || !a || !b)
        return -1;
    win_choose_add(a, "one");
    win_choose_add(a, "two");
    win_choose_add(a, "three");
    win_choose_set_menu(a, menu, 0);
    win_choose_set_menu(b, menu, 0);
    win_add_widget(menu);
    win_add_widget(a);
    win_add_widget(b);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, physical, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, physical, NULL);
    if (text_tap(KB_W) || win_choose_get_index(a) != 1 || text_tap(KB_A) || text_tap(KB_DN) ||
        text_tap(KB_E) || win_choose_get_index(a) != 1 || text_tap(KB_A) || text_tap(KB_UP) ||
        text_tap(KB_F) || win_choose_get_index(a) != 0 || text_tap(KB_A) || text_tap(KB_UP) ||
        text_tap(KB_F) || win_choose_get_index(a) != 2 || text_tap(KB_DN) ||
        !win_widget_has_focus(b) || text_tap(KB_A) || text_tap(KB_UP) || text_tap(KB_F) ||
        win_choose_get_index(b) != -1 || text_tap(KB_A))
        return -1;
    win_remove_widget(b);
    win_widget_destroy(b);
    if (!win_widget_has_focus(a) || text_tap(KB_A) || text_tap(KB_E))
        return -1;
    win_choose_set_menu(a, menu, 1);
    if (text_tap(KB_B) || text_tap(KB_E))
        return -1;
    win_choose_set_menu(a, NULL, 0);
    if (text_tap(KB_B) || physical_edges != 40)
        return -1;
    win_remove_widget(a);
    win_widget_destroy(a);
    win_remove_widget(menu);
    win_widget_destroy(menu);
    if (hpclang_win_reset())
        return -1;
    physical_edges = 0;
    char glyphs[256 * 3] = {0};
    const unsigned char forward[] = {1, 2, 4}, backward[] = {4, 2, 1};
    for (unsigned row = 0; row < 3; ++row) {
        glyphs['A' * 3 + row] = (char)forward[row];
        glyphs['B' * 3 + row] = (char)backward[row];
    }
    hpg_font_t *font = hpg_alloc_font(glyphs, 256, 3, 3);
    menu = win_softmenu_new();
    a = win_choose_new();
    if (!font || !menu || !a)
        return -1;
    win_widget_set_font(a, font);
    win_widget_set_location(a, 1, 1);
    win_widget_set_size(a, 5, 5);
    for (unsigned i = 0; i < 40; ++i)
        win_choose_add_with_label(a, i % 2 ? "B" : "A", "A");
    win_choose_set_menu(a, menu, 0);
    win_add_widget(menu);
    win_add_widget(a);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, physical, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, physical, NULL);
    for (unsigned frame = 0; frame < 4; ++frame) {
        if (frame == 1 && text_tap(KB_A))
            return -1;
        if (frame == 2 && text_tap(KB_UP))
            return -1;
        if (frame == 3 && (text_tap(KB_E) || win_choose_get_index(a) != 0 || text_tap(KB_A) ||
                           text_tap(KB_UP) || text_tap(KB_F) || win_choose_get_index(a) != 39))
            return -1;
        hpg_clip_reset(NULL);
        hpg_clear();
        int result = win_post_event(WIN_REPAINT_EVENT, NULL);
        if (result != (frame == 1 || frame == 2) || hpclang_win_error())
            return -1;
        hpg_flip();
        if (checkpoint((buffered ? 1400 : 1300) + frame))
            return -1;
    }
    if (physical_edges != 12)
        return -1;
    win_remove_widget(a);
    win_widget_destroy(a);
    win_remove_widget(menu);
    win_widget_destroy(menu);
    hpg_free_font(font);
    return hpclang_win_error() ? -1 : hpclang_win_reset();
}
int main(void)
{
    errno = 0;
    hpg_set_mode_mono(1);
    if (errno || keyboard_check(0) || keyboard_check(1))
        failure = failure ? failure : 5;
    if (!failure && (controls_check(0) || controls_check(1)))
        failure = failure ? failure : 10;
    if (!failure && (text_check(0) || text_check(1)))
        failure = failure ? failure : 12;
    if (!failure && (image_check(0) || image_check(1)))
        failure = failure ? failure : 13;
    if (!failure && (menu_check(0) || menu_check(1)))
        failure = failure ? failure : 14;
    if (!failure && (checkbox_check(0) || checkbox_check(1)))
        failure = failure ? failure : 16;
    if (!failure && (choose_check(0) || choose_check(1)))
        failure = failure ? failure : 17;
    /* Successful allocations may retain errno from an internal failed growth
     * attempt. Attribute errors to this mode switch, not prior successful work. */
    errno = 0;
    hpg_set_mode_mono(1);
    hpg_clip_reset(NULL);
    if (errno || hpclang_time_read_us(&start))
        failure = failure ? failure : 5;
    event_id = win_register_event();
    win_add_event_handler(event_id, 0, receive, &deliveries);
    win_add_event_handler(7, 0, paint, NULL);
    win_add_event_handler(WIN_IDLE_EVENT, -100, deadline, NULL);
    win_add_timer(advance, &ticks, 30, 0);
    win_repaint();
    if (!failure)
        win_event_loop();
    uint64_t end;
    if (!failure &&
        (hpclang_win_error() || hpclang_time_read_us(&end) || ticks != 4 || paints != 4 ||
         deliveries != 4 || end - start < 120000 || end - start > 2000000))
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
