#include "../../examples/declarative-ui/app.h"
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/font-package.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

static void observed_init(struct demo_app *, struct hpclang_ui *);
static int observed_shutdown(struct hpclang_ui *);
#define demo_init observed_init
#define hpclang_ui_shutdown observed_shutdown
#define main demo_main
#include "../../examples/declarative-ui/main.c"
#undef main
#undef hpclang_ui_shutdown
#undef demo_init

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
static int remaining = -1;
static unsigned live, attempts;
static int reject(void)
{
    ++attempts;
    if (!remaining) {
        errno = ENOMEM;
        return 1;
    }
    if (remaining > 0)
        --remaining;
    return 0;
}
void *__wrap_malloc(size_t size)
{
    if (reject())
        return NULL;
    void *p = __real_malloc(size);
    live += p != NULL;
    return p;
}
void *__wrap_calloc(size_t count, size_t size)
{
    if (reject())
        return NULL;
    void *p = __real_calloc(count, size);
    live += p != NULL;
    return p;
}
void __wrap_free(void *p)
{
    if (p) {
        assert(live);
        --live;
    }
    __real_free(p);
}

static const uint8_t ink[] = {0x80};
static const struct hpclang_bitmap_glyph glyph = {
    .codepoint = '?', .bitmap = ink, .width = 1, .height = 1, .advance = 2, .stride = 1};
static const struct hpclang_bitmap_glyph *const glyphs[] = {&glyph};
static const struct hpclang_bitmap_font_data font_data = {.glyphs = glyphs,
                                                          .glyph_count = 1,
                                                          .fallback_codepoint = '?',
                                                          .ascent = 1,
                                                          .line_height = 2,
                                                          .nominal_advance = 2};
const struct hpclang_font demo_font = HPCLANG_BITMAP_FONT(font_data);

static struct hpclang_device_info device;
static struct hpclang_surface *screen;
static struct demo_app *active;
static unsigned begins, ends, polls, presents, shutdowns, idle_attempts;
enum fault
{
    NONE,
    BEGIN,
    PRESENT,
    TIMEOUT,
    KEYBOARD,
    ASYNC_ERROR,
    ASYNC_READY
};
static enum fault fault;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return NULL;
}
const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}

static void observed_init(struct demo_app *app, struct hpclang_ui *ui)
{
    demo_init(app, ui);
    active = app;
    struct hpclang_ui_event event = {.kind = HPCLANG_UI_ACTIVATE, .action = {DEMO_CONNECT, 0}};
    assert(demo_update(app, &event) == 1);
    /* The provider fails the first request and succeeds on subsequent attempts. */
    if (fault == ASYNC_READY)
        app->attempts = 1;
    event.action.type = DEMO_SCAN;
    assert(demo_update(app, &event) == 1);
    assert(app->request && app->model.scan == DEMO_LOADING);
}
static int observed_shutdown(struct hpclang_ui *ui)
{
    ++shutdowns;
    int result = hpclang_ui_shutdown(ui);
    assert(result == 0);
    if (active) {
        assert(active->cleanups == 1);
        if (fault == ASYNC_ERROR || fault == ASYNC_READY) {
            assert(active->polls == 20 && !active->request);
            assert(active->model.scan == (fault == ASYNC_READY ? DEMO_READY : DEMO_ERROR));
            assert(active->model.count == (fault == ASYNC_READY ? 3u : 0u));
        } else
            assert(active->polls < 20);
        assert(hpclang_ui_shutdown(ui) == 0 && active->cleanups == 1);
        active = NULL;
    }
    return result;
}
int hpclang_display_begin(uint32_t levels)
{
    assert(levels == 2 && !screen);
    if (fault == BEGIN) {
        errno = EIO;
        return -1;
    }
    screen = hpclang_surface_create_mono(device.display_width, device.display_height);
    if (!screen)
        return -1;
    ++begins;
    return 0;
}
struct hpclang_surface *hpclang_display_surface(void)
{
    assert(screen);
    return screen;
}
void hpclang_display_end(void)
{
    assert(screen && begins == 1 && !ends);
    hpclang_surface_destroy(screen);
    screen = NULL;
    ++ends;
}
int hpclang_display_present(uint32_t timeout_us)
{
    assert(screen && timeout_us == 100000);
    ++presents;
    if (fault == PRESENT || (fault == TIMEOUT && presents == 1)) {
        errno = fault == TIMEOUT ? ETIMEDOUT : EIO;
        return -1;
    }
    return 0;
}

void hpclang_keyboard_events_reset(void)
{
    assert(screen);
}
int hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event)
{
    unsigned exit_poll = fault == ASYNC_ERROR || fault == ASYNC_READY ? 25 : 3;
    assert(screen && ++polls <= exit_poll);
    if (polls == 1)
        idle_attempts = attempts;
    else
        assert(attempts == idle_attempts);
    if (fault == KEYBOARD) {
        errno = EIO;
        return -1;
    }
    if (polls < exit_poll)
        return 0;
    *event =
        (struct hpclang_keyboard_event){.key = HPCLANG_KEY_ON, .type = HPCLANG_KEYBOARD_EVENT_DOWN};
    return 1;
}
void hpclang_delay_us(uint64_t us)
{
    assert(us == 20000);
}
static int run(int fail_after, enum fault injected)
{
    assert(!live && !screen && !active);
    remaining = fail_after;
    fault = injected;
    begins = ends = polls = presents = shutdowns = attempts = 0;
    int result = demo_main();
    remaining = -1;
    assert(!live && !screen && !active && begins == ends);
    assert(shutdowns <= 1);
    if (!result) {
        unsigned exit_poll = fault == ASYNC_ERROR || fault == ASYNC_READY ? 25 : 3;
        assert(begins == 1 && polls == exit_poll && shutdowns == 1);
        /* One initial publication; one more only for completion or timeout retry. */
        assert(presents == (fault == NONE ? 1u : 2u));
    }
    return result;
}
int main(void)
{
    for (unsigned model = 1; model <= 6; ++model) {
        device = (struct hpclang_device_info){
            .model = model, .display_width = 131, .display_height = model <= 4 ? 64 : 80};
        assert(run(-1, NONE) == 0);
        unsigned allocations = attempts;
        for (unsigned i = 0; i < allocations; ++i) {
            assert(run((int)i, NONE) == 1);
            assert(run(-1, NONE) == 0);
        }
        assert(run(-1, BEGIN) == 1);
        assert(run(-1, PRESENT) == 1);
        assert(run(-1, KEYBOARD) == 1);
        assert(run(-1, TIMEOUT) == 0);
        assert(run(-1, ASYNC_ERROR) == 0);
        assert(run(-1, ASYNC_READY) == 0);
        assert(run(-1, NONE) == 0);
    }
    return 0;
}
