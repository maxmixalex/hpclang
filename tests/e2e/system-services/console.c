#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <hpkeyb49.h>
#include <hpclang/abi.h>
#include <hpclang/console.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/input.h>
#include <hpclang/stdio.h>
#include <hpclang/time.h>

extern volatile uint32_t hpclang_e2e_result;
extern const struct hpclang_font hpclang_system_font;
unsigned hpclang_historical_system(int (*checkpoint)(uint32_t, uint32_t));

static int native_symbols(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot->model >= HPCLANG_MODEL_HP48GII && !boot->saturn_state)
        return 0; /* Direct classic launchers do not fabricate a live font map. */
    struct hpclang_surface *surface = hpclang_surface_create_mono(24, 12);
    if (!surface)
        return -1;
    const uint32_t unicode[] = {0x03b1, 0x03b2, 0x0394, 0x03b8, 0x03bc, 0x03a0,
                                0x03c0, 0x03a3, 0x2220, 0x2192, 0x221a};
    const uint8_t native[] = {0x8c, 0xdf, 0x9b, 0x95, 0xb5, 0x9c, 0x87, 0x85, 0x80, 0x8d, 0x83};
    const struct hpclang_font *fonts[] = {hpclang_font_mini(), hpclang_font_big(),
                                          hpclang_font_editline()};
    int result = -1;
    for (unsigned f = 0; f < 3; ++f) {
        if (!fonts[f])
            continue;
        unsigned differences = 0;
        for (unsigned i = 0; i < sizeof(native); ++i) {
            hpclang_surface_clear(surface, 0);
            hpclang_surface_draw_codepoint(surface, fonts[f], 0, 0, unicode[i], 255);
            hpclang_surface_draw_char(surface, fonts[f], 8, 0, native[i], 255);
            hpclang_surface_draw_char(surface, fonts[f], 16, 0, '?', 255);
            for (int y = 0; y < 12; ++y)
                for (int x = 0; x < 8; ++x) {
                    uint8_t pixel = hpclang_surface_get_pixel(surface, x, y);
                    if (pixel != hpclang_surface_get_pixel(surface, x + 8, y))
                        goto done;
                    differences += pixel != hpclang_surface_get_pixel(surface, x + 16, y);
                }
        }
        if (!differences)
            goto done;
    }
    result = 0;
done:
    hpclang_surface_destroy(surface);
    return result;
}

static int mailbox(uint32_t request, uint32_t acknowledgement)
{
    hpclang_e2e_result = request;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != acknowledgement)
        if (hpclang_time_monotonic_us() - start > 2000000u) {
            errno = ETIMEDOUT;
            return -1;
        }
    return 0;
}
static int physical_event(unsigned scan, unsigned down, struct hpclang_keyboard_event *event)
{
    unsigned argument = scan | (down << 8);
    if (mailbox(0x4b490000u | argument, 0x4b4a0000u | argument))
        return -1;
    uint64_t start = hpclang_time_monotonic_us();
    while (!hpclang_keyboard_event_poll(event))
        if (hpclang_time_monotonic_us() - start > 2000000u) {
            errno = ETIMEDOUT;
            return -1;
        }
    if (event->scan_code != scan ||
        event->type != (down ? HPCLANG_KEYBOARD_EVENT_DOWN : HPCLANG_KEYBOARD_EVENT_UP)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int physical_symbols(uint32_t model)
{
    int hp39 = model <= HPCLANG_MODEL_HP40GS;
    /* Electrical matrix positions, independent of the SDK's key lookup.
     * Classic RS remains held across 6 to distinguish degree from angle. */
    static const uint8_t hp39_events[][2] = {{58, 1}, {58, 0}, {27, 1}, {27, 0}, {59, 1}, {59, 0},
                                             {4, 1},  {4, 0},  {59, 1}, {59, 0}, {12, 1}, {12, 0}};
    static const uint8_t classic_events[][2] = {
        {57, 1}, {57, 0}, {59, 1}, {59, 0}, {35, 1}, {35, 0}, {57, 1}, {57, 0}, {59, 1},
        {11, 1}, {11, 0}, {59, 0}, {58, 1}, {58, 0}, {3, 1},  {3, 0},  {19, 1}, {19, 0}};
    const uint8_t (*events)[2] = hp39 ? hp39_events : classic_events;
    unsigned count = hp39 ? sizeof(hp39_events) / sizeof(hp39_events[0])
                          : sizeof(classic_events) / sizeof(classic_events[0]);
    char text[24];
    struct hpclang_text_input input;
    struct hpclang_text_mode mode;
    if (hpclang_text_input_init(&input, text, sizeof(text)) || hpclang_text_mode_init(&mode, model))
        return -1;
    for (unsigned i = 0; i < count; ++i) {
        struct hpclang_keyboard_event event;
        if (physical_event(events[i][0], events[i][1], &event) ||
            hpclang_text_input_event(&input, &mode, &event))
            return -1;
    }
    const char *expected = hp39 ? "\xce\xb8\xe2\x88\xa0\xcf\x80" : "\xce\xb1\xc2\xb0[2]";
    struct hpclang_keyboard_state keys;
    hpclang_keyboard_read(&keys);
    return strcmp(text, expected) || hpclang_keyboard_any_is_down(&keys) ? -1 : 0;
}

static int historical_keyboard_adapter(uint32_t model)
{
    unsigned alpha = model <= HPCLANG_MODEL_HP40GS ? 58 : 57;
    unsigned left_shift = model <= HPCLANG_MODEL_HP40GS ? 59 : 58;
    for (unsigned scan = 0; scan < 60; ++scan) {
        struct hpclang_keyboard_event event;
        if (physical_event(scan, 1, &event))
            return -1;
        keymatrix matrix;
        keyb_getmatrix(&matrix);
        unsigned bit = scan < 56 ? (scan / 7) * 8 + scan % 7 + 1 : (scan == 56 ? 63 : scan + 3);
        if (matrix.full != (UINT64_C(1) << bit) || !keyb_isKeyPrM((int)bit) ||
            !keyb_isAnyKeyPressed() || keyb_isON() != (scan == 56) ||
            keyb_isAlpha() != (scan == alpha) || keyb_isLS() != (scan == left_shift) ||
            keyb_isRS() != (model > HPCLANG_MODEL_HP40GS && scan == 59) ||
            keyb_isUp() != (scan == 42) || keyb_isLeft() != (scan == 43) ||
            keyb_isDown() != (scan == 44) || keyb_isRight() != (scan == 45))
            return -1;
        if (scan < 56 && (!keyb_isKeyPressed(scan / 7, scan % 7) ||
                          keyb_getkey(0) != (int)((scan % 7) * 16 + scan / 7)))
            return -1;
        if (keyb_getkeyM(0) != (int)bit ||
            keyb_getnextkey(&matrix, -1) != (bit < 60 ? (int)bit : 0))
            return -1;
        if (physical_event(scan, 0, &event) || keyb_isAnyKeyPressed() || keyb_getkey(0) != -1 ||
            keyb_getkeyM(0))
            return -1;
    }
    struct hpclang_keyboard_event event;
    if (physical_event(left_shift, 1, &event) || physical_event(35, 1, &event))
        return -1;
    uint32_t plane = model <= HPCLANG_MODEL_HP40GS ? UINT32_C(0x40000000) : UINT32_C(0x20000000);
    if (keyb_getkeyM(0) != (int)(41 | plane) || !keyb_isShiftKeyPrM((int)(41 | plane)) ||
        !keyb_isShiftKeyPrM(KB_A | SHFT_ANY) || keyb_isShiftKeyPrM(KB_A) ||
        keyb_isShiftKeyPrM(KB_B | SHFT_ANY))
        return -1;
    if (physical_event(35, 0, &event) || physical_event(left_shift, 0, &event))
        return -1;
    volatile const uint8_t *rtc_tick = (volatile const uint8_t *)(uintptr_t)0x07b00044u;
    uint8_t saved_tick = *rtc_tick;
    uint64_t start = hpclang_time_monotonic_us();
    if (mailbox(0x4b500123u, 0x4b510123u))
        return -1; /* delayed physical F1 press/release */
    keyb_waitKeyPressed();
    if (keyb_isAnyKeyPressed() || hpclang_time_monotonic_us() - start < 100000u ||
        *rtc_tick != saved_tick)
        return -1;
    if (mailbox(0x4b500123u, 0x4b510123u) || keyb_getkey(1) != 5 || keyb_isAnyKeyPressed() ||
        *rtc_tick != saved_tick)
        return -1;
    /* Keep Shift held while the ordinary key is pressed and released. */
    if (physical_event(left_shift, 1, &event) || mailbox(0x4b500123u, 0x4b510123u) ||
        keyb_getkeyM(1) != (int)(41 | plane) || !keyb_isLS() || *rtc_tick != saved_tick)
        return -1;
    if (physical_event(left_shift, 0, &event))
        return -1;
    return 0;
}

/* HPGCC 1.0.3 examples/libtests/keyboard.c application flow: scan the physical
 * matrix, print its columns and ON input, pace repeated snapshots, exit on
 * Alpha. Clean snapshots replace raw GPIO/IRQ writes; standard text replaces
 * the historical nonstandard printf("%b"). The test drives real switches. */
static int historical_keyboard(uint32_t model)
{
    const struct hpclang_font *font = &hpclang_system_font;
    struct hpclang_surface *surface = hpclang_display_surface();
    unsigned rows = hpclang_surface_height(surface) / hpclang_font_height(font);
    struct hpclang_console *console = hpclang_console_create(16, rows, font);
    if (!console)
        return -1;
    unsigned alpha = model <= HPCLANG_MODEL_HP40GS ? 58 : 57;
    const unsigned scans[] = {0, 0, 56, 56, alpha};
    const unsigned down[] = {1, 0, 1, 0, 1};
    const uint64_t expected[] = {UINT64_C(1), 0, UINT64_C(1) << 56, 0};
    int result = -1;
    unsigned frames = 0;
    for (unsigned step = 0; step < 5; ++step) {
        struct hpclang_keyboard_event event;
        if (physical_event(scans[step], down[step], &event))
            goto done;
        if (event.key == HPCLANG_KEY_ALPHA)
            break;
        if (step >= 4)
            goto done;
        struct hpclang_keyboard_state keys;
        hpclang_keyboard_read(&keys);
        if (keys.bits != expected[step] || hpclang_console_clear(console))
            goto done;
        for (unsigned column = 0; column < HPCLANG_KEYBOARD_COLUMNS; ++column) {
            static const char digits[] = HPCLANG_UI_TEXT("01234567");
            char line[16] = HPCLANG_UI_TEXT("Con0: ");
            line[3] = digits[column];
            int n = 6;
            for (unsigned row = 0; row < HPCLANG_KEYBOARD_ROWS; ++row)
                line[n++] = hpclang_keyboard_matrix_is_down(&keys, column, row) ? '1' : '0';
            line[n++] = '\n';
            if (hpclang_console_write(console, line, (size_t)n))
                goto done;
        }
        const char *on = hpclang_keyboard_dedicated_is_down(&keys, HPCLANG_KEYBOARD_ON)
                             ? HPCLANG_UI_TEXT("ON: 1")
                             : HPCLANG_UI_TEXT("ON: 0");
        if (hpclang_console_write(console, on, 5) || hpclang_console_render(console, surface, 0, 0))
            goto done;
        uint32_t cell;
        unsigned last = rows < 9 ? rows - 1 : 8;
        if (hpclang_console_cell(console, 4, last, &cell) || cell != (step == 2 ? '1' : '0'))
            goto done;
        if (step >= 2 && mailbox(0x465200cau + step - 2, 0x464b00cau + step - 2))
            goto done;
        uint64_t before = hpclang_time_monotonic_us();
        hpclang_delay_us(1000);
        if (hpclang_time_monotonic_us() - before < 1000)
            goto done;
        ++frames;
    }
    struct hpclang_keyboard_event released;
    if (frames != 4 || physical_event(alpha, 0, &released))
        goto done;
    result = 0;
done:
    hpclang_console_destroy(console);
    return result;
}

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

struct alpha_session
{
    struct line_session line;
    struct hpclang_text_mode mode;
    uint32_t model;
    int physical;
    int locks;
};
static int alpha_next(void *context, struct hpclang_keyboard_event *event)
{
    struct alpha_session *s = context;
    int hp39 = s->model <= HPCLANG_MODEL_HP40GS;
    if (s->locks) {
        /* HP39 Notes exposes its lock as an application menu action. Classic
         * models enter it by double Alpha. Both produce ABcD2D2, exercising a
         * temporary lowercase letter, ordinary input, and leaving the lock. */
        static const unsigned hp39_scans[] = {39, 28, 21, 59, 14, 7, 58, 19, 7, 39, 19, 6};
        static const unsigned classic_scans[] = {57, 57, 35, 36, 58, 37, 38, 57, 19, 57, 38, 19, 6};
        unsigned count = hp39 ? sizeof(hp39_scans) / sizeof(hp39_scans[0])
                              : sizeof(classic_scans) / sizeof(classic_scans[0]);
        unsigned index = s->line.next++;
        if (index / 2 >= count) {
            errno = EIO;
            return -1;
        }
        unsigned scan = (hp39 ? hp39_scans : classic_scans)[index / 2];
        return physical_event(scan, !(index & 1), event) ? -1 : 1;
    }
    uint16_t a = hp39 ? HPCLANG_KEY_HP39_VARS : HPCLANG_KEY_F1;
    uint16_t z = hp39 ? HPCLANG_KEY_3 : HPCLANG_KEY_DIVIDE;
    uint16_t left = hp39 ? HPCLANG_KEY_HP39_SHIFT : HPCLANG_KEY_LEFT_SHIFT;
    uint32_t left_bit = HPCLANG_KEYBOARD_MOD_SHIFT | (hp39 ? 0 : HPCLANG_KEYBOARD_MOD_LEFT_SHIFT);
    /* Scripted integration: single A, model-native lowercase a, held ZZ,
     * release, ordinary 2, accept. Physical event delivery has its own gate. */
#define DOWN(k, m) {.key = (k), .type = HPCLANG_KEYBOARD_EVENT_DOWN, .modifiers = (m)}
#define UP(k, m)                                                                                   \
    {                                                                                              \
        .key = (k), .type = HPCLANG_KEYBOARD_EVENT_UP, .modifiers = (m)                            \
    }
    const struct hpclang_keyboard_event events[] = {
        DOWN(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA),
        UP(HPCLANG_KEY_ALPHA, 0),
        DOWN(a, 0),
        UP(a, 0),
        DOWN(hp39 ? left : HPCLANG_KEY_ALPHA, hp39 ? left_bit : HPCLANG_KEYBOARD_MOD_ALPHA),
        UP(hp39 ? left : HPCLANG_KEY_ALPHA, 0),
        DOWN(hp39 ? HPCLANG_KEY_ALPHA : left, hp39 ? HPCLANG_KEYBOARD_MOD_ALPHA : left_bit),
        UP(hp39 ? HPCLANG_KEY_ALPHA : left, 0),
        DOWN(a, 0),
        UP(a, 0),
        DOWN(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA),
        DOWN(z, HPCLANG_KEYBOARD_MOD_ALPHA),
        UP(z, HPCLANG_KEYBOARD_MOD_ALPHA),
        DOWN(z, HPCLANG_KEYBOARD_MOD_ALPHA),
        UP(z, HPCLANG_KEYBOARD_MOD_ALPHA),
        UP(HPCLANG_KEY_ALPHA, 0),
        DOWN(HPCLANG_KEY_2, 0),
        UP(HPCLANG_KEY_2, 0),
        DOWN(HPCLANG_KEY_ENTER, 0)};
#undef DOWN
#undef UP
    if (s->line.next >= sizeof(events) / sizeof(events[0])) {
        errno = EIO;
        return -1;
    }
    unsigned index = s->line.next++;
    if (s->physical) {
        /* Independent electrical positions from the model keyboard diagrams.
         * Only the emulator drives them; events come from the real SDK poller. */
        unsigned alpha = hp39 ? 58 : 57, shift = hp39 ? 59 : 58;
        unsigned letter_a = hp39 ? 28 : 35, letter_z = hp39 ? 12 : 2;
        const unsigned scans[] = {alpha,
                                  alpha,
                                  letter_a,
                                  letter_a,
                                  hp39 ? shift : alpha,
                                  hp39 ? shift : alpha,
                                  hp39 ? alpha : shift,
                                  hp39 ? alpha : shift,
                                  letter_a,
                                  letter_a,
                                  alpha,
                                  letter_z,
                                  letter_z,
                                  letter_z,
                                  letter_z,
                                  alpha,
                                  19,
                                  19,
                                  6};
        if (physical_event(scans[index], events[index].type == HPCLANG_KEYBOARD_EVENT_DOWN, event))
            return -1;
        if (event->key != events[index].key || event->modifiers != events[index].modifiers) {
            errno = EIO;
            return -1;
        }
    } else {
        *event = events[index];
    }
    return 1;
}
static int alpha_edit(void *context, struct hpclang_text_input *input,
                      const struct hpclang_keyboard_event *event)
{
    struct alpha_session *s = context;
    if (s->locks && s->model <= HPCLANG_MODEL_HP40GS && event->key == HPCLANG_KEY_F5) {
        if (event->type == HPCLANG_KEYBOARD_EVENT_DOWN) {
            struct hpclang_text_mode_status status;
            if (hpclang_text_mode_get(&s->mode, &status))
                return -1;
            return hpclang_text_mode_set_lock(&s->mode, status.lock == HPCLANG_TEXT_UNLOCKED
                                                            ? HPCLANG_TEXT_LOCK_UPPER
                                                            : HPCLANG_TEXT_UNLOCKED);
        }
        return 0;
    }
    return hpclang_text_input_event(input, &s->mode, event);
}
static int alpha_update(void *context, const struct hpclang_text_input *input, int result,
                        int error)
{
    struct alpha_session *s = context;
    return line_update(&s->line, input, result, error);
}

unsigned hpclang_e2e_console_test(void)
{
    if (native_symbols())
        return 13;
    /* Direct RPL-family runs have no live Saturn font context. Use the same
     * explicitly packaged font in both Direct and Stock. */
    const struct hpclang_font *font = &hpclang_system_font;
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
        fputs(HPCLANG_UI_TEXT("!"), stdout) == EOF || ferror(stdout))
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
    struct hpclang_text_input_callbacks line_callbacks = {
        .next = line_next, .accept = line_accept, .update = line_update, .context = &session};
    if (hpclang_text_input_init(&input, edit, sizeof(edit)) != 0 ||
        hpclang_text_input_run(&input, &line_callbacks) != HPCLANG_INPUT_ACCEPTED ||
        strcmp(edit, HPCLANG_UI_TEXT("-12")) != 0 || session.rejected != 1 ||
        session.updates != 6 || hpclang_console_cell(c, 2, 0, &cp) != 0 || cp != '2')
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
    result = 10;
    struct alpha_session alpha = {.line = {.console = c}, .model = model};
    struct hpclang_text_input_callbacks alpha_callbacks = {
        .next = alpha_next, .update = alpha_update, .context = &alpha, .mode = &alpha.mode};
    struct hpclang_text_mode_status mode_status;
    if (hpclang_text_mode_init(&alpha.mode, model) != 0 ||
        hpclang_text_input_init(&input, edit, sizeof(edit)) != 0 ||
        hpclang_text_input_run(&input, &alpha_callbacks) != HPCLANG_INPUT_ACCEPTED ||
        strcmp(edit, "AaZZ2") != 0 || alpha.line.next != 19 || alpha.line.updates != 20 ||
        hpclang_text_mode_get(&alpha.mode, &mode_status) != 0 ||
        mode_status.plane != HPCLANG_TEXT_PLAIN || mode_status.lock != HPCLANG_TEXT_UNLOCKED ||
        hpclang_console_cell(c, 4, 0, &cp) != 0 || cp != '2')
        goto done;
    result = 11;
    /* Begin with released hardware, including the Stock launcher key. Capture
     * the empty screen first so Stock knows the payload owns subsequent input. */
    hpclang_surface_clear(screen, 0);
    if (mailbox(0x465200c7u, 0x464b00c7u))
        goto done;
    struct hpclang_keyboard_state keys;
    uint64_t start = hpclang_time_monotonic_us();
    do {
        hpclang_keyboard_read(&keys);
        if (hpclang_time_monotonic_us() - start > 2000000u)
            goto done;
    } while (hpclang_keyboard_any_is_down(&keys));
    hpclang_keyboard_events_set_timing(20000, 0, 0);
    hpclang_keyboard_events_reset();
    alpha = (struct alpha_session){.line = {.console = c}, .model = model, .physical = 1};
    if (hpclang_text_mode_init(&alpha.mode, model) ||
        hpclang_text_input_init(&input, edit, sizeof(edit)) ||
        hpclang_text_input_run(&input, &alpha_callbacks) != HPCLANG_INPUT_ACCEPTED ||
        strcmp(edit, "AaZZ2") != 0 || alpha.line.next != 19)
        goto done;
    struct hpclang_keyboard_event released;
    enum hpclang_text_plane unused;
    if (physical_event(6, 0, &released) ||
        hpclang_text_mode_event(&alpha.mode, &released, &unused) != 0)
        goto done;
    hpclang_keyboard_read(&keys);
    if (hpclang_keyboard_any_is_down(&keys))
        goto done;
    /* The reference comes from a literal, not from the edited buffer. The host
     * compares both visible bands and rejects an all-blank capture. */
    hpclang_surface_draw_text_n(screen, font, 2, 20, HPCLANG_UI_TEXT("AaZZ2"), 5, 255);
    if (mailbox(0x465200c8u, 0x464b00c8u))
        goto done;
    result = 12;
    hpclang_surface_clear(screen, 0);
    alpha =
        (struct alpha_session){.line = {.console = c}, .model = model, .physical = 1, .locks = 1};
    alpha_callbacks.edit = alpha_edit;
    if (hpclang_text_mode_init(&alpha.mode, model) ||
        hpclang_text_input_init(&input, edit, sizeof(edit)) ||
        hpclang_text_input_run(&input, &alpha_callbacks) != HPCLANG_INPUT_ACCEPTED ||
        strcmp(edit, "ABcD2D2") != 0 || physical_event(6, 0, &released) ||
        hpclang_text_mode_event(&alpha.mode, &released, &unused) != 0 ||
        hpclang_text_mode_get(&alpha.mode, &mode_status) ||
        mode_status.plane != HPCLANG_TEXT_PLAIN || mode_status.lock != HPCLANG_TEXT_UNLOCKED)
        goto done;
    hpclang_keyboard_read(&keys);
    if (hpclang_keyboard_any_is_down(&keys))
        goto done;
    hpclang_surface_draw_text_n(screen, font, 2, 20, HPCLANG_UI_TEXT("ABcD2D2"), 7, 255);
    if (mailbox(0x465200c9u, 0x464b00c9u))
        goto done;
    result = 14;
    if (physical_symbols(model))
        goto done;
    result = 15;
    if (historical_keyboard(model))
        goto done;
    result = 17;
    if (historical_keyboard_adapter(model))
        goto done;
    result = 16;
    if (hpclang_historical_system(mailbox))
        goto done;
    result = 0;
done:
    hpclang_stdio_unbind();
    hpclang_surface_destroy(expected);
    hpclang_console_destroy(c);
    return result;
}
