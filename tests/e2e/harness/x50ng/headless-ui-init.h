#ifndef HPCLANG_X50NG_HEADLESS_UI_INIT_H
#define HPCLANG_X50NG_HEADLESS_UI_INIT_H

#include <glib.h>
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

#endif
