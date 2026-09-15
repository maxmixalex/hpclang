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
 * Temporary Stock-HP39 guest-view probe.
 *
 * The state/flash scan proves that the native font tables really occupy the
 * physical NOR pages selected by stage1. This probe answers the next question:
 * what does the running HPC1 address space actually see through the private
 * 0x01200000 code-window alias after the bootstrap installs its coarse table?
 *
 * cpu_memory_rw_debug is intentionally used here as a virtual/debug read. It
 * is triggered only while the Stock Gallery result word reports a live Gallery
 * state, so the bootstrap MMU mappings are still installed. No guest or public
 * ABI state is modified.
 */
static inline int hpclang_hp39_alias_read_nibble(
    void *cpu, uint32_t window_nibble, uint8_t *value)
{
    enum { HPCLANG_PROBE_CODE_WINDOW_BASE = 0x01200000u };
    uint8_t byte;
    uint32_t address = HPCLANG_PROBE_CODE_WINDOW_BASE + (window_nibble >> 1);
    if (cpu_memory_rw_debug(cpu, address, &byte, 1, 0) != 0)
        return 0;
    *value = (window_nibble & 1u) != 0u
        ? (uint8_t)(byte >> 4)
        : (uint8_t)(byte & 0x0fu);
    return 1;
}

static inline int hpclang_hp39_alias_records(
    void *cpu,
    uint32_t table_nibble,
    uint32_t stride,
    uint32_t record_nibbles,
    char records[5][17])
{
    static const char glyphs[5] = { 'P', 'A', 'S', '5', '1' };
    if (record_nibbles > 16u)
        return 0;

    for (size_t i = 0; i < 5u; ++i) {
        uint32_t record = table_nibble +
            (uint32_t)(uint8_t)glyphs[i] * stride;
        for (uint32_t n = 0; n < record_nibbles; ++n) {
            uint8_t value;
            if (!hpclang_hp39_alias_read_nibble(cpu, record + n, &value))
                return 0;
            records[i][n] = "0123456789abcdef"[value & 0x0fu];
        }
        records[i][record_nibbles] = '\0';
    }
    return 1;
}

static inline void hpclang_hp39_alias_probe(void *cpu)
{
    static int emitted;
    const char *target = getenv("TARGET");
    uint32_t main_table;
    uint32_t mini_table;
    char main_records[5][17];
    char mini_records[5][17];

    if (emitted || target == NULL)
        return;

    if (strcmp(target, "hp39gs") == 0) {
        main_table = 0x00000ae9u;
        mini_table = 0x00002000u + 0x00000b2du;
    } else if (strcmp(target, "hp40gs") == 0) {
        main_table = 0x00000c9cu;
        mini_table = 0x00002000u + 0x00000ce0u;
    } else {
        return;
    }

    if (!hpclang_hp39_alias_records(
            cpu, main_table, 16u, 16u, main_records) ||
        !hpclang_hp39_alias_records(
            cpu, mini_table, 11u, 11u, mini_records))
        return;

    emitted = 1;
    fprintf(stderr,
            "HP39 PRIVATE ALIAS PROBE target=%s "
            "main[P,A,S,5,1]=%s,%s,%s,%s,%s "
            "mini[P,A,S,5,1]=%s,%s,%s,%s,%s\n",
            target,
            main_records[0], main_records[1], main_records[2],
            main_records[3], main_records[4],
            mini_records[0], mini_records[1], mini_records[2],
            mini_records[3], mini_records[4]);
}

static inline int hpclang_hp39_probe_cpu_memory_rw_debug(
    void *cpu,
    uint32_t address,
    uint8_t *buffer,
    int length,
    int is_write)
{
    int result = cpu_memory_rw_debug(cpu, address, buffer, length, is_write);
    if (!is_write && result == 0 && length == 4) {
        const char *mode = getenv("HPCLANG_STOCK_MODE");
        const char *result_text = getenv("HPCLANG_STOCK_RESULT_ADDR");
        if (mode != NULL && strcmp(mode, "run") == 0 && result_text != NULL) {
            char *end = NULL;
            unsigned long result_address = strtoul(result_text, &end, 0);
            if (end != result_text && *end == '\0' &&
                address == (uint32_t)result_address) {
                uint32_t word = (uint32_t)buffer[0] |
                    ((uint32_t)buffer[1] << 8) |
                    ((uint32_t)buffer[2] << 16) |
                    ((uint32_t)buffer[3] << 24);
                if ((word & 0xff000000u) == 0x47000000u)
                    hpclang_hp39_alias_probe(cpu);
            }
        }
    }
    return result;
}

#define cpu_memory_rw_debug hpclang_hp39_probe_cpu_memory_rw_debug

#endif
