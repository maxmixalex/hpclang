#ifndef HPCLANG_X50NG_HEADLESS_UI_INIT_H
#define HPCLANG_X50NG_HEADLESS_UI_INIT_H

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emulator_api.h"
#include "options.h"
#include "qemu/cpu-common.h"
#include "ui4x/src/api.h"

/*
 * Temporary stock-HP39 diagnostic. Keep this probe in the host harness only:
 * it observes the already-running guest through QEMU debug reads and does not
 * change the HPC1 payload, launcher ABI, or calculator memory.
 *
 * HPGCC 1.1's c_loader treats ARMSYS r1 as SAT_CPU *, whose first member is
 * Read[257]. The native HP39 mini font is at Saturn nibble address 0x07e695.
 * Capture Read[0x7e] and five known glyph records so a Stock run can be
 * compared nibble-for-nibble with the independently decoded rom.39g table.
 */
static inline int hpclang_probe_raw_read(
    void *cpu, uint32_t address, uint8_t *buffer, int length)
{
    return cpu_memory_rw_debug(cpu, address, buffer, length, 0);
}

static inline int hpclang_probe_raw_read32(
    void *cpu, uint32_t address, uint32_t *value)
{
    uint8_t b[4];
    if (hpclang_probe_raw_read(cpu, address, b, 4) != 0)
        return 0;
    *value = (uint32_t)b[0] |
             ((uint32_t)b[1] << 8) |
             ((uint32_t)b[2] << 16) |
             ((uint32_t)b[3] << 24);
    return 1;
}

static inline int hpclang_probe_saturn_nibble(
    void *cpu, uint32_t read_page, uint32_t saturn_address, uint8_t *value)
{
    uint8_t byte;
    uint32_t arm_address = read_page + ((saturn_address & 0xfffu) >> 1);
    if (hpclang_probe_raw_read(cpu, arm_address, &byte, 1) != 0)
        return 0;
    *value = (saturn_address & 1u) != 0u
        ? (uint8_t)(byte >> 4)
        : (uint8_t)(byte & 0x0fu);
    return 1;
}

static inline void hpclang_probe_stock_hp39_font(void *cpu)
{
    static int emitted;
    static const char glyphs[] = { 'P', 'A', 'S', '5', '1' };
    const char *stock_mode;
    uint32_t model;
    uint32_t saturn_state;
    uint32_t read_7e;
    char records[sizeof(glyphs)][12];

    if (emitted)
        return;

    stock_mode = getenv("HPCLANG_STOCK_MODE");
    if (stock_mode == NULL || strcmp(stock_mode, "run") != 0)
        return;

    if (!hpclang_probe_raw_read32(cpu, 0x01100000u, &model) ||
        model < 1u || model > 3u ||
        !hpclang_probe_raw_read32(cpu, 0x01100008u, &saturn_state) ||
        saturn_state == 0u ||
        !hpclang_probe_raw_read32(
            cpu, saturn_state + 0x7eu * 4u, &read_7e) ||
        read_7e == 0u)
        return;

    for (size_t i = 0; i < sizeof(glyphs); ++i) {
        uint32_t record = 0x07e695u + (uint32_t)(uint8_t)glyphs[i] * 11u;
        for (uint32_t n = 0; n < 11u; ++n) {
            uint8_t value;
            if (!hpclang_probe_saturn_nibble(
                    cpu, read_7e, record + n, &value))
                return;
            records[i][n] = "0123456789abcdef"[value & 0x0fu];
        }
        records[i][11] = '\0';
    }

    emitted = 1;
    fprintf(
        stderr,
        "HP39 FONT PROBE model=%u saturn_state=%08x Read[7e]=%08x "
        "P=%s A=%s S=%s 5=%s 1=%s\n",
        model,
        saturn_state,
        read_7e,
        records[0], records[1], records[2], records[3], records[4]);
}

static inline int hpclang_probe_cpu_memory_rw_debug(
    void *cpu,
    uint32_t address,
    uint8_t *buffer,
    int length,
    int is_write)
{
    int result = cpu_memory_rw_debug(cpu, address, buffer, length, is_write);
    if (!is_write && result == 0)
        hpclang_probe_stock_hp39_font(cpu);
    return result;
}

#define cpu_memory_rw_debug hpclang_probe_cpu_memory_rw_debug

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

#endif
