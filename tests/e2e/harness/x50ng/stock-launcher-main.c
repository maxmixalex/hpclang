#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emulator_api.h"
#include "hdw.h"
#include "module.h"
#include "options.h"
#include "timer.h"
#include "headless-ui-init.h"
#include "s3c2410/s3c2410_lcd.h"

#define LCD_W 131
#define KEY_HOLD_US 100000LL
#define PAYLOAD_KEY_HOLD_US 50000LL
#define PAYLOAD_KEY_STEP_US 120000LL
#define PAYLOAD_KEY_FIRST_DELAY_US 20000LL
#define PASS_WORD 0x50415353u
#define FAIL_PREFIX 0x46410000u
#define PHASE_KEYBOARD_READY 0x4b425244u
#define POLL_US 20000LL
#define DEFAULT_RUN_DEADLINE_US 10000000LL
#define RETURN_CAPTURE_US 800000LL

enum stock_mode {
    MODE_INSTALL,
    MODE_RUN,
};

struct state {
    hdw_t *hdw;
    enum stock_mode mode;
    const char *prefix;
    const char *json;
    const char *before_pgm;
    const char *pass_pgm;
    const char *return_pgm;
    const char *runtime_json;
    uint32_t result_addr;
    int saw_before;
    int saw_pass;
    int payload_keyboard_scheduled;
    int returned;
    int failed;
    int lcd_height;
    int64_t deadline;
};

struct key_action { int key; int down; };

#define KEY_PAIR(name, code) \
    static const struct key_action name##_down = { code, 1 }; \
    static const struct key_action name##_up = { code, 0 }

KEY_PAIR(f1, HP4950_KEY_A);
KEY_PAIR(f2, HP4950_KEY_B);
KEY_PAIR(f3, HP4950_KEY_C);
KEY_PAIR(f4, HP4950_KEY_D);
KEY_PAIR(f5, HP4950_KEY_E);
KEY_PAIR(f6, HP4950_KEY_F);
KEY_PAIR(files, HP4950_KEY_G);
KEY_PAIR(symb, HP4950_KEY_G);
KEY_PAIR(plot, HP4950_KEY_H);
KEY_PAIR(num, HP4950_KEY_I);
KEY_PAIR(home, HP4950_KEY_J);
KEY_PAIR(aplet, HP4950_KEY_K);
KEY_PAIR(views, HP4950_KEY_L);
KEY_PAIR(mode, HP4950_KEY_H);
KEY_PAIR(var, HP4950_KEY_J);
KEY_PAIR(eval, HP4950_KEY_N);
KEY_PAIR(nxt, HP4950_KEY_L);
KEY_PAIR(neg, HP4950_KEY_W);
KEY_PAIR(up, HP4950_KEY_UP);
KEY_PAIR(down, HP4950_KEY_DOWN);
KEY_PAIR(left, HP4950_KEY_LEFT);
KEY_PAIR(right, HP4950_KEY_RIGHT);
KEY_PAIR(zero, HP4950_KEY_0);
KEY_PAIR(two, HP4950_KEY_2);
KEY_PAIR(sto, HP4950_KEY_K);
KEY_PAIR(period, HP4950_KEY_PERIOD);
KEY_PAIR(enter, HP4950_KEY_ENTER);
KEY_PAIR(on, HP4950_KEY_ON);
KEY_PAIR(lshift, HP4950_KEY_LEFTSHIFT);
KEY_PAIR(rshift, HP4950_KEY_RIGHTSHIFT);

struct key_pair_ref {
    const char *name;
    const struct key_action *down;
    const struct key_action *up;
};

#define KEY_REF(name) { #name, &name##_down, &name##_up }
static const struct key_pair_ref key_pairs[] = {
    KEY_REF(f1), KEY_REF(f2), KEY_REF(f3), KEY_REF(f4), KEY_REF(f5), KEY_REF(f6),
    KEY_REF(files), KEY_REF(symb), KEY_REF(plot), KEY_REF(num), KEY_REF(home),
    KEY_REF(aplet), KEY_REF(views), KEY_REF(mode), KEY_REF(var), KEY_REF(eval),
    KEY_REF(nxt), KEY_REF(neg), KEY_REF(up), KEY_REF(down), KEY_REF(left),
    KEY_REF(right), KEY_REF(zero), KEY_REF(two), KEY_REF(sto), KEY_REF(period),
    KEY_REF(enter), KEY_REF(on), KEY_REF(lshift), KEY_REF(rshift),
};
#undef KEY_REF

static const int payload_keyboard_codes[] = {
    HP4950_KEY_BACKSPACE,
    HP4950_KEY_U,
    HP4950_KEY_Z,
    HP4950_KEY_MULTIPLY,
    HP4950_KEY_MINUS,
    HP4950_KEY_PLUS,
    HP4950_KEY_ENTER,
    HP4950_KEY_P,
    HP4950_KEY_T,
    HP4950_KEY_Y,
    HP4950_KEY_9,
    HP4950_KEY_6,
    HP4950_KEY_3,
    HP4950_KEY_SPACE,
    HP4950_KEY_O,
    HP4950_KEY_S,
    HP4950_KEY_X,
    HP4950_KEY_8,
    HP4950_KEY_5,
    HP4950_KEY_2,
    HP4950_KEY_PERIOD,
    HP4950_KEY_N,
    HP4950_KEY_R,
    HP4950_KEY_W,
    HP4950_KEY_7,
    HP4950_KEY_4,
    HP4950_KEY_1,
    HP4950_KEY_0,
    HP4950_KEY_M,
    HP4950_KEY_Q,
    HP4950_KEY_V,
    HP4950_KEY_A,
    HP4950_KEY_B,
    HP4950_KEY_C,
    HP4950_KEY_D,
    HP4950_KEY_E,
    HP4950_KEY_F,
    HP4950_KEY_G,
    HP4950_KEY_UP,
    HP4950_KEY_LEFT,
    HP4950_KEY_DOWN,
    HP4950_KEY_RIGHT,
    HP4950_KEY_H,
    HP4950_KEY_I,
    HP4950_KEY_J,
    HP4950_KEY_K,
    HP4950_KEY_L,
    HP4950_KEY_ON,
    HP4950_KEY_ALPHA,
    HP4950_KEY_LEFTSHIFT,
    HP4950_KEY_RIGHTSHIFT,
};

static struct key_action payload_keyboard_down[
    sizeof(payload_keyboard_codes) / sizeof(payload_keyboard_codes[0])];
static struct key_action payload_keyboard_up[
    sizeof(payload_keyboard_codes) / sizeof(payload_keyboard_codes[0])];

static const struct key_pair_ref *find_key_pair(const char *name)
{
    size_t n = sizeof(key_pairs) / sizeof(key_pairs[0]);
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(key_pairs[i].name, name) == 0)
            return &key_pairs[i];
    }
    return NULL;
}

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

static int tap(
    const struct key_action *down,
    const struct key_action *up_action,
    int64_t us)
{
    return later(key_cb, (void *)down, us) &&
           later(key_cb, (void *)up_action, us + KEY_HOLD_US);
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

static int schedule_payload_keyboard(struct state *s)
{
    size_t count = sizeof(payload_keyboard_codes) /
                   sizeof(payload_keyboard_codes[0]);

    if (s->payload_keyboard_scheduled)
        return 1;

    for (size_t i = 0; i < count; ++i) {
        payload_keyboard_down[i].key = payload_keyboard_codes[i];
        payload_keyboard_down[i].down = 1;
        payload_keyboard_up[i].key = payload_keyboard_codes[i];
        payload_keyboard_up[i].down = 0;

        int64_t press_us = PAYLOAD_KEY_FIRST_DELAY_US +
            (int64_t)i * PAYLOAD_KEY_STEP_US;
        if (!later(key_cb, &payload_keyboard_down[i], press_us) ||
            !later(key_cb, &payload_keyboard_up[i],
                   press_us + PAYLOAD_KEY_HOLD_US))
            return 0;
    }

    s->payload_keyboard_scheduled = 1;
    fprintf(stderr, "stock launcher: payload keyboard phase reached; "
                    "scheduled %zu keys\n", count);
    return 1;
}

static unsigned capture(hdw_t *h, const char *path, int height)
{
    FILE *f = fopen(path, "wb");
    unsigned nonzero = 0;
    if (!f) return 0;

    fprintf(f, "P5\n%d %d\n255\n", LCD_W, height);
    for (int y = 0; y < height; ++y) {
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

static void capture_named(struct state *s, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s-%s.pgm", s->prefix, name);
    if (!capture(s->hdw, path, s->lcd_height))
        fprintf(stderr, "stock launcher: blank capture %s\n", name);
}

static int guest_read32(hdw_t *h, uint32_t address, uint32_t *value)
{
    uint8_t b[4];
    if (cpu_memory_rw_debug(h->cpu, address, b, sizeof(b), 0) != 0)
        return 0;
    *value = (uint32_t)b[0] |
             ((uint32_t)b[1] << 8) |
             ((uint32_t)b[2] << 16) |
             ((uint32_t)b[3] << 24);
    return 1;
}

static void write_json(struct state *s, const char *status)
{
    FILE *f = fopen(s->json, "w");
    if (!f) return;
    fprintf(
        f,
        "{\n"
        "  \"mode\":\"%s\",\n"
        "  \"status\":\"%s\",\n"
        "  \"saw_pass\":%s,\n"
        "  \"payload_keyboard_scheduled\":%s,\n"
        "  \"returned\":%s,\n"
        "  \"pc\":\"0x%08x\",\n"
        "  \"display_on\":%s,\n"
        "  \"sd_mounted\":%s\n"
        "}\n",
        s->mode == MODE_INSTALL ? "install" : "run",
        status,
        s->saw_pass ? "true" : "false",
        s->payload_keyboard_scheduled ? "true" : "false",
        s->returned ? "true" : "false",
        s->hdw->cpu->regs[15],
        is_display_on() ? "true" : "false",
        emulator_is_sd_mounted() ? "true" : "false");
    fclose(f);
}

static void write_runtime_json(struct state *s, const char *status)
{
    FILE *f = fopen(s->runtime_json, "w");
    if (!f) return;
    fprintf(
        f,
        "{\n"
        "  \"status\":\"%s\",\n"
        "  \"saw_pass\":%s,\n"
        "  \"payload_keyboard_scheduled\":%s,\n"
        "  \"returned\":%s,\n"
        "  \"pc\":\"0x%08x\"\n"
        "}\n",
        status,
        s->saw_pass ? "true" : "false",
        s->payload_keyboard_scheduled ? "true" : "false",
        s->returned ? "true" : "false",
        s->hdw->cpu->regs[15]);
    fclose(f);
}

static void fail(struct state *s, const char *why)
{
    s->failed = 1;
    fprintf(stderr, "stock launcher FAIL: %s\n", why);
    capture_named(s, "failure");
    write_json(s, why);
    if (s->mode == MODE_RUN)
        write_runtime_json(s, why);
    hdw_stop(s->hdw);
}

static void finish_install(void *arg)
{
    struct state *s = arg;
    capture_named(s, "installed");
    if (!is_display_on()) {
        fail(s, "install-display");
        return;
    }
    write_json(s, "installed");
    hdw_stop(s->hdw);
}

static void finish_return(void *arg)
{
    struct state *s = arg;
    unsigned pixels = capture(s->hdw, s->return_pgm, s->lcd_height);

    if (!is_display_on() || pixels == 0) {
        fail(s, "stock-resume");
        return;
    }

    s->returned = 1;
    write_runtime_json(s, "pass");
    write_json(s, "pass");
    hdw_stop(s->hdw);
}

static void monitor(void *arg)
{
    struct state *s = arg;
    uint32_t value = 0;
    int readable;

    if (s->deadline == 0) {
        if (!later(monitor, s, POLL_US))
            fail(s, "monitor-timer");
        return;
    }

    if (timer_get_clock() >= s->deadline) {
        fail(s, "timeout");
        return;
    }

    if (!s->saw_before) {
        if (!capture(s->hdw, s->before_pgm, s->lcd_height)) {
            fail(s, "before-capture");
            return;
        }
        s->saw_before = 1;
    }

    readable = guest_read32(s->hdw, s->result_addr, &value);

    if (!s->payload_keyboard_scheduled && readable &&
        value == PHASE_KEYBOARD_READY) {
        if (!schedule_payload_keyboard(s)) {
            fail(s, "keyboard-schedule");
            return;
        }
    }

    if (readable) {
        if (!s->saw_pass && value == PASS_WORD) {
            s->saw_pass = 1;
            if (!capture(s->hdw, s->pass_pgm, s->lcd_height)) {
                fail(s, "pass-capture");
                return;
            }
            fprintf(stderr, "stock ARMToolbox path reached HPC1 PASS\n");
            write_runtime_json(s, "pass-observed");
        } else if ((value & 0xffff0000u) == FAIL_PREFIX) {
            fprintf(stderr, "stock runtime failure 0x%08x\n", value);
            fail(s, "runtime-fail");
            return;
        }
    }

    /*
     * The payload never changes hpclang_e2e_result after PASS. A different
     * value (or an unmapped address) therefore means stage0 has left HPC1 and
     * restored the stock address space. Do not inject a wake key here: the
     * transition back to the stock UI is itself part of the return-path gate.
     */
    if (s->saw_pass && (!readable || value != PASS_WORD)) {
        if (!later(finish_return, s, RETURN_CAPTURE_US))
            fail(s, "return-timer");
        return;
    }

    if (!later(monitor, s, POLL_US))
        fail(s, "monitor-timer");
}

struct seq_named_event {
    struct state *state;
    char text[96];
};

static struct seq_named_event *seq_named_event_new(
    struct state *s, const char *text)
{
    struct seq_named_event *event = calloc(1, sizeof(*event));
    if (!event) return NULL;
    snprintf(event->text, sizeof(event->text), "%s", text);
    event->state = s;
    return event;
}

static void seq_capture(void *arg)
{
    struct seq_named_event *event = arg;
    capture_named(event->state, event->text);
    free(event);
}

static void seq_fail(void *arg)
{
    struct seq_named_event *event = arg;
    struct state *s = event->state;
    char why[sizeof(event->text)];
    snprintf(why, sizeof(why), "%s", event->text);
    free(event);
    fail(s, why);
}

struct seq_monitor_event {
    struct state *state;
    int64_t timeout_us;
};

static void seq_monitor(void *arg)
{
    struct seq_monitor_event *event = arg;
    struct state *s = event->state;
    s->deadline = timer_get_clock() + event->timeout_us;
    free(event);
    monitor(s);
}

static int schedule_sequence(struct state *s, const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    unsigned line_no = 0;

    if (!f) {
        fprintf(stderr, "cannot open E2E sequence %s\n", path);
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        unsigned long long ms = 0;
        char op[32] = {0};
        char first[96] = {0};
        char second[96] = {0};
        char *p = line;
        int fields;
        int64_t us;

        ++line_no;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '\n' || *p == '#')
            continue;

        fields = sscanf(p, "%llu %31s %95s %95s",
                        &ms, op, first, second);
        if (fields < 2 || ms > 600000ULL) {
            fprintf(stderr, "%s:%u: invalid sequence line\n", path, line_no);
            fclose(f);
            return 0;
        }
        us = (int64_t)ms * 1000LL;

        if (strcmp(op, "tap") == 0 && fields == 3) {
            const struct key_pair_ref *key = find_key_pair(first);
            if (!key || !tap(key->down, key->up, us))
                goto invalid_action;
        } else if (strcmp(op, "chord") == 0 && fields == 4) {
            const struct key_pair_ref *mod = find_key_pair(first);
            const struct key_pair_ref *key = find_key_pair(second);
            if (!mod || !key || !chord(
                    mod->down, key->down, key->up, mod->up, us))
                goto invalid_action;
        } else if (strcmp(op, "capture") == 0 && fields == 3) {
            struct seq_named_event *event = seq_named_event_new(s, first);
            if (!event || !later(seq_capture, event, us)) {
                free(event);
                goto invalid_action;
            }
        } else if (strcmp(op, "fail") == 0 && fields == 3) {
            struct seq_named_event *event = seq_named_event_new(s, first);
            if (!event || !later(seq_fail, event, us)) {
                free(event);
                goto invalid_action;
            }
        } else if (strcmp(op, "monitor") == 0 &&
                   (fields == 2 || fields == 3)) {
            unsigned long long timeout_ms = 0;
            struct seq_monitor_event *event = calloc(1, sizeof(*event));
            if (!event)
                goto invalid_action;
            event->state = s;
            event->timeout_us = DEFAULT_RUN_DEADLINE_US;
            if (fields == 3) {
                char *timeout_end = NULL;
                timeout_ms = strtoull(first, &timeout_end, 10);
                if (timeout_end == first || *timeout_end != '\0' ||
                    timeout_ms == 0 || timeout_ms > 600000ULL) {
                    free(event);
                    goto invalid_action;
                }
                event->timeout_us = (int64_t)timeout_ms * 1000LL;
            }
            if (!later(seq_monitor, event, us)) {
                free(event);
                goto invalid_action;
            }
        } else if (strcmp(op, "finish-install") == 0 && fields == 2) {
            if (!later(finish_install, s, us))
                goto invalid_action;
        } else {
invalid_action:
            fprintf(stderr, "%s:%u: invalid E2E action: %s",
                    path, line_no, line);
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}

static int parse_address(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (!text)
        return 0;
    parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || parsed > 0xfffffffful)
        return 0;
    *value = (uint32_t)parsed;
    return 1;
}

int main(int argc, char **argv)
{
    struct state s;
    const char *mode = getenv("HPCLANG_STOCK_MODE");
    const char *sequence = getenv("HPCLANG_E2E_SEQUENCE");
    const char *result_text = getenv("HPCLANG_STOCK_RESULT_ADDR");
    const char *lcd_height_text = getenv("HPCLANG_X50NG_LCD_HEIGHT");

    memset(&s, 0, sizeof(s));
    s.mode = mode && strcmp(mode, "run") == 0 ? MODE_RUN : MODE_INSTALL;
    s.prefix = getenv("HPCLANG_STOCK_PREFIX");
    s.json = getenv("HPCLANG_STOCK_JSON");
    if (!s.prefix) s.prefix = "stock";
    if (!s.json) s.json = "stock.json";
    s.lcd_height = lcd_height_text ? atoi(lcd_height_text) : 80;
    if (s.lcd_height != 64 && s.lcd_height != 80) {
        fprintf(stderr, "invalid stock LCD height\n");
        return EXIT_FAILURE;
    }

    if (s.mode == MODE_RUN) {
        s.before_pgm = getenv("HPCLANG_X50NG_BEFORE_PGM");
        s.pass_pgm = getenv("HPCLANG_X50NG_PASS_PGM");
        s.return_pgm = getenv("HPCLANG_X50NG_RETURN_PGM");
        s.runtime_json = getenv("HPCLANG_X50NG_E2E_JSON");
        if (!parse_address(result_text, &s.result_addr) ||
            !s.before_pgm || !s.pass_pgm || !s.return_pgm ||
            !s.runtime_json) {
            fprintf(stderr, "stock run mode needs canonical runtime outputs\n");
            return EXIT_FAILURE;
        }
    }

    config_t config = hpclang_x50ng_headless_config(argc, argv);
    s.hdw = emulator_init(&config);

    if (!sequence) {
        fprintf(stderr, "missing HPCLANG_E2E_SEQUENCE\n");
        return EXIT_FAILURE;
    }
    if (!schedule_sequence(&s, sequence)) {
        fprintf(stderr, "cannot schedule stock launcher sequence\n");
        return EXIT_FAILURE;
    }

    main_loop(s.hdw);
    save_modules(s.hdw);
    exit_modules(s.hdw);

    return s.failed ||
           (s.mode == MODE_RUN && (!s.saw_pass || !s.returned))
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
