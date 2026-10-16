#include <hpgraphics.h>
#include <hpclang/display.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <hpclang/font-package.h>
#include "display-surface.h"
#include "hpg-internal.h"
#define HPCLANG_HPCONIO_NO_AUTOINIT
#include <hpconio.h>
#include <hpclang/sound.h>
#include <hpclang/stdio.h>
#include "stdio-routing.h"

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
int hpg_test_getchar(void)
{
    int result = hpclang_stdio_route_read();
    if (result == HPCLANG_STDIO_ERROR) {
        stream_error = 1;
        return EOF;
    }
    return result;
}
int hpg_test_putchar(int byte)
{
    return hpclang_stdio_route_write(1, (unsigned char)byte) ? EOF : (unsigned char)byte;
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
static int present_failure;
static unsigned display_height = 6;
static int configure(unsigned levels, unsigned count)
{
    if (mode_failure) {
        errno = ENOMEM;
        return -1;
    }
    for (unsigned i = 0; i < 2; ++i) {
        hpclang_surface_destroy(pages[i]);
        pages[i] = i < count ? hpclang_surface_create(9, display_height, levels) : NULL;
        assert(i >= count || pages[i]);
        if (pages[i])
            pages[i]->width = 8;
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
    ++published;
    return 0;
}
int hpclang_display_indicator_set(unsigned segment, uint8_t color)
{
    assert(segment < 6 && page_count);
    struct hpclang_surface view = *pages[back];
    ++view.width;
    hpclang_surface_set_pixel(&view, 8, (int)segment, color);
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
static void edited_lines(void)
{
    char text[16];
    hpg_set_mode_mono(0);
    _clear_screen();
    struct hpclang_surface *s = hpclang_display_surface();
    hpclang_surface_set_pixel(s, 7, 5, 255);
    input_bytes = (const unsigned char *)"ABCDEFG\b\b\b\b\b\b\n";
    assert(hpclang_hpgcc_gets(text, sizeof text) == text && !strcmp(text, "A"));
    assert(hpclang_surface_get_pixel(s, 0, 0) == 255);
    assert(hpclang_surface_get_pixel(s, 7, 5) == 255); /* restore pre-scroll graphics */
    assert(getx() == 0 && gety() == 1);
    hpg_set_mode_gray16(1);
    _clear_screen();
    input_bytes = (const unsigned char *)"ABC\b\n";
    unsigned previous = flips;
    assert(hpclang_hpgcc_gets(text, sizeof text) == text && !strcmp(text, "AB"));
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
    hpclang_surface_destroy(display);
    display = NULL;
    assert(hpg_get_width(NULL) == 0 && errno == ENODEV);
    copies();
    text();
    defaults();
    pixmaps();
    return 0;
}
