#include <hpgraphics.h>
#include <hpclang/display.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <hpclang/font-package.h>
#include "display-surface.h"
#include "hpg-internal.h"
#include "win-repaint.h"
#include <wincore.h>
#include <winprogress.h>
#include <wintext.h>
#include <winimg.h>
#include <winsoftmenu.h>
#include <wincheckbox.h>
#include <winchoose.h>
#include <hpclang/device.h>
#include <winkeys.h>
#include "hpgcc-keyboard.h"
#define HPCLANG_HPCONIO_NO_AUTOINIT
#include <hpconio.h>
#include <hpclang/sound.h>
#include <hpclang/stdio.h>
#include "stdio-routing.h"

static uint64_t window_time;
int hpclang_time_read_us(uint64_t *value)
{
    *value = window_time;
    return 0;
}
int hpclang_hpgcc_keymatrix_read(keymatrix *matrix)
{
    matrix->full = 0;
    return 0;
}

uint32_t hpclang_font_advance(const struct hpclang_font *font)
{
    return font ? 4 : 0;
}
uint32_t hpclang_font_height(const struct hpclang_font *font)
{
    return font ? 6 : 0;
}
static const unsigned char *input_bytes;
static int input_end = HPCLANG_STDIO_EOF, stream_error;
int hpclang_hpgcc_console_read(void *context)
{
    (void)context;
    assert(input_bytes && "graphics checks must not poll input");
    if (*input_bytes)
        return *input_bytes++;
    if (input_end == HPCLANG_STDIO_ERROR)
        errno = ECANCELED;
    return input_end;
}
void hpclang_stdio_clear_streams(void)
{
    stream_error = 0;
}
int hpg_test_getc(FILE *stream)
{
    assert(stream == stdin);
    int result = hpclang_stdio_route_read();
    if (result == HPCLANG_STDIO_ERROR) {
        stream_error = 1;
        return EOF;
    }
    return result;
}
int hpg_test_getchar(void)
{
    /* Optimized libc headers may inline getchar as getc(stdin). Both entry
     * points must consume the scripted stream rather than the host terminal. */
    return hpg_test_getc(stdin);
}
int hpg_test_putc(int byte, FILE *stream)
{
    assert(stream == stdout);
    return hpclang_stdio_route_write(1, (unsigned char)byte) ? EOF : (unsigned char)byte;
}
int hpg_test_putchar(int byte)
{
    return hpg_test_putc(byte, stdout);
}
int hpg_test_fputs(const char *text, FILE *stream)
{
    assert(stream == stdout);
    while (*text)
        if (hpg_test_putchar((unsigned char)*text++) == EOF)
            return EOF;
    return 0;
}
int hpg_test_ferror(FILE *stream)
{
    assert(stream == stdin);
    return stream_error;
}
static unsigned tones;
int hpclang_sound_play(uint32_t frequency, uint32_t duration, hpclang_sound_cancel_fn cancel,
                       void *context)
{
    assert(frequency == 880 && duration == 100 && !cancel && !context);
    ++tones;
    return 0;
}

/* Distinct font providers let this test observe adapter selection and HP-byte
 * translation independently of the clean font renderer's own raster tests. */
static const uint8_t wide_bits[] = {0xff, 0x80};
static const struct hpclang_bitmap_glyph wide_glyph = {
    .codepoint = 0x03c9, .bitmap = wide_bits, .width = 9, .height = 1, .stride = 2};
static const struct hpclang_bitmap_glyph *const fixture_glyphs[] = {&wide_glyph};
static const struct hpclang_bitmap_font_data fixture_data = {.glyphs = fixture_glyphs,
                                                             .glyph_count = 1};
const struct hpclang_font hpclang_font_newrpl_font6_menus_full = HPCLANG_BITMAP_FONT(fixture_data);
const struct hpclang_font hpclang_font_newrpl_font8_styleb_full = HPCLANG_BITMAP_FONT(fixture_data);
static const struct hpclang_font native_mini = {1, 2, NULL}, native_big = {1, 3, NULL};
static int classic, available;
static const struct hpclang_font *drawn_font;
static uint32_t drawn_code;
const struct hpclang_font *hpclang_font_big(void)
{
    return classic ? &native_big : NULL;
}
const struct hpclang_font *hpclang_font_mini(void)
{
    return &native_mini;
}
const struct hpclang_font *hpclang_font_default(void)
{
    return &native_big;
}
int hpclang_font_available(const struct hpclang_font *font)
{
    return font && available;
}
void hpclang_surface_draw_codepoint(struct hpclang_surface *s, const struct hpclang_font *f,
                                    int32_t x, int32_t y, uint32_t cp, uint8_t color)
{
    drawn_font = f;
    drawn_code = cp;
    hpclang_surface_set_pixel(s, x, y, color);
}
void hpclang_surface_draw_char(struct hpclang_surface *s, const struct hpclang_font *f, int32_t x,
                               int32_t y, uint8_t cp, uint8_t color)
{
    hpclang_surface_draw_codepoint(s, f, x, y, cp, color);
}

static void defaults(void)
{
    hpg_t *g = hpg_alloc_mono_image(16, 16);
    assert(g);
    hpg_draw_text_on(g, "AB\nC", 1, 1);
    assert(hpg_get_font(g) == hpg_get_minifont());
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 5, 1) == 255);
    assert(hpg_get_pixel(g, 1, 7) == 255);
    assert(drawn_font == &hpclang_font_newrpl_font6_menus_full);
    hpg_draw_letter_on(g, (char)0x87, 1, 1);
    assert(drawn_code == 0x03c0);
    classic = available = 1;
    hpg_draw_letter_on(g, (char)0x87, 1, 1);
    assert(drawn_font == &native_mini && drawn_code == 0x87);
    classic = 0;
    hpg_draw_letter_on(g, 'A', 1, 1);
    assert(drawn_font == &hpclang_font_newrpl_font6_menus_full);
    hpg_set_font(g, hpg_get_bigfont());
    assert(hpg_font_get_advance(hpg_get_bigfont()) == 6);
    assert(hpg_font_get_height(hpg_get_bigfont()) == 8);
    hpg_draw_letter_on(g, 'A', 1, 1);
    assert(drawn_font == &native_big);
    available = 0;
    hpg_draw_letter_on(g, 'A', 1, 1);
    assert(drawn_font == &hpclang_font_newrpl_font8_styleb_full);
    hpg_clear_on(g);
    hpg_draw_letter_on(g, (char)0x9a, 2, 3);
    for (int x = 0; x < 16; ++x)
        assert(hpg_get_pixel(g, x, 3) == (x >= 2 && x < 8 ? 255 : 0));
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_letter_on(g, (char)0x9a, 2, 3);
    for (int x = 0; x < 16; ++x)
        assert(hpg_get_pixel(g, x, 3) == 0);
    hpg_draw_letter_on(g, (char)0x80, 2, 3);
    assert(hpg_get_pixel(g, 2, 9) == 255 && hpg_get_pixel(g, 6, 5) == 255);
    hpg_free_font(hpg_get_minifont());
    hpg_free_font(hpg_get_bigfont());
    hpg_free_image(g);
}

static void pixmaps(void)
{
    char *source[] = {
        "4 2 5 2",   "aa c #000000",           "bb c #ffffff", "cc c #ff0000 g #00ff00 g4 #0000ff",
        "dd c None", "ee s ignored c #808080", "aabbccdd",     "ee??aabb"};
    hpg_t *g = hpg_load_xpm_gray16(source);
    assert(g && hpg_get_width(g) == 4 && hpg_get_height(g) == 2);
    assert(hpg_get_pixel(g, 0, 0) == 255 && hpg_get_pixel(g, 1, 0) == 0);
    assert(hpg_get_pixel(g, 2, 0) == 238 && hpg_get_pixel(g, 3, 0) == 0);
    assert(hpg_get_pixel(g, 0, 1) == 119 && hpg_get_pixel(g, 1, 1) == 0);
    assert(hpg_get_color(g) == 0);
    hpg_free_image(g);
    g = hpg_load_xpm_gray4(source);
    assert(g && hpg_get_pixel(g, 0, 1) == 85);
    hpg_free_image(g);
    g = hpg_load_xpm_mono(source);
    assert(g && hpg_get_pixel(g, 0, 1) == 0 && hpg_get_pixel(g, 2, 0) == 255);
    hpg_free_image(g);
    char *space_key[] = {"2\t1 2 1 0 0", "  c #000000", ". g4 invalid c #FFFFFF", " ."};
    g = hpg_load_xpm_mono(space_key);
    assert(g && hpg_get_pixel(g, 0, 0) == 255 && hpg_get_pixel(g, 1, 0) == 0);
    hpg_free_image(g);
    char *short_row[] = {"2 1 1 2", "aa c #000000", "aa"};
    assert(!hpg_load_xpm_gray16(short_row) && errno == EINVAL);
    char *duplicate[] = {"1 1 2 1", "a c #000000", "a c #ffffff", "a"};
    assert(!hpg_load_xpm_mono(duplicate) && errno == EINVAL);
    char *bad_header[] = {"9999999999999999999999 1 1 1"};
    assert(!hpg_load_xpm_mono(bad_header) && errno == EINVAL);
    bad_header[0] = "0 1 1 1";
    assert(!hpg_load_xpm_mono(bad_header) && errno == EINVAL);
    bad_header[0] = "1x 1 1 1";
    assert(!hpg_load_xpm_mono(bad_header) && errno == EINVAL);
    assert(!hpg_load_xpm_mono(NULL) && errno == EINVAL);
}

static struct hpclang_surface *display;
static unsigned borrowed;
static struct hpclang_surface *pages[2];
static unsigned page_count, back, published, flips, ended;
static int mode_failure;
static int present_failure, publish_failure;
static unsigned display_height = 6, display_width = 8;
const struct hpclang_device_info *hpclang_device_info(void)
{
    static struct hpclang_device_info info;
    info.display_width = display_width;
    info.display_height = display_height;
    return &info;
}
static int configure(unsigned levels, unsigned count)
{
    if (mode_failure) {
        errno = ENOMEM;
        return -1;
    }
    for (unsigned i = 0; i < 2; ++i) {
        hpclang_surface_destroy(pages[i]);
        pages[i] =
            i < count ? hpclang_surface_create(display_width + 1, display_height, levels) : NULL;
        assert(i >= count || pages[i]);
        if (pages[i])
            pages[i]->width = display_width;
    }
    page_count = count;
    back = count - 1;
    return 0;
}
int hpclang_display_begin_pages(uint32_t levels, unsigned count)
{
    if (page_count) {
        errno = EBUSY;
        return -1;
    }
    return configure(levels, count);
}
int hpclang_display_replace_pages(uint32_t levels, unsigned count)
{
    assert(page_count);
    return configure(levels, count);
}
int hpclang_display_publish(void)
{
    assert(page_count);
    if (publish_failure) {
        errno = EIO;
        return -1;
    }
    ++published;
    return 0;
}
int hpclang_display_indicator_set(unsigned segment, uint8_t color)
{
    assert(segment < 6 && page_count);
    struct hpclang_surface view = *pages[back];
    ++view.width;
    hpclang_surface_set_pixel(&view, (int)display_width, (int)segment, color);
    return 0;
}
int hpclang_display_present(uint32_t timeout)
{
    assert(timeout && page_count == 2);
    if (present_failure) {
        errno = ETIMEDOUT;
        return -1;
    }
    back ^= 1;
    ++flips;
    return 0;
}
void hpclang_display_end(void)
{
    for (unsigned i = 0; i < 2; ++i) {
        hpclang_surface_destroy(pages[i]);
        pages[i] = NULL;
    }
    page_count = 0;
    ++ended;
}
struct hpclang_surface *hpclang_display_surface(void)
{
    ++borrowed;
    return page_count ? pages[back] : display;
}

static void modes(void)
{
    hpg_set_mode_gray4(0);
    assert(page_count == 1 && hpclang_surface_levels(pages[0]) == 4);
    hpg_set_color(NULL, 85);
    hpg_draw_pixel(1, 1);
    assert(hpg_get_pixel(NULL, 1, 1) == 85 && published == 1);
    hpg_fill_rect(0, 0, 2, 2);
    assert(published == 2);
    hpg_flip();
    assert(flips == 0);
    mode_failure = 1;
    hpg_set_mode_gray16(1);
    assert(errno == ENOMEM && page_count == 1 && hpg_get_pixel(NULL, 1, 1) == 85);
    mode_failure = 0;
    hpg_set_mode_gray16(1);
    assert(page_count == 2 && hpg_get_color(NULL) == 85);
    hpg_draw_pixel(1, 1);
    assert(published == 2);
    hpg_flip();
    assert(flips == 1 && hpg_get_pixel(NULL, 1, 1) == 0);
    hpg_draw_pixel(2, 2);
    hpg_flip();
    assert(hpg_get_pixel(NULL, 1, 1) == 85 && hpg_get_pixel(NULL, 2, 2) == 0);
    hpg_set_mode_mono(0);
    hpg_clear();
    assert(page_count == 1 && published == 3);
    hpg_cleanup();
    assert(!page_count && ended == 1);
    hpg_cleanup();
    assert(ended == 1);
    assert(hpclang_display_begin_pages(2, 2) == 0);
    hpg_set_mode_gray4(0);
    assert(errno == EBUSY && page_count == 2);
    hpg_cleanup();
    assert(ended == 1 && page_count == 2); /* Other owner's session survives. */
    hpclang_display_end();
}

static void indicators(void)
{
    struct hpclang_surface original = *display;
    ++original.width;
    hpclang_surface_set_pixel(&original, 8, HPG_INDICATOR_WAIT, 255);
    unsigned previous = published;
    hpg_set_indicator(HPG_INDICATOR_ALPHA, 255);
    assert(page_count == 1 && published == previous + 1);
    assert(hpg_get_pixel(NULL, 1, 1) == 255);
    struct hpclang_surface status = *pages[back];
    ++status.width;
    assert(hpclang_surface_get_pixel(&status, 8, HPG_INDICATOR_ALPHA) == 255);
    assert(hpclang_surface_get_pixel(&status, 8, HPG_INDICATOR_WAIT) == 255);
    assert(hpclang_surface_get_pixel(&original, 8, HPG_INDICATOR_ALPHA) == 0);
    publish_failure = 1;
    assert(hpclang_hpg_indicator_set(HPG_INDICATOR_ALPHA, 255) == -1 && errno == EIO);
    publish_failure = 0;
    hpg_set_mode(NULL, HPG_MODE_XOR);
    hpg_set_indicator(HPG_INDICATOR_ALPHA, 255);
    assert(hpclang_surface_get_pixel(&status, 8, HPG_INDICATOR_ALPHA) == 0);
    hpg_set_mode_gray4(1);
    previous = published;
    hpg_set_indicator(HPG_INDICATOR_BATTERY, 255);
    assert(published == previous);
    status = *pages[back];
    ++status.width;
    assert(hpclang_surface_get_pixel(&status, 8, HPG_INDICATOR_BATTERY) == 255);
    hpg_flip();
    status = *pages[back];
    ++status.width;
    assert(hpclang_surface_get_pixel(&status, 8, HPG_INDICATOR_BATTERY) == 0);
    assert(hpclang_hpg_indicator_set(6, 255) == -1);
    hpg_set_indicator(6, 255);
    assert(errno == EINVAL && published == previous);
    hpg_cleanup();
    assert(hpclang_surface_get_pixel(display, 1, 1) == 255);
}

static void shared_screen(void)
{
    unsigned previous_end = ended, previous_publish = published;
    mode_failure = 1;
    assert(!hpclang_hpg_acquire_screen() && errno == ENOMEM);
    assert(!page_count && ended == previous_end && published == previous_publish);
    assert(hpclang_surface_get_pixel(display, 1, 1) == 255);
    mode_failure = 0;
    hpg_t *shared = hpclang_hpg_acquire_screen();
    assert(shared && page_count == 1 && published == previous_publish);
    assert(hpg_get_pixel(shared, 1, 1) == 255);
    struct hpclang_surface *page = hpclang_hpg_surface(shared);
    hpclang_surface_set_pixel(page, 2, 2, 255);
    assert(hpclang_hpg_acquire_screen() == shared && hpclang_hpg_surface(shared) == page);
    assert(hpclang_surface_get_pixel(page, 2, 2) == 255);
    hpclang_hpg_finish(shared);
    assert(published == previous_publish + 1);
    hpg_cleanup();
    assert(ended == previous_end + 1 && !page_count);
    assert(hpclang_surface_get_pixel(display, 2, 2) == 0);

    for (int dbuf = 0; dbuf <= 1; ++dbuf) {
        hpg_set_mode_gray16(dbuf);
        hpg_set_color(NULL, 119);
        hpg_set_mode(NULL, HPG_MODE_XOR);
        hpg_clip(NULL, 1, 1, 7, 5);
        page = hpclang_display_surface();
        hpclang_surface_set_pixel(page, 2, 2, 85);
        previous_publish = published;
        unsigned previous_flips = flips;
        shared = hpclang_hpg_acquire_screen();
        assert(shared && hpclang_hpg_surface(shared) == page);
        assert(page_count == (unsigned)dbuf + 1 && hpclang_surface_levels(page) == 16);
        assert(hpclang_surface_get_pixel(page, 2, 2) == 85);
        assert(shared->color == 119 && shared->mode == HPG_MODE_XOR && shared->left == 1 &&
               shared->top == 1 && shared->right == 7 && shared->bottom == 5);
        hpclang_hpg_finish(shared);
        assert(published == previous_publish + !dbuf && flips == previous_flips);
        hpg_cleanup();
    }
    assert(hpclang_display_begin_pages(4, 2) == 0);
    page = hpclang_display_surface();
    hpclang_surface_set_pixel(page, 3, 3, 170);
    previous_end = ended;
    assert(!hpclang_hpg_acquire_screen() && errno == EBUSY);
    hpg_cleanup();
    assert(ended == previous_end && page_count == 2 && hpclang_display_surface() == page);
    assert(hpclang_surface_get_pixel(page, 3, 3) == 170);
    hpclang_display_end();
}

static char captured[32];
static unsigned captured_length;
static int capture_byte(void *context, unsigned stream, unsigned char byte)
{
    (void)context;
    assert(stream == 1 && captured_length + 1 < sizeof captured);
    captured[captured_length++] = (char)byte;
    captured[captured_length] = 0;
    return 0;
}
static unsigned buffer_calls;
static char dynamic_buffer[8];
static char *next_buffer(void)
{
    ++buffer_calls;
    return dynamic_buffer;
}
static void edited_lines(void)
{
    char text[16];
    hpg_set_mode_mono(0);
    _clear_screen();
    struct hpclang_surface *s = hpclang_display_surface();
    hpclang_surface_set_pixel(s, 7, 5, 255);
    input_bytes = (const unsigned char *)"ABCDEFG\b\b\b\b\b\b\n";
    assert(gets(text) == text && !strcmp(text, "A"));
    assert(hpclang_surface_get_pixel(s, 0, 0) == 255);
    assert(hpclang_surface_get_pixel(s, 7, 5) == 255); /* restore pre-scroll graphics */
    assert(getx() == 0 && gety() == 1);
    hpg_set_mode_gray16(1);
    _clear_screen();
    input_bytes = (const unsigned char *)"ABC\b\n";
    unsigned previous = flips;
    assert(_gets(text) == text && !strcmp(text, "AB"));
    assert(flips == previous + 5);
    for (unsigned i = 0; i < 2; ++i) {
        assert(hpclang_surface_get_pixel(pages[i], 0, 0) == 255);
        assert(hpclang_surface_get_pixel(pages[i], 4, 0) == 255);
        assert(hpclang_surface_get_pixel(pages[i], 0, 6) == 0);
    }
    input_bytes = (const unsigned char *)"X";
    input_end = HPCLANG_STDIO_ERROR;
    assert(!hpclang_hpgcc_gets(text, sizeof text) && errno == ECANCELED && !strcmp(text, "X"));
    input_end = HPCLANG_STDIO_EOF;
    stream_error = 0;
    input_bytes = (const unsigned char *)"A\n";
    present_failure = 1;
    unsigned before_flips = flips;
    assert(!hpclang_hpgcc_gets(text, sizeof text) && errno == ETIMEDOUT && !text[0]);
    assert(flips == before_flips && page_count == 2);
    present_failure = 0;
    hpg_cleanup();
    unsigned before = borrowed;
    const struct hpclang_stdio_callbacks io = {.read = hpclang_hpgcc_console_read,
                                               .write = capture_byte};
    assert(hpclang_stdio_bind(&io) == 0);
    input_bytes = (const unsigned char *)"AB\bC\n";
    assert(hpclang_hpgcc_gets(text, sizeof text) == text && !strcmp(text, "AC"));
    assert(!strcmp(captured, "AB\b \bC\n") && borrowed == before && !page_count);
    struct
    {
        char before;
        char text[4];
        char after;
    } bounded = {'x', "", 'y'};
    captured_length = 0;
    input_bytes = (const unsigned char *)"ABCDE\bZ\n";
    assert(gets(bounded.text) == bounded.text && !strcmp(bounded.text, "ABZ"));
    assert(bounded.before == 'x' && bounded.after == 'y');
    captured_length = 0;
    input_bytes = (const unsigned char *)"123\n";
    assert(!_gets(&bounded.text[2]) && errno == EINVAL && !strcmp(bounded.text, "ABZ"));
    assert(bounded.after == 'y');
    char larger[12];
    volatile int choose_small = 1;
    captured_length = 0;
    input_bytes = (const unsigned char *)"123456\n";
    char *choice = choose_small ? bounded.text : larger;
    char *chosen = gets(choice);
    /* A pointer choice has no declared array bound. */
    assert(!chosen && errno == EINVAL);
    assert(bounded.before == 'x' && bounded.after == 'y');
    input_bytes = (const unsigned char *)"untouched\n";
    const unsigned char *unread = input_bytes;
    strcpy(dynamic_buffer, "saved");
    assert(!gets(next_buffer()) && errno == EINVAL);
    assert(buffer_calls == 1 && input_bytes == unread && !strcmp(dynamic_buffer, "saved"));
    volatile unsigned dynamic_size = 4;
    char variable[dynamic_size];
    memcpy(variable, "old", 4);
    assert(!gets(variable) && errno == EINVAL && !memcmp(variable, "old", 4));
    assert(input_bytes == unread);
    assert(hpclang_stdio_unbind() == 0);
    strcpy(text, "OK");
    assert(!hpclang_hpgcc_gets(text, 0) && errno == EINVAL && !strcmp(text, "OK"));
    assert(!hpclang_hpgcc_gets(text, SIZE_MAX) && errno == EINVAL && !strcmp(text, "OK"));
    input_bytes = NULL;
}
static void historical_console(void)
{
    struct hpclang_surface *saved_display = display;
    display_height = 18;
    display = hpclang_surface_create_mono(9, display_height);
    assert(display);
    display->width = 8;
    unsigned previous_borrowed = borrowed;
    assert(hpclang_hpgcc_console_enable() == 0);
    assert(hpclang_hpgcc_console_enable() == 0 && borrowed == previous_borrowed && !page_count);
    assert(hpclang_stdio_route_flush(1) == 0 && borrowed == previous_borrowed);
    assert(hpclang_display_begin_pages(2, 2) == 0);
    assert(hpclang_stdio_route_write(1, 'A') == -1 && errno == EBUSY);
    hpg_cleanup();
    assert(page_count == 2);
    hpclang_display_end();
    assert(screen_width() == 2 && screen_height() == 3);
    _clear_screen();
    struct hpclang_surface *s = hpclang_display_surface();
    hpclang_surface_set_pixel(s, 7, 5, 255);
    assert(hpclang_stdio_route_write(1, 'A') == 0);
    assert(getx() == 1 && gety() == 0);
    assert(hpclang_surface_get_pixel(s, 0, 0) == 255);
    assert(hpclang_surface_get_pixel(s, 7, 5) == 255); /* unrelated graphics preserved */
    home();
    assert(hpclang_stdio_route_write(2, 0x87) == 0 && drawn_code == 0x3c0);
    clreol();
    assert(hpclang_surface_get_pixel(s, 0, 0) == 255);
    assert(hpclang_surface_get_pixel(s, 7, 5) == 0);
    gotoxy(-1, 0);
    assert(errno == EINVAL && getx() == 1);
    gotoxy(2, 0);
    assert(errno == EINVAL && getx() == 1);
    gotoxy(1, 2);
    hpclang_surface_set_pixel(s, 7, 11, 255);
    assert(hpclang_stdio_route_write(1, 'B') == 0); /* immediate wrap/scroll */
    assert(getx() == 0 && gety() == 2 && hpclang_surface_get_pixel(s, 0, 0) == 0);
    assert(hpclang_surface_get_pixel(s, 7, 5) == 255); /* graphics scroll with text */
    set_tabsize(INT_MAX);
    assert(get_tabsize() == INT_MAX);
    assert(hpclang_stdio_route_write(1, '\t') == 0 && getx() == 1);
    set_tabsize(0);
    assert(errno == EINVAL && get_tabsize() == INT_MAX);
    set_tabsize(4);
    assert(hpclang_stdio_route_write(1, '\t') == 0 && getx() == 0);
    assert(hpclang_stdio_route_write(1, 7) == 0 && tones == 1);
    beep();
    assert(tones == 2);
    hpg_set_mode_gray16(1);
    unsigned previous_publish = published, previous_flips = flips;
    assert(hpclang_stdio_route_write(1, 'A') == 0);
    assert(published == previous_publish && flips == previous_flips);
    assert(hpclang_surface_levels(hpclang_display_surface()) == 16);
    hpg_flip();
    assert(flips == previous_flips + 1);
    hpg_cleanup();
    edited_lines();
    hpclang_surface_destroy(display);
    display = saved_display;
    display_height = 6;
}

static void copies(void)
{
    hpg_t *g = hpg_alloc_gray16_image(8, 6);
    assert(g);
    unsigned char before[48];
    for (int mode = 0; mode < 2; ++mode)
        for (int direction = 0; direction < 4; ++direction) {
            hpg_set_mode(g, HPG_MODE_PAINT);
            for (int y = 0; y < 6; ++y)
                for (int x = 0; x < 8; ++x) {
                    unsigned char value = (unsigned char)(((y * 8 + x) % 16) * 17);
                    before[y * 8 + x] = value;
                    hpg_set_color(g, value);
                    hpg_draw_pixel_on(g, x, y);
                }
            int sx = direction & 1 ? 2 : 0, dx = 2 - sx;
            int sy = direction & 2 ? 2 : 0, dy = 2 - sy;
            hpg_set_color(g, 51);
            hpg_set_mode(g, (unsigned char)mode);
            hpg_blit(g, sx, sy, 6, 4, g, dx, dy);
            assert(hpg_get_color(g) == 51);
            for (int y = 0; y < 6; ++y)
                for (int x = 0; x < 8; ++x) {
                    unsigned char expected = before[y * 8 + x];
                    if (x >= dx && x < dx + 6 && y >= dy && y < dy + 4) {
                        unsigned char source = before[(y - dy + sy) * 8 + x - dx + sx];
                        expected = mode ? source ^ expected : source;
                    }
                    assert(hpg_get_pixel(g, x, y) == expected);
                }
        }
    hpg_t *mono = hpg_alloc_mono_image(8, 6);
    assert(mono);
    hpg_set_mode(g, HPG_MODE_PAINT);
    hpg_set_color(g, 170);
    hpg_fill_rect_on(g, 0, 0, 7, 5);
    hpg_clip(g, 0, 0, 1, 1); /* Source clip does not restrict reading. */
    hpg_clip(mono, 2, 1, 6, 5);
    hpg_blit(g, 0, 0, INT_MAX, INT_MAX, mono, 0, 0);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            assert(hpg_get_pixel(mono, x, y) == (x >= 2 && x < 6 && y >= 1 && y < 5 ? 255 : 0));
    hpg_blit(g, INT_MIN, 0, INT_MAX, 1, mono, INT_MAX, 0);
    hpg_free_image(mono);
    hpg_free_image(g);
}

static void text(void)
{
    char bits[256 * 3] = {0};
    bits[65 * 3] = 1;
    bits[65 * 3 + 1] = 2;
    bits[65 * 3 + 2] = 4;
    bits[255 * 3] = 4;
    bits[255 * 3 + 1] = 2;
    bits[255 * 3 + 2] = 1;
    hpg_font_t *font = hpg_alloc_font(bits, 256, 3, 4);
    hpg_t *g = hpg_alloc_mono_image(16, 10);
    char zero = 0;
    hpg_pattern_t *pattern = hpg_alloc_pattern(&zero, 1, 1);
    assert(font && g && pattern);
    hpg_set_font(g, font);
    hpg_set_pattern(g, pattern);
    char message[] = "A\n\xff";
    hpg_draw_text_on(g, message, 1, 1);
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 2, 2) == 255);
    assert(hpg_get_pixel(g, 3, 4) == 255 && hpg_get_pixel(g, 1, 6) == 255);
    assert(hpg_get_pixel(g, 5, 4) == 0);
    assert(hpg_get_pattern(g) == pattern && hpg_get_font(g) == font);
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_text_on(g, message, 1, 1);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 16; ++x)
            assert(hpg_get_pixel(g, x, y) == 0);
    hpg_draw_letter_on(g, 'A', -1, -1);
    assert(hpg_get_pixel(g, 0, 0) == 255 && hpg_get_pixel(g, 1, 1) == 255);
    hpg_draw_text_on(g, message, INT_MAX, INT_MAX);
    bits[0] = 1;
    hpg_draw_letter_on(g, 0, 5, 0);
    assert(hpg_get_pixel(g, 5, 0) == 255); /* Borrowed data, including direct NUL. */
    hpg_set_font(g, NULL);
    hpg_free_font(font);
    char small_bits[] = {1, 1};
    font = hpg_alloc_font(small_bits, 2, 1, 12);
    assert(font);
    hpg_set_font(g, font);
    hpg_clear_on(g);
    char outside[] = {3, 1, 0};
    hpg_draw_text_on(g, outside, 1, 2);
    assert(hpg_get_pixel(g, 1, 2) == 255 && hpg_get_pixel(g, 9, 2) == 255);
    assert(hpg_get_pixel(g, 13, 2) == 0); /* Out-of-font byte did not advance. */
    hpg_set_font(g, NULL);
    hpg_set_pattern(g, NULL);
    hpg_free_font(font);
    hpg_free_pattern(pattern);
    hpg_free_image(g);
}

static unsigned window_frame;
static int window_paint(void *event, void *context)
{
    assert(!event && !context);
    ++window_frame;
    hpg_set_color(NULL, 255);
    hpg_draw_pixel((int)window_frame, 2);
    return 0;
}
static void window_frames(void)
{
    for (unsigned levels = 2; levels <= 16; levels *= levels == 2 ? 2 : 4)
        for (unsigned buffered = 0; buffered < 2; ++buffered) {
            hpg_cleanup();
            if (levels == 2)
                hpg_set_mode_mono((int)buffered);
            else if (levels == 4)
                hpg_set_mode_gray4((int)buffered);
            else
                hpg_set_mode_gray16((int)buffered);
            struct hpclang_win_events events;
            struct hpclang_win_handler handlers[2];
            struct hpclang_win_call snapshots[2];
            struct hpclang_win_loop loop;
            struct hpclang_win_repaint repaint;
            assert(!hpclang_win_events_init(&events, handlers, 2, snapshots, 2));
            assert(!hpclang_win_loop_init(&loop, &events, 0));
            assert(!hpclang_win_repaint_init(&repaint, &loop, 1, hpclang_hpg_frame_begin,
                                             hpclang_hpg_frame_present, NULL));
            assert(!hpclang_win_events_add(&events, 0, 0, hpclang_win_repaint_idle, &repaint));
            assert(!hpclang_win_events_add(&events, 1, 0, window_paint, NULL));
            window_frame = 0;
            unsigned before_flips = flips, before_publish = published;
            for (unsigned frame = 1; frame <= 2; ++frame) {
                assert(!hpclang_win_repaint_request(&repaint));
                assert(hpclang_win_loop_run(&loop, 1) == 1);
                struct hpclang_surface *shown = pages[buffered ? back ^ 1 : back];
                assert(hpclang_surface_levels(shown) == levels);
                assert(hpclang_surface_get_pixel(shown, (int)frame, 2) == 255);
                assert(hpclang_surface_get_pixel(shown, frame == 1 ? 2 : 1, 2) == 0);
                assert(page_count == 1 + buffered);
            }
            if (buffered) {
                assert(flips == before_flips + 2 && published == before_publish);
                present_failure = 1;
                assert(!hpclang_win_repaint_request(&repaint));
                assert(hpclang_win_loop_run(&loop, 1) == -1 && errno == ETIMEDOUT);
                assert(flips == before_flips + 2);
                assert(hpclang_surface_get_pixel(pages[back ^ 1], 2, 2) == 255);
                assert(hpclang_surface_get_pixel(pages[back ^ 1], 3, 2) == 0);
                present_failure = 0;
            } else {
                assert(flips == before_flips && published >= before_publish + 2);
            }
            unsigned before_end = ended;
            hpg_cleanup();
            assert(!page_count && ended == before_end + 1);
        }
    /* A foreign clean display lease must not be adopted by window rendering. */
    assert(!hpclang_display_begin_pages(2, 1));
    assert(hpclang_hpg_frame_begin(NULL) == -1 && errno == EBUSY);
    hpg_cleanup();
    assert(page_count == 1);
    hpclang_display_end();
    assert(hpclang_hpg_frame_present(NULL) == -1 && errno == EPERM);
}

static void widget_noop(win_widget_t *widget)
{
    (void)widget;
}
static void widget_size(win_widget_t *widget, int *width, int *height)
{
    (void)widget;
    *width = 4;
    *height = 3;
}
static void widget_pixels(void)
{
    assert(!hpclang_win_reset());
    hpg_cleanup();
    hpg_set_mode_gray16(1);
    hpg_clip_reset(NULL);
    hpg_set_color(NULL, 51);
    hpg_fill_rect(0, 0, 7, 5);
    win_widget_t *widget =
        win_wcore_alloc(widget_noop, widget_noop, widget_noop, widget_size, 0, NULL);
    assert(widget);
    win_widget_set_location(widget, 2, 1);
    win_widget_set_colors(widget, 255, 170);
    char bits[] = {0x55};
    hpg_pattern_t *pattern = hpg_alloc_pattern(bits, 1, 1);
    assert(pattern);
    hpg_set_pattern(NULL, pattern);
    hpg_set_mode(NULL, HPG_MODE_XOR);
    win_wcore_paint_base(widget);
    assert(!hpclang_win_error());
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            assert(hpg_get_pixel(NULL, x, y) ==
                   (x >= 2 && x < 6 && y >= 1 && y < 4 && !(x & 1) ? 153 : 51));
    assert(hpg_get_color(NULL) == 255 && hpg_get_mode(NULL) == HPG_MODE_PAINT);
    assert(!hpg_get_pattern(NULL) && hpg_get_font(NULL) == hpg_get_minifont());
    win_widget_set_transparent(widget, 1);
    win_wcore_paint_base(widget);
    assert(hpg_get_pixel(NULL, 2, 1) == 153);
    hpg_fill_rect(0, 0, 7, 5);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            assert(hpg_get_pixel(NULL, x, y) == (x >= 2 && x < 6 && y >= 1 && y < 4 ? 255 : 51));
    win_widget_destroy(widget);
    hpg_free_pattern(pattern);
    assert(!hpclang_win_reset());
    hpg_cleanup();
}

static int progress_changes, remove_on_change;
static int progress_changed(void *event, void *data)
{
    assert(event == data);
    ++progress_changes;
    if (remove_on_change)
        win_remove_widget(event);
    return 0;
}
static void progress_widgets(void)
{
    assert(!hpclang_win_reset());
    hpg_set_mode_gray16(1);
    int changed = win_register_event();
    win_widget_t *bar = win_progress_new(1);
    assert(bar && win_progress_get_value(bar) == 0);
    win_progress_set_event(bar, changed);
    win_add_event_handler(changed, 0, progress_changed, bar);
    win_widget_set_location(bar, 1, 1);
    win_widget_set_size(bar, 6, 4);
    win_progress_set_value(bar, 50);
    win_add_widget(bar);
    for (int direction = 0; direction < 4; ++direction) {
        win_progress_set_orient(bar, direction);
        hpg_clip_reset(NULL);
        hpg_set_color(NULL, 0);
        hpg_fill_rect(0, 0, 7, 5);
        assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 8; ++x) {
                int inside = x >= 1 && x <= 6 && y >= 1 && y <= 4;
                int border = x == 1 || x == 6 || y == 1 || y == 4;
                int filled = direction == 0   ? x <= 3
                             : direction == 1 ? y >= 3
                             : direction == 2 ? x >= 4
                                              : y <= 2;
                assert(hpg_get_pixel(NULL, x, y) == (inside && (border || filled) ? 255 : 0));
            }
    }
    const int increment[] = {KB_RT, KB_UP, KB_LF, KB_DN};
    const int decrement[] = {KB_LF, KB_DN, KB_RT, KB_UP};
    for (int direction = 0; direction < 4; ++direction) {
        win_progress_set_orient(bar, direction);
        win_progress_set_value(bar, 50);
        assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)increment[direction]) == 1);
        assert(win_progress_get_value(bar) == 60);
        assert(!win_post_event(WIN_KEY_RELEASED, (void *)(uintptr_t)increment[direction]));
        assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)decrement[direction]) == 1);
        assert(win_progress_get_value(bar) == 50);
        assert(!win_post_event(WIN_KEY_RELEASED, (void *)(uintptr_t)decrement[direction]));
    }
    assert(progress_changes == 8);
    win_progress_set_orient(bar, 0);
    win_progress_set_value(bar, 0);
    win_progress_set_delay(bar, 10, 5);
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_RT) == 1);
    window_time = 10000;
    assert(hpclang_win_step(1) == 1 && win_progress_get_value(bar) == 20);
    window_time = 15000;
    assert(hpclang_win_step(1) == 1 && win_progress_get_value(bar) == 30);
    assert(!win_post_event(WIN_KEY_RELEASED, (void *)(uintptr_t)KB_RT));
    window_time = 30000;
    assert(hpclang_win_step(1) == 1 && win_progress_get_value(bar) == 30);
    window_time = 500000;
    assert(hpclang_win_step(2) == 1);
    assert(hpclang_surface_get_pixel(pages[back ^ 1], 4, 2) == 255 &&
           hpclang_surface_get_pixel(pages[back ^ 1], 5, 2) == 0);
    win_widget_t *other = win_progress_new(1);
    assert(other);
    win_progress_set_event(other, -1);
    win_add_widget(other);
    win_progress_set_value(bar, 100);
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_RT) == 1);
    assert(win_widget_has_focus(other));
    win_remove_widget(other);
    win_widget_destroy(other);
    win_progress_set_range(bar, -20, 20);
    assert(win_progress_get_value(bar) == 20);
    win_progress_set_range(bar, 5, 5);
    win_progress_set_value(bar, -100);
    assert(win_progress_get_value(bar) == -20);
    win_progress_set_value(bar, 0);
    remove_on_change = 1;
    int before = progress_changes;
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_RT) == 1);
    win_widget_destroy(bar);
    win_remove_event_handler(changed, progress_changed, bar);
    window_time = 1000000;
    assert(hpclang_win_step(1) == 1 && progress_changes == before + 1);
    assert(!hpclang_win_error() && !hpclang_win_reset());
    bar = win_progress_new(0);
    assert(bar);
    win_progress_set_inc(bar, 0);
    assert(hpclang_win_error() == EINVAL);
    win_widget_destroy(bar);
    assert(!hpclang_win_reset());
    bar = win_progress_new(0);
    assert(bar);
    int old_width, old_height, new_width, new_height;
    win_widget_get_size(bar, &old_width, &old_height);
    win_progress_set_inc(bar, DBL_MIN);
    win_widget_pack(bar);
    assert(hpclang_win_error() == EOVERFLOW);
    win_widget_get_size(bar, &new_width, &new_height);
    assert(new_width == old_width && new_height == old_height);
    win_widget_destroy(bar);
    assert(!hpclang_win_reset());
    bar = win_progress_new(0);
    assert(bar);
    win_progress_set_value(bar, NAN);
    assert(hpclang_win_error() == EINVAL && win_progress_get_value(bar) == 0);
    win_widget_destroy(bar);
    assert(!hpclang_win_reset());
    bar = win_wcore_alloc(widget_noop, widget_noop, widget_noop, widget_size, 0, NULL);
    assert(bar);
    win_progress_set_value(bar, 50);
    assert(hpclang_win_error() == EINVAL);
    win_widget_destroy(bar);
    assert(!hpclang_win_reset());
    hpg_cleanup();
}

static void text_widget_pixels(void)
{
    assert(!hpclang_win_reset());
    window_time = 0;
    hpg_set_mode_gray16(1);
    char bits[256 * 3] = {0};
    const unsigned char a[] = {1, 2, 4}, b[] = {7, 5, 7};
    for (unsigned row = 0; row < 3; ++row) {
        bits['A' * 3 + row] = (char)a[row];
        bits['B' * 3 + row] = (char)b[row];
        bits[28 * 3 + row] = 7;
    }
    hpg_font_t *font = hpg_alloc_font(bits, 256, 3, 3);
    win_widget_t *entry = win_text_new_entry(2);
    assert(font && entry);
    win_widget_set_font(entry, font);
    win_widget_set_size(entry, 8, 6);
    win_text_set_text(entry, "AB");
    win_add_widget(entry);
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x) {
            int ink = 0;
            if (y >= 1 && y <= 3) {
                if (x >= 1 && x <= 3)
                    ink = a[y - 1] & (1u << (x - 1));
                if (x >= 4 && x <= 6)
                    ink = b[y - 1] & (1u << (x - 4));
            }
            assert(hpg_get_pixel(NULL, x, y) == (ink ? 255 : 0));
        }
    win_keytyped_t key = {KB_RT, 0, 0, 0, 0};
    assert(win_post_event(WIN_KEY_TYPED, &key) == 1);
    window_time = 500000;
    assert(hpclang_win_step(1) == 1);
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x) {
            int ink = 0;
            if (y >= 1 && y <= 3) {
                if (x >= 1 && x <= 3)
                    ink = a[y - 1] & (1u << (x - 1));
                if (x >= 4 && x <= 6)
                    ink = 1;
            }
            assert(hpg_get_pixel(NULL, x, y) == (ink ? 255 : 0));
        }
    assert(!strcmp(win_text_get_text(entry), "AB"));
    key.keycode = KB_BKS;
    assert(win_post_event(WIN_KEY_TYPED, &key) == 1);
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    assert(!strcmp(win_text_get_text(entry), "B"));
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            assert(hpg_get_pixel(NULL, x, y) == (x >= 1 && x <= 3 && y >= 1 && y <= 3 ? 255 : 0));
    win_remove_widget(entry);
    win_widget_destroy(entry);
    hpg_free_font(font);
    assert(!hpclang_win_error() && !hpclang_win_reset());
    hpg_cleanup();
}

static void image_widget_pixels(void)
{
    assert(!hpclang_win_reset());
    hpg_set_mode_gray16(1);
    hpg_t *image = hpg_alloc_gray16_image(2, 2);
    char bits[] = {0x55, (char)0xaa};
    hpg_pattern_t *pattern = hpg_alloc_pattern(bits, 2, 0);
    assert(image && pattern);
    hpg_set_color(image, 119);
    hpg_draw_pixel_on(image, 0, 0);
    hpg_set_color(image, 255);
    hpg_draw_pixel_on(image, 1, 1);
    win_widget_t *widget = win_img_new(pattern, image);
    assert(widget);
    int w, h;
    win_widget_get_size(widget, &w, &h);
    assert(w == 2 && h == 2);
    win_widget_set_location(widget, 1, 1);
    win_widget_set_size(widget, 5, 4);
    win_add_widget(widget);
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x) {
            int expected = 0;
            if (x >= 1 && x < 6 && y >= 1 && y < 5)
                expected = ((unsigned char)bits[(y - 1) % 2] & (1u << ((x - 1) % 8))) ? 255 : 0;
            if (x >= 1 && x < 3 && y >= 1 && y < 3)
                expected = x == 1 && y == 1 ? 119 : x == 2 && y == 2 ? 255 : 0;
            assert(hpg_get_pixel(NULL, x, y) == expected);
        }
    assert(hpg_get_pattern(NULL) == NULL);
    win_remove_widget(widget);
    hpg_clip_reset(NULL);
    hpg_clear();
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    assert(hpg_get_pixel(NULL, 1, 1) == 0);
    win_widget_destroy(widget);
    /* Destruction must preserve borrowed sources for reuse by another widget. */
    assert(hpg_get_pixel(image, 0, 0) == 119);
    widget = win_img_new(pattern, NULL);
    win_widget_get_size(widget, &w, &h);
    assert(w == 0 && h == 0);
    win_widget_set_size(widget, 3, 2);
    win_add_widget(widget);
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    assert(hpg_get_pixel(NULL, 0, 0) == 255 && hpg_get_pixel(NULL, 1, 0) == 0);
    win_remove_widget(widget);
    win_widget_destroy(widget);
    widget = win_img_new(NULL, NULL);
    assert(widget);
    win_widget_destroy(widget);
    hpg_free_pattern(pattern);
    hpg_free_image(image);
    assert(!hpclang_win_error() && !hpclang_win_reset());
    hpg_cleanup();
}

static void menu_widget_pixels(void)
{
    struct hpclang_surface *saved = display;
    display_width = 131;
    for (display_height = 64; display_height <= 80; display_height += 16) {
        display = hpclang_surface_create_mono(132, display_height);
        assert(display);
        display->width = 131;
        for (int buffered = 0; buffered < 2; ++buffered) {
            assert(!hpclang_win_reset());
            hpg_set_mode_gray16(buffered);
            char glyphs[256 * 6] = {0};
            for (unsigned row = 0; row < 4; ++row)
                glyphs['A' * 6 + row] = (char)(1u << row);
            hpg_font_t *font = hpg_alloc_font(glyphs, 256, 6, 4);
            win_widget_t *menu = win_softmenu_new();
            assert(menu && font);
            win_widget_set_font(menu, font);
            for (int i = 0; i < 7; ++i)
                win_softmenu_set_item(menu, i, "A", -1, NULL);
            win_softmenu_set_disable_colors(menu, 1, 51, 102);
            win_softmenu_set_enabled(menu, 1, 0);
            win_softmenu_set_colors(menu, 6, 17, 238);
            win_add_widget(menu);
            for (int page = 0; page < 2; ++page) {
                hpg_clip_reset(NULL);
                hpg_clear();
                assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
                for (int y = 0; y < (int)display_height; ++y)
                    for (int x = 0; x < 131; ++x) {
                        int cell = x / 22, local = x % 22;
                        int row = y - ((int)display_height - 6);
                        int expected = 0;
                        if (y >= (int)display_height - 7 && local <= 20 && (!page || cell == 0)) {
                            expected = page ? 238 : cell == 1 ? 102 : 255;
                            if (row >= 0 && row < 4 && local == 9 + row)
                                expected = page ? 17 : cell == 1 ? 51 : 0;
                        }
                        assert(hpg_get_pixel(NULL, x, y) == expected);
                    }
                win_keytyped_t key = {KB_L, 0, 0, 0, 0};
                assert(win_post_event(WIN_KEY_TYPED, &key) == 1);
            }
            win_remove_widget(menu);
            win_widget_destroy(menu);
            hpg_free_font(font);
            assert(!hpclang_win_error() && !hpclang_win_reset());
            hpg_cleanup();
        }
        hpclang_surface_destroy(display);
    }
    display = saved;
    display_width = 8;
    display_height = 6;
}

static void checkbox_widget_pixels(void)
{
    struct hpclang_surface *saved = display;
    display_width = 32;
    display_height = 16;
    display = hpclang_surface_create_mono(33, 16);
    assert(display);
    display->width = 32;
    const unsigned char checked[] = {0, 0x20, 0x30, 0x1a, 0x0e, 4, 0x3e, 0};
    const unsigned char empty[] = {0, 0, 0, 0, 0, 0, 0x3e, 0};
    for (int buffered = 0; buffered < 2; ++buffered) {
        assert(!hpclang_win_reset());
        hpg_set_mode_gray16(buffered);
        char glyphs[256 * 3] = {0};
        glyphs['A' * 3] = 1;
        glyphs['A' * 3 + 1] = 2;
        glyphs['A' * 3 + 2] = 4;
        hpg_font_t *font = hpg_alloc_font(glyphs, 256, 3, 3);
        win_widget_t *boxes[] = {win_checkbox_new("A"), win_checkbox_new("")};
        assert(font && boxes[0] && boxes[1]);
        for (unsigned i = 0; i < 2; ++i) {
            win_widget_set_font(boxes[i], font);
            win_widget_set_location(boxes[i], i ? 20 : 3, 2);
            win_widget_set_size(boxes[i], 12, 9);
            win_add_widget(boxes[i]);
        }
        for (int focus = 0; focus < 2; ++focus) {
            assert(win_widget_has_focus(boxes[focus]));
            for (int selected = 0; selected < 2; ++selected) {
                win_checkbox_set_selected(boxes[0], selected);
                hpg_clip_reset(NULL);
                hpg_clear();
                assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 32; ++x) {
                        int expected = 0;
                        for (int i = 0; i < 2; ++i) {
                            int local_x = x - (i ? 20 : 3), row = y - 2;
                            if (local_x < 0 || local_x >= 12 || row < 0 || row >= 9)
                                continue;
                            if (focus == i && local_x <= 6)
                                expected = 255;
                            const unsigned char *pattern = !i && selected ? checked : empty;
                            if (local_x <= 8 && (pattern[row % 8] & (1u << (local_x % 8))))
                                expected = focus == i ? 0 : 255;
                            if (!i && row >= 1 && row <= 3 && local_x == 8 + row - 1)
                                expected = 255;
                        }
                        assert(hpg_get_pixel(NULL, x, y) == expected);
                    }
            }
            win_focus_next();
        }
        for (unsigned i = 2; i; --i) {
            win_remove_widget(boxes[i - 1]);
            win_widget_destroy(boxes[i - 1]);
        }
        hpg_free_font(font);
        assert(!hpclang_win_error() && !hpclang_win_reset());
        hpg_cleanup();
    }
    hpclang_surface_destroy(display);
    display = saved;
    display_width = 8;
    display_height = 6;
}

static void choose_widget_pixels(void)
{
    struct hpclang_surface *saved = display;
    display_width = 131;
    for (display_height = 64; display_height <= 80; display_height += 16) {
        display = hpclang_surface_create_mono(132, display_height);
        assert(display);
        display->width = 131;
        for (int buffered = 0; buffered < 2; ++buffered) {
            assert(!hpclang_win_reset());
            hpg_set_mode_gray16(buffered);
            char glyphs[256 * 3] = {0};
            glyphs['A' * 3] = 1;
            glyphs['A' * 3 + 1] = 2;
            glyphs['A' * 3 + 2] = 4;
            glyphs['B' * 3] = 4;
            glyphs['B' * 3 + 1] = 2;
            glyphs['B' * 3 + 2] = 1;
            hpg_font_t *font = hpg_alloc_font(glyphs, 256, 3, 3);
            win_widget_t *menu = win_softmenu_new(), *chooser = win_choose_new();
            assert(font && menu && chooser);
            win_widget_set_font(chooser, font);
            win_widget_set_location(chooser, 1, 1);
            win_widget_set_size(chooser, 5, 5);
            win_choose_set_menu(chooser, menu, 0);
            for (int i = 0; i < 40; ++i)
                win_choose_add_with_label(chooser, i % 2 ? "B" : "A", "A");
            win_add_widget(menu);
            win_add_widget(chooser);
            for (int selected = 0; selected < 40; selected += 39) {
                win_choose_set_index(chooser, selected);
                win_keytyped_t key = {KB_A, 0, 0, 0, 0};
                assert(win_post_event(WIN_KEY_TYPED, &key) == 1);
                hpg_clip_reset(NULL);
                hpg_clear();
                assert(win_post_event(WIN_REPAINT_EVENT, NULL) == 1);
                /* Fixed expected panel geometry: 13 rows at y=0 on 64 lines,
                 * 17 rows at y=0 on 80 lines. Last selection scrolls to bottom. */
                int rows = display_height == 64 ? 13 : 17;
                int first = selected ? 40 - rows : 0;
                int bottom = rows * 4 + 3;
                for (int y = 0; y <= bottom + 1; ++y)
                    for (int x = 15; x <= 115; ++x) {
                        int expected = 0;
                        if (x <= 114 && y <= bottom &&
                            (y == 0 || y == bottom || x == 15 || x == 114))
                            expected = 255;
                        int row = (y - 2) / 4;
                        int local_y = (y - 2) % 4;
                        if (y >= 2 && row < rows && x >= 17 && x <= 112) {
                            int chosen = first + row == selected;
                            expected = chosen ? 255 : 0;
                            int glyph_x = (first + row) % 2 ? 2 - (local_y - 1) : local_y - 1;
                            if (local_y >= 1 && x == 18 + glyph_x)
                                expected = chosen ? 0 : 255;
                        }
                        if (hpg_get_pixel(NULL, x, y) != expected)
                            fprintf(
                                stderr,
                                "choose pixel %d,%d height=%u selected=%d actual=%d expected=%d\n",
                                x, y, display_height, selected, hpg_get_pixel(NULL, x, y),
                                expected);
                        assert(hpg_get_pixel(NULL, x, y) == expected);
                    }
                /* Painting the modal must preserve the ordinary label outside it. */
                assert(hpg_get_pixel(NULL, 2, 2) == 0);
                assert(hpg_get_pixel(NULL, 3, 2) == 255);
                assert(hpg_get_pixel(NULL, 0, display_height - 7) == 0);
                assert(hpg_get_pixel(NULL, 88, display_height - 7) == 255);
                assert(hpg_get_pixel(NULL, 110, display_height - 7) == 255);
                key.keycode = KB_E;
                assert(win_post_event(WIN_KEY_TYPED, &key) == 1);
                assert(win_choose_get_index(chooser) == selected);
            }
            win_remove_widget(chooser);
            win_remove_widget(menu);
            win_widget_destroy(chooser);
            win_widget_destroy(menu);
            hpg_free_font(font);
            assert(!hpclang_win_error() && !hpclang_win_reset());
            hpg_cleanup();
        }
        hpclang_surface_destroy(display);
    }
    display = saved;
    display_width = 8;
    display_height = 6;
}

int main(void)
{
    hpg_t *g = hpg_alloc_gray4_image(8, 6);
    assert(g && borrowed == 0 && hpg_get_width(g) == 8 && hpg_get_height(g) == 6);
    assert(hpg_get_color(g) == HPG_COLOR_BLACK && hpg_get_mode(g) == HPG_MODE_PAINT);
    assert(hpg_get_pixel(g, 0, 0) == 0);
    hpg_set_color(g, 170);
    hpg_fill_rect_on(g, INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    assert(hpg_get_pixel(g, 7, 5) == 170);
    hpg_clip_set(g, 0, 0, 8, 6);
    hpg_set_color(g, 85);
    hpg_fill_rect_on(g, 0, 0, 7, 5);
    assert(hpg_get_pixel(g, 6, 4) == 85 && hpg_get_pixel(g, 7, 5) == 170);
    hpg_clip_reset(g);
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_pixel_on(g, 7, 5);
    assert(hpg_get_pixel(g, 7, 5) == 255);
    hpg_set_mode(g, 3);
    assert(errno == EINVAL && hpg_get_mode(g) == HPG_MODE_XOR);
    hpg_clear_on(g); /* Whole image, independent of current ink/mode/clip. */
    assert(hpg_get_pixel(g, 7, 5) == 0);
    char pattern[] = {1, 2};
    hpg_pattern_t *p = hpg_alloc_pattern(pattern, 2, 0);
    assert(p);
    hpg_set_pattern(g, p);
    assert(hpg_get_pattern(g) == p);
    hpg_set_mode(g, HPG_MODE_PAINT);
    hpg_set_color(g, 255);
    hpg_fill_rect_on(g, 2, 1, 7, 5);
    assert(hpg_get_pixel(g, 2, 1) == 255 && hpg_get_pixel(g, 3, 2) == 255);
    assert(hpg_get_pixel(g, 3, 1) == 0);
    pattern[0] = 4;
    hpg_clear_on(g);
    hpg_fill_rect_on(g, 2, 1, 7, 5);
    assert(hpg_get_pixel(g, 4, 1) == 255 && hpg_get_pixel(g, 2, 1) == 0);
    hpg_set_pattern(g, NULL);
    hpg_free_pattern(p);
    hpg_clear_on(g);
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_rect_on(g, 1, 1, 6, 4);
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 6, 4) == 255);
    assert(hpg_get_pixel(g, 3, 2) == 0);
    hpg_draw_rect_on(g, 6, 4, 1, 1);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            assert(hpg_get_pixel(g, x, y) == 0);
    hpg_clip(g, 2, 1, 6, 5);
    hpg_draw_line_on(g, INT_MIN, 3, INT_MAX, 3);
    for (int x = 0; x < 8; ++x)
        assert(hpg_get_pixel(g, x, 3) == (x >= 2 && x < 6 ? 255 : 0));
    hpg_clip_reset(g);
    hpg_clear_on(g);
    hpg_fill_circle_on(g, 3, 3, 2);
    assert(hpg_get_pixel(g, 3, 3) == 255 && hpg_get_pixel(g, 1, 3) == 255);
    assert(hpg_get_pixel(g, 0, 0) == 0);
    hpg_fill_circle_on(g, 3, 3, 2);
    assert(hpg_get_pixel(g, 3, 3) == 0 && hpg_get_pixel(g, 1, 3) == 0);
    hpg_draw_circle_on(g, 3, 3, 2);
    assert(hpg_get_pixel(g, 3, 3) == 0 && hpg_get_pixel(g, 1, 3) == 255);
    hpg_draw_circle_on(g, INT_MAX, INT_MAX, INT_MAX);
    hpg_clear_on(g);
    int vx[] = {1, 6, 1}, vy[] = {1, 1, 5};
    hpg_fill_polygon_on(g, vx, vy, 3);
    assert(hpg_get_pixel(g, 2, 2) == 255 && hpg_get_pixel(g, 7, 5) == 0);
    hpg_fill_polygon_on(g, vx, vy, 3);
    assert(hpg_get_pixel(g, 2, 2) == 0 && vx[1] == 6 && vy[2] == 5);
    hpg_draw_polygon_on(g, vx, vy, 3);
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 2, 2) == 0);
    hpg_fill_polygon_on(g, NULL, vy, 3);
    assert(errno == EINVAL);
    char font_bits[] = {1, 2, 4, 1, 2, 4};
    hpg_font_t *f = hpg_alloc_font(font_bits, 2, 3, 4);
    assert(f && hpg_font_get_height(f) == 3 && hpg_font_get_advance(f) == 4);
    hpg_set_font(g, f);
    assert(hpg_get_font(g) == f);
    hpg_set_font(g, NULL);
    hpg_free_font(f);
    assert(!hpg_alloc_font(font_bits, 257, 3, 4) && errno == EINVAL);
    assert(!hpg_alloc_pattern(pattern, 0, 0) && errno == EINVAL);
    assert(!hpg_alloc_mono_image(0, 1) && errno == EINVAL);
    hpg_free_image(g);
    display = hpclang_surface_create_mono(9, 6);
    assert(display);
    display->width = 8;
    hpg_set_color(NULL, 255);
    hpg_draw_pixel(1, 1);
    assert(hpclang_surface_get_pixel(display, 1, 1) == 255);
    hpg_free_image(HPG_STDSCREEN);
    assert(errno == EINVAL && hpg_get_width(NULL) == 8);
    hpg_cleanup();
    assert(hpclang_surface_get_pixel(display, 1, 1) == 255);
    modes();
    indicators();
    shared_screen();
    historical_console();
    window_frames();
    widget_pixels();
    progress_widgets();
    text_widget_pixels();
    image_widget_pixels();
    menu_widget_pixels();
    checkbox_widget_pixels();
    choose_widget_pixels();
    hpclang_surface_destroy(display);
    display = NULL;
    assert(hpg_get_width(NULL) == 0 && errno == ENODEV);
    copies();
    text();
    defaults();
    pixmaps();
    return 0;
}
