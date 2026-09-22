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
#include "lcd-quiescence.h"
#include "stock-return-phase.h"
#include "s3c2410/s3c2410_lcd.h"

#define LCD_W 131
#define KEY_HOLD_US 100000LL
#define PAYLOAD_KEY_HOLD_US 50000LL
#define PAYLOAD_KEY_FIRST_DELAY_US 20000LL
#define PASS_WORD 0x50415353u
#define FAIL_PREFIX 0x46410000u
#define PHASE_KEYBOARD_READY 0x4b425244u
#define PHASE_KEYBOARD_ACK 0x4b410000u
#define PHASE_OBJECT_STORE 0x4f530000u
#define POLL_US 20000LL
#define DEFAULT_RUN_DEADLINE_US 10000000LL
#define RETURN_POLL_US 50000LL
#define RETURN_DEADLINE_US 2000000LL
#define RETURN_REPAINT_DELAY_US 20000LL
#define HP48_RETURN_REPAINT_DELAY_US 250000LL
#define RETURN_REPAINT_SETTLE_US 180000LL
#define RETURN_STABLE_OBSERVATIONS 4u

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
    struct hpclang_lcd_quiescence return_quiescence;
    int saw_before;
    int saw_pass;
    uint32_t object_store_phase;
    int payload_keyboard_scheduled;
    size_t payload_keyboard_sent;
    size_t payload_keyboard_captured;
    int return_mapping_restored;
    int stage_baseline_valid;
    uint32_t stage_baseline_descriptor;
    int hp48_stage_alias_seen;
    uint32_t hp48_stage_alias_descriptor;
    int hp48_stage_alias_cleared;
    int return_repaint_scheduled;
    int returned;
    int failed;
    int lcd_height;
    int64_t deadline;
    int64_t return_teardown_deadline;
    int64_t return_deadline;
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

static int is_hp48_target(void)
{
    const char *target = getenv("TARGET");
    return target != NULL && strcmp(target, "hp48gii") == 0;
}

static int is_hp4950_target(void)
{
    const char *target = getenv("TARGET");
    return target != NULL &&
           (strcmp(target, "hp49gplus") == 0 ||
            strcmp(target, "hp50g") == 0);
}

static int uses_armtoolbox_stage_alias(void)
{
    return is_hp48_target() || is_hp4950_target();
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

/*
 * Stock sequence files pre-schedule sparse ENTER retries so slow serial-disk
 * transfers can still launch ARMToolbox. Once the HPC1 payload publishes KBRD,
 * those launcher retries must no longer inject key-down edges into the strict
 * 51-key payload test. Always allow key-up so a key already pressed before
 * KBRD can still be released.
 */
struct sequence_key_event {
    struct state *state;
    const struct key_action *action;
};

static void sequence_key_cb(void *arg)
{
    struct sequence_key_event *event = arg;
    const struct key_action *action = event->action;
    struct state *s = event->state;

    if (!action->down || !s->payload_keyboard_scheduled)
        key_cb((void *)action);
    free(event);
}

static int sequence_key(
    struct state *s,
    const struct key_action *action,
    int64_t us)
{
    struct sequence_key_event *event = calloc(1, sizeof(*event));
    if (!event)
        return 0;
    event->state = s;
    event->action = action;
    if (!later(sequence_key_cb, event, us)) {
        free(event);
        return 0;
    }
    return 1;
}

static int sequence_tap(
    struct state *s,
    const struct key_action *down,
    const struct key_action *up_action,
    int64_t us)
{
    return sequence_key(s, down, us) &&
           sequence_key(s, up_action, us + KEY_HOLD_US);
}

static int sequence_chord(
    struct state *s,
    const struct key_action *mod_down,
    const struct key_action *key_down,
    const struct key_action *key_up,
    const struct key_action *mod_up,
    int64_t us)
{
    return sequence_key(s, mod_down, us) &&
           sequence_key(s, key_down, us + 30000) &&
           sequence_key(s, key_up, us + 130000) &&
           sequence_key(s, mod_up, us + 160000);
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

static int schedule_return_repaint(struct state *s)
{
    const char *target;
    const struct key_action *down_action;
    const struct key_action *up_action;
    const char *name;
    int64_t delay = RETURN_REPAINT_DELAY_US;

    if (s->return_repaint_scheduled)
        return 1;

    target = getenv("TARGET");
    if (target != NULL &&
        (strcmp(target, "hp39gs") == 0 || strcmp(target, "hp40gs") == 0)) {
        down_action = &aplet_down;
        up_action = &aplet_up;
        name = "APLET";
    } else {
        down_action = &var_down;
        up_action = &var_up;
        name = "VAR";
        /*
         * HP48gII/49g+/50g wait until the stage0-owned L1 alias descriptor is
         * no longer present before this function is reached. Keep a small
         * post-teardown delay so the optional KOS release and final launcher
         * stack pop can complete.
         */
        if (uses_armtoolbox_stage_alias())
            delay = HP48_RETURN_REPAINT_DELAY_US;
    }

    if (!tap(down_action, up_action, delay))
        return 0;

    s->return_repaint_scheduled = 1;
    fprintf(stderr,
            "stock launcher: scheduled native %s repaint after %lld us\n",
            name, (long long)delay);
    return 1;
}

static int schedule_payload_keyboard_key(struct state *s, size_t index)
{
    size_t count = sizeof(payload_keyboard_codes) /
                   sizeof(payload_keyboard_codes[0]);
    if (index >= count)
        return 0;

    payload_keyboard_down[index].key = payload_keyboard_codes[index];
    payload_keyboard_down[index].down = 1;
    payload_keyboard_up[index].key = payload_keyboard_codes[index];
    payload_keyboard_up[index].down = 0;

    if (!later(
            key_cb, &payload_keyboard_down[index],
            PAYLOAD_KEY_FIRST_DELAY_US) ||
        !later(
            key_cb, &payload_keyboard_up[index],
            PAYLOAD_KEY_FIRST_DELAY_US + PAYLOAD_KEY_HOLD_US))
        return 0;

    s->payload_keyboard_sent = index + 1u;
    return 1;
}

static int schedule_payload_keyboard(struct state *s)
{
    size_t count = sizeof(payload_keyboard_codes) /
                   sizeof(payload_keyboard_codes[0]);

    if (s->payload_keyboard_scheduled)
        return 1;
    if (count == 0u || !schedule_payload_keyboard_key(s, 0u))
        return 0;

    s->payload_keyboard_scheduled = 1;
    fprintf(stderr,
            "stock launcher: payload keyboard phase reached; started "
            "%zu-key acknowledged sequence\n",
            count);
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

static int capture_keyboard_progress(struct state *s, size_t index)
{
    const char *dir = getenv("HPCLANG_X50NG_KEYBOARD_DIR");
    char path[512];

    if (dir == NULL || *dir == '\0')
        return 1;

    snprintf(path, sizeof(path), "%s/%02zu.pgm", dir, index);
    if (!capture(s->hdw, path, s->lcd_height)) {
        fprintf(stderr,
                "stock launcher: blank keyboard progress capture %zu\n",
                index);
        return 0;
    }
    return 1;
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

static int armtoolbox_capture_stage_baseline(struct state *s)
{
    uint32_t stage_slot = 0;

    if (!uses_armtoolbox_stage_alias() || s->stage_baseline_valid)
        return 1;
    if (!guest_read32(
            s->hdw, HPCLANG_STOCK_STAGE_L1_SLOT, &stage_slot))
        return 0;

    s->stage_baseline_valid = 1;
    s->stage_baseline_descriptor = stage_slot;
    fprintf(stderr,
            "stock launcher: ARMToolbox stage baseline: 0x%08x\n",
            stage_slot);
    return 1;
}

static void armtoolbox_observe_stage_alias(struct state *s)
{
    uint32_t stage_slot = 0;

    if (!uses_armtoolbox_stage_alias() ||
        !s->stage_baseline_valid ||
        s->hp48_stage_alias_seen)
        return;
    if (!guest_read32(
            s->hdw, HPCLANG_STOCK_STAGE_L1_SLOT, &stage_slot))
        return;
    if (stage_slot == s->stage_baseline_descriptor ||
        (stage_slot & 3u) != 1u)
        return;

    s->hp48_stage_alias_seen = 1;
    s->hp48_stage_alias_descriptor = stage_slot;
    fprintf(stderr,
            "stock launcher: ARMToolbox stage alias active: 0x%08x\n",
            stage_slot);
}

static int return_is_settled(const struct state *s)
{
    return s->return_quiescence.changed_from_baseline &&
           s->return_quiescence.stable_observations >=
               RETURN_STABLE_OBSERVATIONS;
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
        "  \"return_settled\":%s,\n"
        "  \"return_observations\":%u,\n"
        "  \"pc\":\"0x%08x\",\n"
        "  \"display_on\":%s,\n"
        "  \"sd_mounted\":%s\n"
        "}\n",
        s->mode == MODE_INSTALL ? "install" : "run",
        status,
        s->saw_pass ? "true" : "false",
        s->payload_keyboard_scheduled ? "true" : "false",
        s->returned ? "true" : "false",
        return_is_settled(s) ? "true" : "false",
        s->return_quiescence.observations,
        s->hdw->cpu->regs[15],
        is_display_on() ? "true" : "false",
        emulator_is_sd_mounted() ? "true" : "false");
    fclose(f);
}

static void hpclang_log_cpu_snapshot(struct state *s, const char *tag)
{
    CPUARMState *cpu = s->hdw->cpu;
    fprintf(stderr,
            "stock launcher CPU %s: pc=0x%08x cpsr=0x%08x "
            "sp=0x%08x lr=0x%08x r4=0x%08x spsr=0x%08x\n",
            tag,
            cpu->regs[15],
            cpsr_read(cpu),
            cpu->regs[13],
            cpu->regs[14],
            cpu->regs[4],
            cpu->spsr);
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
        "  \"return_settled\":%s,\n"
        "  \"return_observations\":%u,\n"
        "  \"pc\":\"0x%08x\"\n"
        "}\n",
        status,
        s->saw_pass ? "true" : "false",
        s->payload_keyboard_scheduled ? "true" : "false",
        s->returned ? "true" : "false",
        return_is_settled(s) ? "true" : "false",
        s->return_quiescence.observations,
        s->hdw->cpu->regs[15]);
    fclose(f);
}

static void fail(struct state *s, const char *why)
{
    s->failed = 1;
    fprintf(stderr, "stock launcher FAIL: %s\n", why);
    if (s->object_store_phase != 0u) {
        fprintf(
            stderr,
            "stock launcher last object-store phase: OS%02u (0x%08x)\n",
            (unsigned)(s->object_store_phase & 0xffffu),
            s->object_store_phase);
    }
    hpclang_log_cpu_snapshot(s, "failure");
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

    if (!is_display_on()) {
        fail(s, "stock-resume");
        return;
    }

    if (hpclang_lcd_quiescence_observe(
            &s->return_quiescence,
            s->hdw,
            LCD_W,
            s->lcd_height,
            RETURN_STABLE_OBSERVATIONS)) {
        unsigned pixels = capture(
            s->hdw, s->return_pgm, s->lcd_height);
        if (pixels == 0u) {
            fail(s, "stock-resume");
            return;
        }
        s->returned = 1;
        write_runtime_json(s, "pass");
        write_json(s, "pass");
        hdw_stop(s->hdw);
        return;
    }

    if (timer_get_clock() >= s->return_deadline) {
        fail(s, "return-unsettled");
        return;
    }
    if (!later(finish_return, s, RETURN_POLL_US))
        fail(s, "return-timer");
}

static int armtoolbox_wait_for_stage_teardown(struct state *s)
{
    uint32_t stage_slot = 0;
    int readable = guest_read32(
        s->hdw, HPCLANG_STOCK_STAGE_L1_SLOT, &stage_slot);

    if (!readable)
        return 0;

    if (s->stage_baseline_valid &&
        stage_slot == s->stage_baseline_descriptor) {
        if (!s->hp48_stage_alias_cleared) {
            s->hp48_stage_alias_cleared = 1;
            if (s->hp48_stage_alias_seen) {
                fprintf(stderr,
                        "stock launcher: ARMToolbox stage alias released: "
                        "0x%08x -> 0x%08x; continuing with Stock UI witness\n",
                        s->hp48_stage_alias_descriptor, stage_slot);
            } else {
                fprintf(stderr,
                        "stock launcher: ARMToolbox stage alias already "
                        "released at baseline 0x%08x; continuing with "
                        "Stock UI witness\n",
                        stage_slot);
            }
        }
        return 1;
    }

    if (!s->hp48_stage_alias_seen) {
        if ((stage_slot & 3u) != 1u)
            return 0;

        s->hp48_stage_alias_seen = 1;
        s->hp48_stage_alias_descriptor = stage_slot;
        fprintf(stderr,
                "stock launcher: ARMToolbox stage alias active: 0x%08x\n",
                stage_slot);
        return 0;
    }

    if (stage_slot == s->hp48_stage_alias_descriptor)
        return 0;

    if (!s->hp48_stage_alias_cleared) {
        s->hp48_stage_alias_cleared = 1;
        fprintf(stderr,
                "stock launcher: ARMToolbox stage alias released: "
                "0x%08x -> 0x%08x; continuing with Stock UI witness\n",
                s->hp48_stage_alias_descriptor, stage_slot);
    }
    return 1;
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

    armtoolbox_observe_stage_alias(s);
    readable = guest_read32(s->hdw, s->result_addr, &value);

    if (!s->payload_keyboard_scheduled && readable &&
        value == PHASE_KEYBOARD_READY) {
        if (!capture_keyboard_progress(s, 0u)) {
            fail(s, "keyboard-capture");
            return;
        }
        if (!schedule_payload_keyboard(s)) {
            fail(s, "keyboard-schedule");
            return;
        }
    } else if (s->payload_keyboard_scheduled && readable &&
               (value & 0xffff0000u) == PHASE_KEYBOARD_ACK) {
        size_t count = sizeof(payload_keyboard_codes) /
                       sizeof(payload_keyboard_codes[0]);
        size_t ack = (size_t)(value & 0xffffu);
        if (ack == 0u || ack > count || ack > s->payload_keyboard_sent) {
            fail(s, "keyboard-ack");
            return;
        }
        if (ack > s->payload_keyboard_captured) {
            if (!capture_keyboard_progress(s, ack)) {
                fail(s, "keyboard-capture");
                return;
            }
            s->payload_keyboard_captured = ack;
        }
        if (ack == s->payload_keyboard_sent && ack < count) {
            if (!schedule_payload_keyboard_key(s, ack)) {
                fail(s, "keyboard-schedule");
                return;
            }
        }
    }

    if (readable) {
        if ((value & 0xffff0000u) == PHASE_OBJECT_STORE &&
            value != s->object_store_phase) {
            s->object_store_phase = value;
            fprintf(
                stderr,
                "stock object-store phase OS%02u (0x%08x)\n",
                (unsigned)(value & 0xffffu),
                value);
        }

        if (!s->saw_pass && value == PASS_WORD) {
            size_t keyboard_count = sizeof(payload_keyboard_codes) /
                                    sizeof(payload_keyboard_codes[0]);
            s->saw_pass = 1;
            /*
             * KACK(51) can be replaced by PASS between monitor polls. In that
             * case PASS itself is the authoritative fully accepted keyboard
             * frame, so use it to complete the 0..51 progression sequence.
             */
            if (s->payload_keyboard_scheduled &&
                s->payload_keyboard_captured < keyboard_count) {
                if (!capture_keyboard_progress(s, keyboard_count)) {
                    fail(s, "keyboard-capture");
                    return;
                }
                s->payload_keyboard_captured = keyboard_count;
            }
            if (!capture(s->hdw, s->pass_pgm, s->lcd_height)) {
                fail(s, "pass-capture");
                return;
            }
            hpclang_lcd_quiescence_seed(
                &s->return_quiescence, s->hdw, LCD_W, s->lcd_height);
            fprintf(stderr, "stock ARMToolbox path reached HPC1 PASS\n");
            hpclang_log_cpu_snapshot(s, "pass");
            write_runtime_json(s, "pass-observed");
            if (uses_armtoolbox_stage_alias())
                (void)armtoolbox_wait_for_stage_teardown(s);
        } else if ((value & 0xffff0000u) == FAIL_PREFIX) {
            fprintf(stderr, "stock runtime failure 0x%08x\n", value);
            fail(s, "runtime-fail");
            return;
        }
    }

    /*
     * hpclang_e2e_result lives in hpclang's temporary runtime mapping. After
     * PASS, disappearance/change of that word proves only that the common
     * bootstrap restored the stock MMU. HP48gII, HP49g+ and HP50g still have
     * to return through stage1 and stage0; injecting a key during that teardown
     * can trip the stock launcher. The monitor snapshots the pre-PrRUN Stock
     * L1[0x90] descriptor and observes the stage0 coarse alias while the
     * payload runs. After PASS, returning to that baseline or otherwise leaving
     * the observed alias proves that stage0 no longer owns the slot. Stock may
     * immediately reuse/remap the address space, so a transient literal zero is
     * not required.
     *
     * On hp4950 the restored Stock address space can leave the old result
     * virtual address readable with the same PASS word. The stage0 ownership
     * transition is therefore the authoritative control-flow-return witness;
     * result-word disappearance is not required there. HP39 keeps its existing
     * result-word gate. Visible framebuffer change and quiescence remain the
     * final return-to-Stock criterion for every family.
     */
    int result_mapping_left_pass =
        s->saw_pass && (!readable || value != PASS_WORD);
    int hp4950_stage_released =
        s->saw_pass && is_hp4950_target() &&
        armtoolbox_wait_for_stage_teardown(s);

    if (s->saw_pass &&
        ((is_hp4950_target() && hp4950_stage_released) ||
         (!is_hp4950_target() && result_mapping_left_pass))) {
        if (is_hp48_target()) {
            if (!s->return_mapping_restored) {
                s->return_mapping_restored = 1;
                s->return_teardown_deadline = timer_get_clock() +
                    HPCLANG_STOCK_TEARDOWN_DEADLINE_US;
                fprintf(stderr,
                        "stock launcher: hpclang mapping restored; "
                        "waiting for HP48 stage0 teardown\n");
            }

            if (!armtoolbox_wait_for_stage_teardown(s)) {
                if (timer_get_clock() >= s->return_teardown_deadline) {
                    fail(s, "return-teardown");
                    return;
                }
                if (!later(monitor, s, POLL_US))
                    fail(s, "monitor-timer");
                return;
            }
        }

        if (!schedule_return_repaint(s)) {
            fail(s, "return-repaint-timer");
            return;
        }
        s->return_deadline = timer_get_clock() + RETURN_DEADLINE_US;
        if (!later(finish_return, s, RETURN_REPAINT_SETTLE_US))
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

    if (!armtoolbox_capture_stage_baseline(s)) {
        free(event);
        fail(s, "stage-baseline");
        return;
    }

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
            if (!key || !sequence_tap(s, key->down, key->up, us))
                goto invalid_action;
        } else if (strcmp(op, "chord") == 0 && fields == 4) {
            const struct key_pair_ref *mod = find_key_pair(first);
            const struct key_pair_ref *key = find_key_pair(second);
            if (!mod || !key || !sequence_chord(
                    s, mod->down, key->down, key->up, mod->up, us))
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
           (s.mode == MODE_RUN &&
            (!s.saw_pass || !s.returned || !return_is_settled(&s)))
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
