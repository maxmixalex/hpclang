#ifndef HPCLANG_X50NG_HEADLESS_UI_INIT_H
#define HPCLANG_X50NG_HEADLESS_UI_INIT_H

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emulator_api.h"
#include "options.h"
#include "ui4x/src/api.h"

/*
 * x50ng's core config and ui4x config are two separate parsing layers.
 * Headless hpclang harnesses do not start a frontend, but they still need
 * ui4x_init() so --datadir, --style and --sd-dir reach ui4x_config before
 * the hardware modules are loaded.
 */
static inline config_t hpclang_x50ng_headless_config(int argc, char **argv)
{
    char *progname = g_path_get_basename(argv[0]);
    char *progpath = g_path_get_dirname(argv[0]);
    char *default_datadir =
        g_build_filename(g_get_user_config_dir(), progname, NULL);

    config_t config = *config_init(argc, argv);

    ui4x_emulator_api_t emulator_api = {
        .press_key = press_key,
        .release_key = release_key,
        .is_key_pressed = is_key_pressed,
        .is_display_on = is_display_on,
        .get_annunciators = get_annunciators,
        .get_lcd_buffer = get_lcd_buffer,
        .get_contrast = get_contrast,
        .do_mount_sd = emulator_mount_sd,
        .do_unmount_sd = emulator_unmount_sd,
        .is_sd_mounted = emulator_is_sd_mounted,
        .get_sd_path = emulator_get_sd_path,
        .do_reset = emulator_hdw_reset,
        .do_stop = emulator_hdw_stop,
        .do_sleep = emulator_hdw_set_asleep,
        .do_wake = emulator_hdw_set_awake,
        .do_debug = emulator_debug,
    };

    ui4x_init(
        argc, argv, progname, progpath, default_datadir, &emulator_api);

    ui4x_config.model = MODEL_50G;
    ui4x_config.black_lcd = true;

    if (ui4x_config.name == NULL)
        ui4x_config.name = strdup(ui4x_config.progname);
    if (ui4x_config.style_filename == NULL)
        ui4x_config.style_filename = "themes/50g/style.css";

    return config;
}

/*
 * Temporary Stock-HP39 semantic probe.
 *
 * The production native-font path currently renders nonblank but incorrect
 * glyphs on both HP39gs and HP40gs. Do not infer another ROM offset from that
 * screenshot. Instead compare the exact P/A/S/5/1 records through the live
 * SAT_CPU Read[] mapping, the three bank views used by the original probe,
 * the old flat physical interpretation, and the current KINPO compact window.
 * This is host-only evidence and does not change the HPC1 payload or ABI.
 */
static inline int hpclang_font_probe_raw_read(
    void *cpu, uint32_t address, uint8_t *buffer, int length)
{
    return cpu_memory_rw_debug(cpu, address, buffer, length, 0);
}

static inline int hpclang_font_probe_raw_read32(
    void *cpu, uint32_t address, uint32_t *value)
{
    uint8_t b[4];
    if (hpclang_font_probe_raw_read(cpu, address, b, 4) != 0)
        return 0;
    *value = (uint32_t)b[0] |
             ((uint32_t)b[1] << 8) |
             ((uint32_t)b[2] << 16) |
             ((uint32_t)b[3] << 24);
    return 1;
}

static inline int hpclang_font_probe_page_nibble(
    void *cpu, uint32_t page, uint32_t saturn_address, uint8_t *value)
{
    uint8_t byte;
    uint32_t arm_address = page + ((saturn_address & 0xfffu) >> 1);
    if (hpclang_font_probe_raw_read(cpu, arm_address, &byte, 1) != 0)
        return 0;
    *value = (saturn_address & 1u) != 0u
        ? (uint8_t)(byte >> 4)
        : (uint8_t)(byte & 0x0fu);
    return 1;
}

static inline int hpclang_font_probe_flat_nibble(
    void *cpu, uint32_t saturn_address, uint8_t *value)
{
    uint8_t byte;
    uint32_t arm_address = saturn_address >> 1;
    if (hpclang_font_probe_raw_read(cpu, arm_address, &byte, 1) != 0)
        return 0;
    *value = (saturn_address & 1u) != 0u
        ? (uint8_t)(byte >> 4)
        : (uint8_t)(byte & 0x0fu);
    return 1;
}

static inline int hpclang_font_probe_window_nibble(
    void *cpu, uint32_t saturn_address, uint8_t *value)
{
    enum {
        HPCLANG_PROBE_CODE_WINDOW_BASE = 0x01200000u,
        HPCLANG_PROBE_UPDATE_ROM_OFFSET = 0x00004010u,
        HPCLANG_PROBE_FONT_PHYS_BASE = 0x00042000u,
        HPCLANG_PROBE_FONT_WINDOW_BYTES = 0x00002000u,
    };
    uint32_t byte_offset =
        HPCLANG_PROBE_UPDATE_ROM_OFFSET + (saturn_address >> 1);
    uint8_t byte;

    if (byte_offset < HPCLANG_PROBE_FONT_PHYS_BASE ||
        byte_offset >= HPCLANG_PROBE_FONT_PHYS_BASE +
            HPCLANG_PROBE_FONT_WINDOW_BYTES)
        return 0;

    uint32_t arm_address = HPCLANG_PROBE_CODE_WINDOW_BASE +
        (byte_offset - HPCLANG_PROBE_FONT_PHYS_BASE);
    if (hpclang_font_probe_raw_read(cpu, arm_address, &byte, 1) != 0)
        return 0;
    *value = (saturn_address & 1u) != 0u
        ? (uint8_t)(byte >> 4)
        : (uint8_t)(byte & 0x0fu);
    return 1;
}

typedef int (*hpclang_font_probe_nibble_fn)(
    void *cpu, uint32_t context, uint32_t saturn_address, uint8_t *value);

static inline int hpclang_font_probe_page_adapter(
    void *cpu, uint32_t page, uint32_t saturn_address, uint8_t *value)
{
    return hpclang_font_probe_page_nibble(cpu, page, saturn_address, value);
}

static inline int hpclang_font_probe_flat_adapter(
    void *cpu, uint32_t unused, uint32_t saturn_address, uint8_t *value)
{
    (void)unused;
    return hpclang_font_probe_flat_nibble(cpu, saturn_address, value);
}

static inline int hpclang_font_probe_window_adapter(
    void *cpu, uint32_t unused, uint32_t saturn_address, uint8_t *value)
{
    (void)unused;
    return hpclang_font_probe_window_nibble(cpu, saturn_address, value);
}

static inline int hpclang_font_probe_records(
    void *cpu,
    hpclang_font_probe_nibble_fn nibble,
    uint32_t context,
    uint32_t first,
    uint32_t stride,
    uint32_t record_nibbles,
    char records[5][17])
{
    static const char glyphs[5] = { 'P', 'A', 'S', '5', '1' };

    if (record_nibbles > 16u)
        return 0;
    for (size_t i = 0; i < 5u; ++i) {
        uint32_t record = first + (uint32_t)(uint8_t)glyphs[i] * stride;
        for (uint32_t n = 0; n < record_nibbles; ++n) {
            uint8_t value;
            if (!nibble(cpu, context, record + n, &value))
                return 0;
            records[i][n] = "0123456789abcdef"[value & 0x0fu];
        }
        records[i][record_nibbles] = '\0';
    }
    return 1;
}

static inline void hpclang_font_probe_print_records(
    const char *label, char records[5][17])
{
    fprintf(stderr, "%s[P,A,S,5,1]=%s,%s,%s,%s,%s ",
            label,
            records[0], records[1], records[2], records[3], records[4]);
}

static inline void hpclang_font_probe_stock_hp39(void *cpu)
{
    static int emitted;
    const char *stock_mode;
    const char *target;
    uint32_t model;
    uint32_t saturn_state;
    uint32_t cookie;
    uint32_t read_7c;
    uint32_t read_7e;
    char live_main[5][17], live_mini[5][17];
    char bank0_main[5][17], bank0_mini[5][17];
    char bank1_main[5][17], bank1_mini[5][17];
    char bank2_main[5][17], bank2_mini[5][17];
    char flat_main[5][17], flat_mini[5][17];
    char window_main[5][17], window_mini[5][17];

    if (emitted)
        return;
    stock_mode = getenv("HPCLANG_STOCK_MODE");
    target = getenv("TARGET");
    if (stock_mode == NULL || strcmp(stock_mode, "run") != 0 ||
        target == NULL ||
        (strcmp(target, "hp39gs") != 0 && strcmp(target, "hp40gs") != 0))
        return;

    if (!hpclang_font_probe_raw_read32(cpu, 0x01100000u, &model) ||
        (model != 2u && model != 3u) ||
        !hpclang_font_probe_raw_read32(cpu, 0x01100008u, &saturn_state) ||
        saturn_state == 0u ||
        !hpclang_font_probe_raw_read32(cpu, 0x0110000cu, &cookie) ||
        !hpclang_font_probe_raw_read32(cpu, saturn_state + 0x7cu * 4u, &read_7c) ||
        !hpclang_font_probe_raw_read32(cpu, saturn_state + 0x7eu * 4u, &read_7e) ||
        read_7c == 0u || read_7e == 0u)
        return;

    if (!hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            read_7c, 0x07c651u, 16u, 16u, live_main) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            read_7e, 0x07e695u, 11u, 11u, live_mini) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            0x0005e000u, 0x07c651u, 16u, 16u, bank0_main) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            0x0005f000u, 0x07e695u, 11u, 11u, bank0_mini) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            0x0007e000u, 0x07c651u, 16u, 16u, bank1_main) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            0x0007f000u, 0x07e695u, 11u, 11u, bank1_mini) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            0x0009e000u, 0x07c651u, 16u, 16u, bank2_main) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_page_adapter,
            0x0009f000u, 0x07e695u, 11u, 11u, bank2_mini) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_flat_adapter,
            0u, 0x07c651u, 16u, 16u, flat_main) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_flat_adapter,
            0u, 0x07e695u, 11u, 11u, flat_mini) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_window_adapter,
            0u, 0x07c651u, 16u, 16u, window_main) ||
        !hpclang_font_probe_records(cpu, hpclang_font_probe_window_adapter,
            0u, 0x07e695u, 11u, 11u, window_mini))
        return;

    emitted = 1;
    fprintf(stderr,
            "HP39 FONT SEMANTIC PROBE target=%s model=%u saturn_state=%08x "
            "cookie=%08x Read[7c]=%08x Read[7e]=%08x\n",
            target, model, saturn_state, cookie, read_7c, read_7e);
    hpclang_font_probe_print_records("live-main", live_main);
    hpclang_font_probe_print_records("live-mini", live_mini);
    fprintf(stderr, "\n");
    hpclang_font_probe_print_records("bank0-main", bank0_main);
    hpclang_font_probe_print_records("bank0-mini", bank0_mini);
    fprintf(stderr, "\n");
    hpclang_font_probe_print_records("bank1-main", bank1_main);
    hpclang_font_probe_print_records("bank1-mini", bank1_mini);
    fprintf(stderr, "\n");
    hpclang_font_probe_print_records("bank2-main", bank2_main);
    hpclang_font_probe_print_records("bank2-mini", bank2_mini);
    fprintf(stderr, "\n");
    hpclang_font_probe_print_records("flat-main", flat_main);
    hpclang_font_probe_print_records("flat-mini", flat_mini);
    fprintf(stderr, "\n");
    hpclang_font_probe_print_records("window-main", window_main);
    hpclang_font_probe_print_records("window-mini", window_mini);
    fprintf(stderr, "\n");
}

static inline int hpclang_font_probe_cpu_memory_rw_debug(
    void *cpu,
    uint32_t address,
    uint8_t *buffer,
    int length,
    int is_write)
{
    int result = cpu_memory_rw_debug(cpu, address, buffer, length, is_write);
    if (!is_write && result == 0)
        hpclang_font_probe_stock_hp39(cpu);
    return result;
}

#define cpu_memory_rw_debug hpclang_font_probe_cpu_memory_rw_debug

#endif
