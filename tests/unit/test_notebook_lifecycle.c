#include "../../examples/notebook/app.h"
#include <hpclang/bootstrap.h>
#include <hpclang/datetime.h>
#include <hpclang/device.h>
#include <hpclang/font-package.h>
#include <hpclang/port.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

/* Execute the shipped entry point, including every cleanup path. */
#define main notebook_main
#include "../../examples/notebook/main.c"
#undef main

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);
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
const struct hpclang_font notebook_font = HPCLANG_BITMAP_FONT(font_data);
static struct hpclang_device_info device;
static struct hpclang_surface *screen;
static unsigned begins, ends, polls, presents;
enum fault
{
    NONE,
    BEGIN,
    PRESENT,
    TIMEOUT,
    KEYBOARD
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
int hpclang_device_capability(enum hpclang_capability capability)
{
    assert(capability == HPCLANG_CAPABILITY_STREAM_STORAGE);
    return HPCLANG_CAPABILITY_UNSUPPORTED;
}
int hpclang_port_query(uint32_t port, struct hpclang_port_info *info)
{
    (void)port;
    (void)info;
    errno = ENOSYS;
    return -1;
}
int hpclang_datetime_read(struct hpclang_datetime *out)
{
    *out = (struct hpclang_datetime){2026, 11, 4, 12, 0, 0};
    return 0;
}
int notebook_load(char text[NOTEBOOK_CAPACITY])
{
    (void)text;
    errno = ENOENT;
    return -1;
}
int notebook_save(const char *text)
{
    (void)text;
    assert(0);
    return -1;
}
int notebook_backup_load(unsigned port, char text[NOTEBOOK_CAPACITY])
{
    (void)port;
    (void)text;
    assert(0);
    return -1;
}
int notebook_backup_save(unsigned port, const char *text)
{
    (void)port;
    (void)text;
    assert(0);
    return -1;
}
int notebook_export_file(const char *path, const char *text)
{
    (void)path;
    (void)text;
    assert(0);
    return -1;
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
    assert(screen && ++polls <= 3);
    if (fault == KEYBOARD) {
        errno = EIO;
        return -1;
    }
    if (polls == 1)
        return 0;
    *event =
        (struct hpclang_keyboard_event){.key = HPCLANG_KEY_ON, .type = HPCLANG_KEYBOARD_EVENT_DOWN};
    return 1;
}
void hpclang_delay_us(uint64_t microseconds)
{
    assert(microseconds == 20000);
}

static unsigned run(enum fault selected, int allocations, int expected)
{
    assert(!live && !screen);
    fault = selected;
    remaining = allocations;
    attempts = begins = ends = polls = presents = 0;
    assert(notebook_main() == expected);
    assert(!live && !screen && ends == begins);
    if (!expected) {
        assert(begins == 1 && polls == 2);
        assert(presents == (selected == TIMEOUT ? 2u : 1u));
    }
    return attempts;
}
int main(void)
{
    for (unsigned model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        device = (struct hpclang_device_info){
            .model = model, .display_width = 131, .display_height = model <= 4 ? 64 : 80};
        unsigned count = run(NONE, -1, 0);
        assert(count > 0);
        for (unsigned at = 0; at < count; ++at) {
            run(NONE, (int)at, 1);
            run(NONE, -1, 0);
        }
        run(BEGIN, -1, 1);
        run(PRESENT, -1, 1);
        run(KEYBOARD, -1, 1);
        run(TIMEOUT, -1, 0);
        run(NONE, -1, 0);
    }
    return 0;
}
