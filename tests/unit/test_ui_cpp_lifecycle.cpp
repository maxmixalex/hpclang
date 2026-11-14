#include "../../examples/declarative-ui-cpp/app.hpp"
#include <hpclang/bootstrap.h>
#include <hpclang/font-package.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>

static int observed_view(hpclang_ui *, hpclang_ui_surface *, hpclang_ui_rect, unsigned);
#define counter_view observed_view
#define main counter_main
#include "../../examples/declarative-ui-cpp/main.cpp"
#undef main
#undef counter_view

extern "C" void *__real_malloc(size_t);
extern "C" void *__real_calloc(size_t, size_t);
extern "C" void __real_free(void *);
static int remaining = -1;
static unsigned live, attempts;
static bool reject()
{
    ++attempts;
    if (!remaining) {
        errno = ENOMEM;
        return true;
    }
    if (remaining > 0)
        --remaining;
    return false;
}
extern "C" void *__wrap_malloc(size_t size)
{
    if (reject())
        return nullptr;
    void *p = __real_malloc(size);
    live += p != nullptr;
    return p;
}
extern "C" void *__wrap_calloc(size_t count, size_t size)
{
    if (reject())
        return nullptr;
    void *p = __real_calloc(count, size);
    live += p != nullptr;
    return p;
}
extern "C" void __wrap_free(void *p)
{
    if (p) {
        assert(live);
        --live;
    }
    __real_free(p);
}

static const uint8_t ink[] = {0x80};
static const hpclang_bitmap_glyph glyph{'?', ink, 0, 0, 1, 1, 2, 1};
static const hpclang_bitmap_glyph *const glyphs[] = {&glyph};
static const hpclang_bitmap_font_data font_data{glyphs, 1, '?', 1, 0, 2, 2};
extern "C" const hpclang_font counter_font = HPCLANG_BITMAP_FONT(font_data);

static struct hpclang_device_info device;
static hpclang_surface *screen;
static unsigned begins, ends, polls, presents, views, idle_attempts;
enum class fault
{
    none,
    begin,
    present,
    timeout,
    keyboard,
    actions
};
static fault injected;

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return nullptr;
}
const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}

static int observed_view(hpclang_ui *ui, hpclang_ui_surface *renderer, hpclang_ui_rect viewport,
                         unsigned value)
{
    if (injected == fault::actions) {
        const unsigned expected[] = {0, 1, 0};
        assert(views < 3 && value == expected[views]);
    } else
        assert(value == 0);
    ++views;
    return counter_view(ui, renderer, viewport, value);
}

int hpclang_display_begin(uint32_t levels)
{
    assert(levels == 2 && !screen);
    if (injected == fault::begin) {
        errno = EIO;
        return -1;
    }
    screen = hpclang_surface_create_mono(device.display_width, device.display_height);
    if (!screen)
        return -1;
    ++begins;
    return 0;
}
hpclang_surface *hpclang_display_surface(void)
{
    assert(screen);
    return screen;
}
void hpclang_display_end(void)
{
    assert(screen && begins == 1 && !ends);
    hpclang_surface_destroy(screen);
    screen = nullptr;
    ++ends;
}
int hpclang_display_present(uint32_t timeout)
{
    assert(screen && timeout == 100000);
    ++presents;
    if (injected == fault::present || (injected == fault::timeout && presents == 1)) {
        errno = injected == fault::timeout ? ETIMEDOUT : EIO;
        return -1;
    }
    return 0;
}
void hpclang_keyboard_events_reset(void)
{
    assert(screen);
}
int hpclang_keyboard_event_poll(hpclang_keyboard_event *event)
{
    assert(screen && ++polls <= 5);
    if (polls == 1)
        idle_attempts = attempts;
    else
        assert(attempts == idle_attempts);
    if (injected == fault::keyboard) {
        errno = EIO;
        return -1;
    }
    *event = {};
    event->type = HPCLANG_KEYBOARD_EVENT_DOWN;
    if (injected == fault::actions) {
        event->key = polls % 2 ? HPCLANG_KEY_ENTER : HPCLANG_KEY_DOWN;
        return 1;
    }
    if (polls < 3)
        return 0;
    event->key = HPCLANG_KEY_ON;
    return 1;
}
void hpclang_delay_us(uint64_t us)
{
    assert(us == 20000);
}

static int exercise(int fail_after, fault selected)
{
    assert(!live && !screen);
    remaining = fail_after;
    injected = selected;
    begins = ends = polls = presents = views = attempts = 0;
    int result = counter_main();
    remaining = -1;
    assert(!live && !screen && begins == ends);
    if (!result) {
        assert(begins == 1 && polls == (selected == fault::actions ? 5u : 3u));
        assert(views == (selected == fault::actions ? 3u : 1u));
        assert(presents >= (selected == fault::timeout ? 2u : 1u));
    }
    return result;
}
int main()
{
    for (unsigned model = 1; model <= 6; ++model) {
        device = {};
        device.model = model;
        device.display_width = 131;
        device.display_height = model <= 4 ? 64 : 80;
        assert(exercise(-1, fault::none) == 0);
        unsigned allocations = attempts;
        for (unsigned i = 0; i < allocations; ++i) {
            assert(exercise(static_cast<int>(i), fault::none) == 1);
            assert(exercise(-1, fault::none) == 0);
        }
        for (auto selected : {fault::begin, fault::present, fault::keyboard})
            assert(exercise(-1, selected) == 1);
        assert(exercise(-1, fault::timeout) == 0);
        assert(exercise(-1, fault::actions) == 0);
        assert(exercise(-1, fault::none) == 0);
    }
}
