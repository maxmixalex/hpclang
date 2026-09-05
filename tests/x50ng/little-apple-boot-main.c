#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "emulator_api.h"
#include "hdw.h"
#include "module.h"
#include "options.h"\n#include "headless-ui-init.h"
#include "timer.h"
#include "s3c2410/s3c2410_lcd.h"

#define LCD_W 131
#define LCD_H 64

struct state {
    hdw_t *hdw;
    const char *prefix;
    const char *json;
    unsigned captures;
    unsigned last_nonzero;
};

struct key_action { int key; int down; };
static const struct key_action f_down = { HP4950_KEY_F, 1 };
static const struct key_action f_up = { HP4950_KEY_F, 0 };

static void key_cb(void *arg)
{
    const struct key_action *a = arg;
    if (a->down) press_key(a->key);
    else release_key(a->key);
}

static unsigned capture(hdw_t *h, const char *path)
{
    FILE *f = fopen(path, "wb");
    unsigned nonzero = 0;
    if (!f) return 0;
    fprintf(f, "P5\n%d %d\n255\n", LCD_W, LCD_H);
    for (int y = 0; y < LCD_H; ++y) {
        for (int x = 0; x < LCD_W; ++x) {
            int level = s3c2410_get_pixel_color(
                (s3c2410_lcd_t *)h->s3c2410_lcd, x, y);
            if (level < 0) level = 0;
            if (level > 15) level = 15;
            if (level) ++nonzero;
            uint8_t p = (uint8_t)(255 - 17 * level);
            fwrite(&p, 1, 1, f);
        }
    }
    fclose(f);
    return nonzero;
}

static void snapshot(void *arg)
{
    struct state *s = arg;
    char path[512];
    ++s->captures;
    snprintf(path, sizeof(path), "%s-%u.pgm", s->prefix, s->captures);
    s->last_nonzero = capture(s->hdw, path);
    fprintf(stderr,
            "Little-Apple snapshot %u: pc=%08x display=%d pixels=%u\n",
            s->captures, s->hdw->cpu->regs[15],
            is_display_on(), s->last_nonzero);
}

static void finish(void *arg)
{
    struct state *s = arg;
    FILE *f = fopen(s->json, "w");
    if (f) {
        fprintf(
            f,
            "{\n"
            "  \"pc\":\"0x%08x\",\n"
            "  \"cpsr\":\"0x%08x\",\n"
            "  \"display_on\":%s,\n"
            "  \"captures\":%u,\n"
            "  \"last_nonzero_pixels\":%u,\n"
            "  \"sctlr\":\"0x%08x\",\n"
            "  \"ttbr0\":\"0x%08x\",\n"
            "  \"ifsr\":\"0x%08x\",\n"
            "  \"ifar\":\"0x%08x\"\n"
            "}\n",
            s->hdw->cpu->regs[15],
            cpsr_read(s->hdw->cpu),
            is_display_on() ? "true" : "false",
            s->captures,
            s->last_nonzero,
            s->hdw->cpu->cp15.c1_sys,
            s->hdw->cpu->cp15.c2_base0,
            s->hdw->cpu->cp15.c5_insn,
            s->hdw->cpu->cp15.c6_insn);
        fclose(f);
    }
    hdw_stop(s->hdw);
}

static int later(timer_callback_t fn, void *arg, int64_t us)
{
    hdw_timer_t *t = timer_new(HDW_TIMER_REALTIME, fn, arg);
    if (!t) return 0;
    timer_mod(t, timer_get_clock() + us);
    return 1;
}

int main(int argc, char **argv)
{
    struct state s = {0};
    s.prefix = getenv("HPCLANG_LA_PREFIX");
    s.json = getenv("HPCLANG_LA_JSON");
    if (!s.prefix) s.prefix = "little-apple";
    if (!s.json) s.json = "little-apple.json";

    config_t config = hpclang_x50ng_headless_config(argc, argv);
    s.hdw = emulator_init(&config);

    if (!later(snapshot, &s, 3000000LL) ||
        !later(key_cb, (void *)&f_down, 4000000LL) ||
        !later(key_cb, (void *)&f_up, 4100000LL) ||
        !later(snapshot, &s, 6000000LL) ||
        !later(snapshot, &s, 10000000LL) ||
        !later(snapshot, &s, 15000000LL) ||
        !later(finish, &s, 15100000LL)) {
        fprintf(stderr, "Little-Apple explorer: cannot schedule captures\n");
        return EXIT_FAILURE;
    }

    main_loop(s.hdw);
    exit_modules(s.hdw);
    return EXIT_SUCCESS;
}
