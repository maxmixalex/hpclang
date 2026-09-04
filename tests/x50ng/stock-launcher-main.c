#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "emulator_api.h"
#include "hdw.h"
#include "module.h"
#include "options.h"
#include "timer.h"
#include "ui4x/src/api.h"
#include "s3c2410/s3c2410_lcd.h"

#define LCD_W 131
#define LCD_H 80
#define KEY_HOLD_US 100000LL

struct state {
    hdw_t *hdw;
    const char *home;
    const char *files;
    const char *json;
};

struct key_action { int key; int down; };

static const struct key_action f_down = { HP4950_KEY_F, 1 };
static const struct key_action f_up = { HP4950_KEY_F, 0 };
static const struct key_action lshift_down = { HP4950_KEY_LEFTSHIFT, 1 };
static const struct key_action lshift_up = { HP4950_KEY_LEFTSHIFT, 0 };
static const struct key_action g_down = { HP4950_KEY_G, 1 };
static const struct key_action g_up = { HP4950_KEY_G, 0 };

static int later(timer_callback_t fn, void *arg, int64_t us)
{
    hdw_timer_t *t = timer_new(HDW_TIMER_REALTIME, fn, arg);
    if (!t) return 0;
    timer_mod(t, timer_get_clock() + us);
    return 1;
}

static void key_cb(void *arg)
{
    const struct key_action *a = arg;
    if (a->down) press_key(a->key);
    else release_key(a->key);
}

static int tap(const struct key_action *down, const struct key_action *up, int64_t us)
{
    return later(key_cb, (void *)down, us) &&
           later(key_cb, (void *)up, us + KEY_HOLD_US);
}

static int chord(
    const struct key_action *mod_down,
    const struct key_action *key_down,
    const struct key_action *key_up,
    const struct key_action *mod_up,
    int64_t us)
{
    return later(key_cb, (void *)mod_down, us) &&
           later(key_cb, (void *)key_down, us + 30000) &&
           later(key_cb, (void *)key_up, us + 130000) &&
           later(key_cb, (void *)mod_up, us + 160000);
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

static void capture_home(void *arg)
{
    struct state *s = arg;
    if (!capture(s->hdw, s->home)) {
        fprintf(stderr, "stock launcher explorer: blank HOME capture\n");
        hdw_stop(s->hdw);
    }
}

static void open_files(void *arg)
{
    (void)arg;
    if (!chord(&lshift_down, &g_down, &g_up, &lshift_up, 0))
        fprintf(stderr, "stock launcher explorer: cannot schedule FILES chord\n");
}

static void finish(void *arg)
{
    struct state *s = arg;
    unsigned files_pixels = capture(s->hdw, s->files);
    FILE *f = fopen(s->json, "w");
    if (f) {
        fprintf(
            f,
            "{\n"
            "  \"pc\":\"0x%08x\",\n"
            "  \"display_on\":%s,\n"
            "  \"sd_mounted\":%s,\n"
            "  \"files_nonzero_pixels\":%u\n"
            "}\n",
            s->hdw->cpu->regs[15],
            is_display_on() ? "true" : "false",
            emulator_is_sd_mounted() ? "true" : "false",
            files_pixels);
        fclose(f);
    }
    if (!is_display_on() || !files_pixels || !emulator_is_sd_mounted()) {
        fprintf(stderr, "stock launcher explorer did not reach a usable FILES state\n");
        exit(EXIT_FAILURE);
    }
    hdw_stop(s->hdw);
}

int main(int argc, char **argv)
{
    struct state s = {0};
    s.home = getenv("HPCLANG_STOCK_HOME_PGM");
    s.files = getenv("HPCLANG_STOCK_FILES_PGM");
    s.json = getenv("HPCLANG_STOCK_JSON");
    if (!s.home) s.home = "stock-home.pgm";
    if (!s.files) s.files = "stock-files.pgm";
    if (!s.json) s.json = "stock-explorer.json";

    config_t config = *config_init(argc, argv);
    s.hdw = emulator_init(&config);

    if (!tap(&f_down, &f_up, 2000000LL) ||
        !tap(&f_down, &f_up, 4000000LL) ||
        !later(capture_home, &s, 7000000LL) ||
        !later(open_files, &s, 8000000LL) ||
        !later(finish, &s, 10500000LL)) {
        fprintf(stderr, "stock launcher explorer: cannot schedule actions\n");
        return EXIT_FAILURE;
    }

    main_loop(s.hdw);
    exit_modules(s.hdw);
    return EXIT_SUCCESS;
}
