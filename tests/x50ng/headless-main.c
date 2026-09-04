#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emulator_api.h"
#include "hdw.h"
#include "module.h"
#include "options.h"
#include "timer.h"
#include "ui4x/src/api.h"
#include "s3c2410/s3c2410_lcd.h"

#define HPCLANG_LCD_WIDTH 131
#define HPCLANG_LCD_HEIGHT 80
#define RECOVERY_NO_PRESS_DELAY_US 2000000LL
#define MEMORY_CLEAR_OK_PRESS_DELAY_US 4000000LL
#define BOOT_KEY_HOLD_US 100000LL
#define FIRST_CAPTURE_DELAY_US 8000000LL
#define RETRY_DELAY_US 1000000LL
#define MAX_CAPTURE_ATTEMPTS 8

struct capture_state {
    hdw_t *hdw;
    const char *pgm_path;
    const char *json_path;
    int attempts;
};

struct key_action {
    int hpkey;
    int pressed;
};

static int read_guest_u32(hdw_t *hdw, uint32_t address, uint32_t *value)
{
    uint8_t bytes[4];

    if (cpu_memory_rw_debug(hdw->cpu, address, bytes, sizeof(bytes), 0) != 0)
        return 0;

    *value =
        (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
    return 1;
}

static int write_pgm(
    const char *path,
    const uint8_t pixels[HPCLANG_LCD_HEIGHT][HPCLANG_LCD_WIDTH])
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return 0;

    fprintf(f, "P5\n%d %d\n255\n", HPCLANG_LCD_WIDTH, HPCLANG_LCD_HEIGHT);
    if (fwrite(
            pixels,
            1,
            HPCLANG_LCD_WIDTH * HPCLANG_LCD_HEIGHT,
            f) != HPCLANG_LCD_WIDTH * HPCLANG_LCD_HEIGHT) {
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

static void set_key_from_timer(void *opaque)
{
    const struct key_action *action = opaque;

    if (action->pressed)
        press_key(action->hpkey);
    else
        release_key(action->hpkey);
}

static int schedule_key_tap(
    const struct key_action *down,
    const struct key_action *up,
    int64_t delay_us)
{
    hdw_timer_t *key_down = timer_new(
        HDW_TIMER_REALTIME, set_key_from_timer, (void *)down);
    hdw_timer_t *key_up = timer_new(
        HDW_TIMER_REALTIME, set_key_from_timer, (void *)up);

    if (key_down == NULL || key_up == NULL)
        return 0;

    timer_mod(key_down, timer_get_clock() + delay_us);
    timer_mod(key_up, timer_get_clock() + delay_us + BOOT_KEY_HOLD_US);
    return 1;
}

static void capture_and_stop(void *opaque)
{
    struct capture_state *state = opaque;
    s3c2410_lcd_t *lcd = state->hdw->s3c2410_lcd;
    uint8_t pixels[HPCLANG_LCD_HEIGHT][HPCLANG_LCD_WIDTH];
    unsigned nonzero = 0;

    ++state->attempts;

    for (int y = 0; y < HPCLANG_LCD_HEIGHT; ++y) {
        for (int x = 0; x < HPCLANG_LCD_WIDTH; ++x) {
            int level = s3c2410_get_pixel_color(lcd, x, y);
            if (level < 0)
                level = 0;
            if (level > 15)
                level = 15;
            if (level != 0)
                ++nonzero;

            pixels[y][x] = (uint8_t)(255 - level * 17);
        }
    }

    if (nonzero == 0 && state->attempts < MAX_CAPTURE_ATTEMPTS) {
        hdw_timer_t *retry = timer_new(
            HDW_TIMER_REALTIME, capture_and_stop, state);
        if (retry != NULL) {
            timer_mod(retry, timer_get_clock() + RETRY_DELAY_US);
            return;
        }
    }

    if (!write_pgm(state->pgm_path, pixels)) {
        fprintf(stderr, "cannot write %s\n", state->pgm_path);
        exit(EXIT_FAILURE);
    }

    FILE *json = fopen(state->json_path, "w");
    if (json == NULL) {
        fprintf(stderr, "cannot write %s\n", state->json_path);
        exit(EXIT_FAILURE);
    }

    uint32_t l1_data = 0;
    uint32_t l1_code = 0;
    uint32_t l1_stage = 0;
    int l1_data_ok = read_guest_u32(state->hdw, 0x07f00044u, &l1_data);
    int l1_code_ok = read_guest_u32(state->hdw, 0x07f00048u, &l1_code);
    int l1_stage_ok = read_guest_u32(state->hdw, 0x07f00240u, &l1_stage);

    target_phys_addr_t l1_phys =
        cpu_get_phys_page_debug(state->hdw->cpu, 0x07f00000u);
    target_phys_addr_t sram_probe_phys =
        cpu_get_phys_page_debug(state->hdw->cpu, 0x08040000u);
    target_phys_addr_t stage_window_phys =
        cpu_get_phys_page_debug(state->hdw->cpu, 0x09000000u);

    fprintf(
        json,
        "{\n"
        "  \"pc\": \"0x%08x\",\n"
        "  \"display_on\": %s,\n"
        "  \"nonzero_pixels\": %u,\n"
        "  \"capture_attempts\": %d,\n"
        "  \"width\": %d,\n"
        "  \"height\": %d,\n"
        "  \"sctlr\": \"0x%08x\",\n"
        "  \"ttbr0\": \"0x%08x\",\n"
        "  \"l1_page_phys\": \"0x%08llx\",\n"
        "  \"l1_data_entry\": \"0x%08x\",\n"
        "  \"l1_code_entry\": \"0x%08x\",\n"
        "  \"l1_stage_entry\": \"0x%08x\",\n"
        "  \"sram_08040000_phys\": \"0x%08llx\",\n"
        "  \"stage_09000000_phys\": \"0x%08llx\"\n"
        "}\n",
        state->hdw->cpu->regs[15],
        is_display_on() ? "true" : "false",
        nonzero,
        state->attempts,
        HPCLANG_LCD_WIDTH,
        HPCLANG_LCD_HEIGHT,
        state->hdw->cpu->cp15.c1_sys,
        state->hdw->cpu->cp15.c2_base0,
        (unsigned long long)l1_phys,
        l1_data_ok ? l1_data : 0xffffffffu,
        l1_code_ok ? l1_code : 0xffffffffu,
        l1_stage_ok ? l1_stage : 0xffffffffu,
        (unsigned long long)sram_probe_phys,
        (unsigned long long)stage_window_phys);

    fclose(json);

    if (!is_display_on() || nonzero == 0) {
        fprintf(
            stderr,
            "x50ng ARM boot did not produce a live LCD "
            "(display=%d nonzero=%u)\n",
            is_display_on(),
            nonzero);
        exit(EXIT_FAILURE);
    }

    hdw_stop(state->hdw);
}

int main(int argc, char **argv)
{
    const char *pgm_path = getenv("HPCLANG_X50NG_PGM");
    const char *json_path = getenv("HPCLANG_X50NG_JSON");

    if (pgm_path == NULL)
        pgm_path = "x50ng-lcd.pgm";
    if (json_path == NULL)
        json_path = "x50ng-state.json";

    config_t config = *config_init(argc, argv);
    hdw_t *hdw = emulator_init(&config);

    struct capture_state capture = {
        .hdw = hdw,
        .pgm_path = pgm_path,
        .json_path = json_path,
        .attempts = 0,
    };
    static const struct key_action recovery_no_down = {
        .hpkey = HP4950_KEY_F,
        .pressed = 1,
    };
    static const struct key_action recovery_no_up = {
        .hpkey = HP4950_KEY_F,
        .pressed = 0,
    };

    /*
     * A brand-new x50ng state boots the stock ROM at "Try To Recover Memory?".
     * The right-most softkey is NO, i.e. physical F. Drive the calculator key
     * matrix directly; no host-window coordinates are involved.
     */
    if (!schedule_key_tap(
            &recovery_no_down,
            &recovery_no_up,
            RECOVERY_NO_PRESS_DELAY_US) ||
        !schedule_key_tap(
            &recovery_no_down,
            &recovery_no_up,
            MEMORY_CLEAR_OK_PRESS_DELAY_US)) {
        fprintf(stderr, "cannot allocate boot key timers\n");
        return EXIT_FAILURE;
    }

    hdw_timer_t *timer = timer_new(
        HDW_TIMER_REALTIME, capture_and_stop, &capture);
    if (timer == NULL) {
        fprintf(stderr, "cannot allocate headless capture timer\n");
        return EXIT_FAILURE;
    }

    timer_mod(timer, timer_get_clock() + FIRST_CAPTURE_DELAY_US);
    main_loop(hdw);

    exit_modules(hdw);
    return EXIT_SUCCESS;
}
