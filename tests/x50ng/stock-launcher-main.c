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
#define LCD_H 80
#define KEY_HOLD_US 100000LL
#define PASS_WORD 0x50415353u
#define FAIL_PREFIX 0x46410000u
#define POLL_US 20000LL
#define RUN_DEADLINE_US 10000000LL

enum stock_mode {
    MODE_INSTALL,
    MODE_RUN,
};

struct state {
    hdw_t *hdw;
    enum stock_mode mode;
    const char *prefix;
    const char *json;
    uint32_t result_addr;
    int saw_pass;
    int failed;
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
KEY_PAIR(mode, HP4950_KEY_H);
KEY_PAIR(var, HP4950_KEY_J);
KEY_PAIR(eval, HP4950_KEY_N);
KEY_PAIR(nxt, HP4950_KEY_L);
KEY_PAIR(neg, HP4950_KEY_W);
KEY_PAIR(up, HP4950_KEY_UP);
KEY_PAIR(down, HP4950_KEY_DOWN);
KEY_PAIR(two, HP4950_KEY_2);
KEY_PAIR(period, HP4950_KEY_PERIOD);
KEY_PAIR(enter, HP4950_KEY_ENTER);
KEY_PAIR(on, HP4950_KEY_ON);
KEY_PAIR(lshift, HP4950_KEY_LEFTSHIFT);
KEY_PAIR(rshift, HP4950_KEY_RIGHTSHIFT);

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

static void capture_named(struct state *s, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s-%s.pgm", s->prefix, name);
    if (!capture(s->hdw, path))
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
        "  \"pc\":\"0x%08x\",\n"
        "  \"display_on\":%s,\n"
        "  \"sd_mounted\":%s\n"
        "}\n",
        s->mode == MODE_INSTALL ? "install" : "run",
        status,
        s->saw_pass ? "true" : "false",
        s->hdw->cpu->regs[15],
        is_display_on() ? "true" : "false",
        emulator_is_sd_mounted() ? "true" : "false");
    fclose(f);
}

static void fail(struct state *s, const char *why)
{
    s->failed = 1;
    fprintf(stderr, "stock launcher FAIL: %s\n", why);
    capture_named(s, "failure");
    write_json(s, why);
    hdw_stop(s->hdw);
}

static void copy_setup(void *arg)
{
    struct state *s = arg;
    if (!tap(&f2_down, &f2_up, 0))
        fail(s, "setup-copy-key");
}

static void select_copy_home(void *arg)
{
    struct state *s = arg;
    capture_named(s, "copy");
    if (!tap(&down_down, &down_up, 0) ||
        !tap(&f6_down, &f6_up, 300000))
        fail(s, "setup-copy-home-key");
}

static void capture_home_after_copy(void *arg)
{
    capture_named(arg, "home-after-copy");
}

static void open_vars(void *arg)
{
    struct state *s = arg;
    if (!tap(&var_down, &var_up, 0))
        fail(s, "setup-var-key");
}

static void run_setup_var(void *arg)
{
    struct state *s = arg;
    capture_named(s, "vars");
    if (!tap(&f1_down, &f1_up, 0))
        fail(s, "setup-var-f1");
}

static void open_files(void *arg)
{
    struct state *s = arg;
    if (!chord(&lshift_down, &files_down, &files_up, &lshift_up, 0))
        fail(s, "files-key");
}

static void enter_sd(void *arg)
{
    struct state *s = arg;
    if (!tap(&up_down, &up_up, 0) ||
        !tap(&f6_down, &f6_up, 300000))
        fail(s, "sd-key");
}

static void capture_files(void *arg)
{
    capture_named(arg, "files");
}

static void capture_sd(void *arg)
{
    capture_named(arg, "sd");
}

static void eval_setup(void *arg)
{
    struct state *s = arg;
    if (!tap(&f5_down, &f5_up, 0))
        fail(s, "setup-eval-key");
}

static void capture_setup_stack(void *arg)
{
    capture_named(arg, "setup-stack");
}

static void capture_setup_eval(void *arg)
{
    capture_named(arg, "setup-eval");
}

static void eval_home(void *arg)
{
    struct state *s = arg;
    if (!tap(&eval_down, &eval_up, 0))
        fail(s, "home-eval-key");
}

static void warmstart(void *arg)
{
    struct state *s = arg;
    if (!chord(&on_down, &f3_down, &f3_up, &on_up, 0))
        fail(s, "warmstart-key");
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

static void rcl_payload(void *arg)
{
    struct state *s = arg;
    if (!tap(&f4_down, &f4_up, 0))
        fail(s, "payload-rcl-key");
}

static void cancel_filer(void *arg)
{
    struct state *s = arg;
    if (!tap(&on_down, &on_up, 0))
        fail(s, "filer-cancel-key");
}

static void open_lib(void *arg)
{
    struct state *s = arg;
    if (!chord(&rshift_down, &two_down, &two_up, &rshift_up, 0))
        fail(s, "lib-key");
}

static void choose_port2(void *arg)
{
    struct state *s = arg;
    capture_named(s, "lib");
    if (!tap(&f3_down, &f3_up, 0))
        fail(s, "lib-port2-key");
}

static void choose_armtoolbox(void *arg)
{
    struct state *s = arg;
    capture_named(s, "port2");
    fail(s, "port2-after-real-install-probe");
}

static void run_prrun(void *arg)
{
    struct state *s = arg;
    capture_named(s, "armtoolbox");
    if (!tap(&f1_down, &f1_up, 0))
        fail(s, "prrun-key");
    else
        s->deadline = timer_get_clock() + RUN_DEADLINE_US;
}

static void finish_run(void *arg)
{
    struct state *s = arg;
    capture_named(s, "returned");
    if (!is_display_on()) {
        fail(s, "return-display");
        return;
    }
    write_json(s, "pass");
    hdw_stop(s->hdw);
}

static void monitor(void *arg)
{
    struct state *s = arg;
    uint32_t value = 0;

    if (s->deadline == 0) {
        if (!later(monitor, s, POLL_US))
            fail(s, "monitor-timer");
        return;
    }

    if (timer_get_clock() >= s->deadline) {
        fail(s, "timeout");
        return;
    }

    if (guest_read32(s->hdw, s->result_addr, &value)) {
        if (value == PASS_WORD) {
            s->saw_pass = 1;
            capture_named(s, "pass");
            fprintf(stderr, "stock ARMToolbox path reached HPC1 PASS\n");
            if (!tap(&on_down, &on_up, 700000) ||
                !later(finish_run, s, 1800000))
                fail(s, "return-timer");
            return;
        }
        if ((value & 0xffff0000u) == FAIL_PREFIX) {
            fprintf(stderr, "stock runtime failure 0x%08x\n", value);
            fail(s, "runtime-fail");
            return;
        }
    }

    if (!later(monitor, s, POLL_US))
        fail(s, "monitor-timer");
}

static int schedule_install(struct state *s)
{
    /*
     * Fresh stock 2.15 ROM: dismiss recovery/memory-clear, put install port 2
     * on the stack, copy SETUP.BIN from SD to HOME, then run it from VAR.
     */
    return tap(&f6_down, &f6_up, 2000000LL) &&
           tap(&f6_down, &f6_up, 4000000LL) &&
           tap(&mode_down, &mode_up, 5000000LL) &&
           tap(&neg_down, &neg_up, 5500000LL) &&
           tap(&f6_down, &f6_up, 6000000LL) &&
           tap(&two_down, &two_up, 7000000LL) &&
           tap(&period_down, &period_up, 7300000LL) &&
           tap(&enter_down, &enter_up, 7600000LL) &&
           later(open_files, s, 8300000LL) &&
           later(capture_files, s, 10500000LL) &&
           later(enter_sd, s, 10800000LL) &&
           later(capture_sd, s, 12800000LL) &&
           later(copy_setup, s, 13300000LL) &&
           later(select_copy_home, s, 14300000LL) &&
           later(cancel_filer, s, 16500000LL) &&
           later(capture_home_after_copy, s, 17500000LL) &&
           later(open_vars, s, 18000000LL) &&
           later(run_setup_var, s, 19500000LL) &&
           later(capture_setup_eval, s, 22000000LL) &&
           later(warmstart, s, 24500000LL) &&
           later(finish_install, s, 28000000LL);
}

static int schedule_run(struct state *s)
{
    /*
     * The preserved calculator already has ARMToolbox installed. SD contains
     * only HPCLANG.HP, so the File Manager selection is deterministic.
     * RCL it, then use the stock LIB -> ARMToolbox -> PrRUN path.
     */
    return later(open_files, s, 2000000LL) &&
           later(capture_files, s, 4200000LL) &&
           later(enter_sd, s, 4500000LL) &&
           later(capture_sd, s, 6500000LL) &&
           later(rcl_payload, s, 7000000LL) &&
           later(cancel_filer, s, 8200000LL) &&
           later(open_lib, s, 9000000LL) &&
           later(choose_port2, s, 10300000LL) &&
           later(choose_armtoolbox, s, 11600000LL) &&
           later(run_prrun, s, 13000000LL) &&
           later(monitor, s, 13200000LL);
}

int main(int argc, char **argv)
{
    struct state s;
    const char *mode = getenv("HPCLANG_STOCK_MODE");
    const char *result_text = getenv("HPCLANG_STOCK_RESULT_ADDR");
    char *end = NULL;

    memset(&s, 0, sizeof(s));
    s.mode = mode && strcmp(mode, "run") == 0 ? MODE_RUN : MODE_INSTALL;
    s.prefix = getenv("HPCLANG_STOCK_PREFIX");
    s.json = getenv("HPCLANG_STOCK_JSON");
    if (!s.prefix) s.prefix = "stock";
    if (!s.json) s.json = "stock.json";

    if (s.mode == MODE_RUN) {
        unsigned long value;
        if (!result_text) {
            fprintf(stderr, "stock run mode needs result address\n");
            return EXIT_FAILURE;
        }
        value = strtoul(result_text, &end, 0);
        if (end == result_text || *end != '\0' || value > 0xfffffffful) {
            fprintf(stderr, "invalid stock result address\n");
            return EXIT_FAILURE;
        }
        s.result_addr = (uint32_t)value;
    }

    config_t config = hpclang_x50ng_headless_config(argc, argv);
    s.hdw = emulator_init(&config);

    int ok = s.mode == MODE_INSTALL
        ? schedule_install(&s)
        : schedule_run(&s);
    if (!ok) {
        fprintf(stderr, "cannot schedule stock launcher actions\n");
        return EXIT_FAILURE;
    }

    main_loop(s.hdw);
    save_modules(s.hdw);
    exit_modules(s.hdw);

    return s.failed || (s.mode == MODE_RUN && !s.saw_pass)
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
