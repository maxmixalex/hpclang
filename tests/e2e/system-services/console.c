#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <hpclang/console.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/input.h>
#include <hpclang/stdio.h>

struct reader
{
    struct hpclang_console *console;
    unsigned offset;
    int cancel;
};
static int read_byte(void *context)
{
    struct reader *r = context;
    static const char line[] = "12\n";
    if (r->cancel) {
        errno = ECANCELED;
        return HPCLANG_STDIO_ERROR;
    }
    return r->offset < sizeof(line) - 1 ? line[r->offset++] : HPCLANG_STDIO_EOF;
}
static int write_byte(void *context, unsigned stream, unsigned char byte)
{
    struct reader *r = context;
    return hpclang_console_stdio_write(r->console, stream, byte);
}
static int flush(void *context, unsigned stream)
{
    struct reader *r = context;
    return hpclang_console_stdio_flush(r->console, stream);
}

struct line_session
{
    struct hpclang_console *console;
    unsigned next, rejected, updates;
};
static int line_next(void *context, struct hpclang_keyboard_event *event)
{
    struct line_session *s = context;
    static const unsigned keys[] = {HPCLANG_KEY_MINUS, HPCLANG_KEY_ENTER, HPCLANG_KEY_1,
                                    HPCLANG_KEY_2, HPCLANG_KEY_ENTER};
    if (s->next == sizeof(keys) / sizeof(keys[0])) {
        errno = EIO;
        return -1;
    }
    *event = (struct hpclang_keyboard_event){.type = HPCLANG_KEYBOARD_EVENT_DOWN,
                                             .key = keys[s->next++]};
    return 1;
}
static int line_accept(void *context, const struct hpclang_text_input *input)
{
    (void)context;
    int64_t value;
    return hpclang_text_input_parse_int64(input, &value) == 0 ? 0 : 1;
}
static int line_update(void *context, const struct hpclang_text_input *input, int result, int error)
{
    (void)result;
    struct line_session *s = context;
    ++s->updates;
    s->rejected += error == EINVAL;
    if (hpclang_console_clear(s->console) != 0 ||
        hpclang_console_write(s->console, input->buffer, input->length) != 0)
        return -1;
    return hpclang_console_render(s->console, hpclang_display_surface(), 2, 2);
}

unsigned hpclang_e2e_console_test(void)
{
    const struct hpclang_font *font = hpclang_font_mini();
    unsigned w = hpclang_font_advance(font), h = hpclang_font_height(font);
    struct hpclang_console *c = hpclang_console_create(8, 2, font);
    struct hpclang_surface *expected = hpclang_surface_create_mono(8 * w, 2 * h);
    struct reader reader = {c, 0, 0};
    struct hpclang_stdio_callbacks callbacks = {write_byte, read_byte, flush, &reader};
    unsigned result = 1;
    if (!c || !expected)
        goto done;
    if (getchar() != EOF || !feof(stdin) || ferror(stdin))
        goto done;
    result = 2;
    if (hpclang_stdio_bind(&callbacks) != 0 || feof(stdin) || ferror(stdin) ||
        hpclang_stdio_bind(&callbacks) != -1 || errno != EBUSY)
        goto done;
    result = 3;
    if (printf("%s\n", "AB") != 3 || fprintf(stderr, "%d", 12) != 2 || fflush(stdout) != 0 ||
        fflush(stderr) != 0)
        goto done;
    uint32_t cp;
    if (hpclang_console_cell(c, 0, 0, &cp) != 0 || cp != 'A' ||
        hpclang_console_cell(c, 1, 1, &cp) != 0 || cp != '2')
        goto done;
    result = 4;
    hpclang_surface_draw_codepoint(expected, font, 0, 0, 'A', 255);
    hpclang_surface_draw_codepoint(expected, font, (int32_t)w, 0, 'B', 255);
    hpclang_surface_draw_codepoint(expected, font, 0, (int32_t)h, '1', 255);
    hpclang_surface_draw_codepoint(expected, font, (int32_t)w, (int32_t)h, '2', 255);
    struct hpclang_surface *screen = hpclang_display_surface();
    if (hpclang_console_render(c, screen, 2, 2) != 0)
        goto done;
    for (unsigned y = 0; y < 2 * h; ++y)
        for (unsigned x = 0; x < 8 * w; ++x)
            if (hpclang_surface_get_pixel(screen, (int32_t)x + 2, (int32_t)y + 2) !=
                hpclang_surface_get_pixel(expected, (int32_t)x, (int32_t)y))
                goto done;
    result = 5;
    char line[8], edit[8];
    struct hpclang_text_input input;
    int64_t value;
    if (fgets(line, sizeof(line), stdin) != line || strcmp(line, "12\n") != 0 ||
        hpclang_text_input_init(&input, edit, sizeof(edit)) != 0)
        goto done;
    for (unsigned i = 0; line[i] != '\n'; ++i)
        if (hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, (unsigned char)line[i]) != 0)
            goto done;
    if (hpclang_text_input_parse_int64(&input, &value) != 0 || value != 12 || getchar() != EOF ||
        !feof(stdin) || ferror(stdin))
        goto done;
    result = 6;
    clearerr(stdin);
    reader.cancel = 1;
    if (getchar() != EOF || !ferror(stdin) || feof(stdin) || errno != ECANCELED ||
        fputs("!", stdout) == EOF || ferror(stdout))
        goto done;
    result = 7;
    clearerr(stdin);
    if (ungetc('X', stdin) != 'X' || hpclang_stdio_unbind() != 0 || getchar() != EOF ||
        !feof(stdin) || ferror(stdin))
        goto done;
    reader.offset = 0;
    reader.cancel = 0;
    if (hpclang_stdio_bind(&callbacks) != 0 || getchar() != '1')
        goto done;
    result = 8;
    struct line_session session = {.console = c};
    struct hpclang_text_input_callbacks line_callbacks = {line_next, NULL, line_accept, line_update,
                                                          &session};
    if (hpclang_text_input_init(&input, edit, sizeof(edit)) != 0 ||
        hpclang_text_input_run(&input, &line_callbacks) != HPCLANG_INPUT_ACCEPTED ||
        strcmp(edit, "-12") != 0 || session.rejected != 1 || session.updates != 6 ||
        hpclang_console_cell(c, 2, 0, &cp) != 0 || cp != '2')
        goto done;
    result = 9;
    uint32_t model = hpclang_device_info()->model;
    int hp39 = hpclang_device_info()->family == HPCLANG_FAMILY_HP39;
    uint16_t a = hp39 ? HPCLANG_KEY_HP39_VARS : HPCLANG_KEY_F1;
    uint16_t z = hp39 ? HPCLANG_KEY_3 : HPCLANG_KEY_DIVIDE;
    if (hpclang_text_alpha_lookup(model, a, 0, &cp) != 1 || cp != 'A' ||
        hpclang_text_alpha_lookup(model, z, 1, &cp) != 1 || cp != 'z' ||
        hpclang_text_alpha_lookup(model, HPCLANG_KEY_UNKNOWN, 0, &cp) != 0 || cp != 'z')
        goto done;
    result = 0;
done:
    hpclang_stdio_unbind();
    hpclang_surface_destroy(expected);
    hpclang_console_destroy(c);
    return result;
}
