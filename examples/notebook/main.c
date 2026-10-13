#include "store.h"
#include <hpclang/datetime.h>
#include <hpclang/device.h>
#include <hpclang/port.h>
#include <hpclang/time.h>
#include <hpclang/ui-surface.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const struct hpclang_font notebook_font;

enum action
{
    EDIT = 1,
    SAVE,
    LOAD,
    CLOCK,
    EXIT,
    BACKUP,
    RESTORE,
    EXPORT
};
struct notebook
{
    char text[NOTEBOOK_CAPACITY];
    char status[48];
    char clock[32];
};

static void clock_read(struct notebook *note)
{
    struct hpclang_datetime now;
    if (hpclang_datetime_read(&now))
        snprintf(note->clock, sizeof(note->clock), "Clock error %d", errno);
    else
        snprintf(note->clock, sizeof(note->clock), "%04lu-%02lu-%02lu %02lu:%02lu:%02lu",
                 (unsigned long)now.year, (unsigned long)now.month, (unsigned long)now.day,
                 (unsigned long)now.hour, (unsigned long)now.minute, (unsigned long)now.second);
}

static void load(struct notebook *note)
{
    if (!notebook_load(note->text))
        strcpy(note->status, "Loaded");
    else if (errno == ENOENT)
        strcpy(note->status, "No saved note; edit retained");
    else if (errno == EILSEQ)
        strcpy(note->status, "Foreign data; save refused");
    else
        snprintf(note->status, sizeof(note->status), "Load error %d; edit retained", errno);
}

static int view(struct hpclang_ui *ui, struct hpclang_ui_surface *renderer,
                const struct notebook *note, int width, int height)
{
    if (hpclang_ui_begin(ui))
        return -1;
    HPCLANG_UI_SCOPE(hpclang_ui_scroll_begin(ui, 1, 0, height))
    {
        hpclang_ui_text(ui, 1, 0, "Notebook");
        hpclang_ui_text(ui, 2, 0, note->clock);
        hpclang_ui_text(ui, 3, 0, "Alpha to type");
        hpclang_ui_text_input(ui, 4, 0, note->text, sizeof(note->text),
                              (struct hpclang_ui_action){EDIT, 0}, 1);
        hpclang_ui_text(ui, 5, 0, note->status);
        hpclang_ui_button(ui, 6, 0, "Save", (struct hpclang_ui_action){SAVE, 0}, 1);
        hpclang_ui_button(ui, 7, 0, "Reload saved note", (struct hpclang_ui_action){LOAD, 0}, 1);
        hpclang_ui_button(ui, 8, 0, "Refresh clock", (struct hpclang_ui_action){CLOCK, 0}, 1);
        if (hpclang_device_capability(HPCLANG_CAPABILITY_STREAM_STORAGE) ==
            HPCLANG_CAPABILITY_SUPPORTED)
            hpclang_ui_button(ui, 16, 0, "Export HPCNOTE.TXT",
                              (struct hpclang_ui_action){EXPORT, 0}, 1);
        for (unsigned port = 0; port < 3; ++port) {
            struct hpclang_port_info info = {.size = sizeof(info),
                                             .version = HPCLANG_PORT_INFO_VERSION};
            if (hpclang_port_query(port, &info) || !(info.available & HPCLANG_PORT_READ_BACKUP))
                continue;
            char label[24];
            snprintf(label, sizeof(label), "Port %u backup", port);
            hpclang_ui_button(
                ui, 10 + port * 2, 0, label, (struct hpclang_ui_action){BACKUP, port},
                !!(info.available & (HPCLANG_PORT_CREATE_BACKUP | HPCLANG_PORT_REPLACE_BACKUP)));
            snprintf(label, sizeof(label), "Port %u restore", port);
            hpclang_ui_button(ui, 11 + port * 2, 0, label,
                              (struct hpclang_ui_action){RESTORE, port}, 1);
        }
        hpclang_ui_button(ui, 9, 0, "Exit (unsaved edits lost)",
                          (struct hpclang_ui_action){EXIT, 0}, 1);
    }
    if (hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, width, height},
                          hpclang_ui_surface_measure, renderer)) {
        hpclang_ui_abort(ui);
        return -1;
    }
    return hpclang_ui_commit(ui);
}

int main(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_ui_config capacity = {24, 768, 0};
    size_t bytes = hpclang_ui_storage_size(capacity);
    void *storage = malloc(bytes);
    struct hpclang_ui *ui = storage ? hpclang_ui_init(storage, bytes, capacity) : NULL;
    const struct hpclang_ui_surface_style style = {
        .font = &notebook_font, .foreground = 255, .disabled = 85, .accent = 255, .gap = 2};
    struct hpclang_ui_surface *renderer =
        hpclang_ui_surface_create(device->display_width, device->display_height, 2, &style, 1, 0);
    int result = 1, display = 0;
    if (!ui || !renderer || hpclang_display_begin(2))
        goto done;
    display = 1;
    struct notebook note = {0};
    clock_read(&note);
    load(&note);
    struct hpclang_text_mode mode;
    if (hpclang_text_mode_init(&mode, device->model))
        goto done;
    hpclang_keyboard_events_reset();
    int declare = 1, publish = 0, quit = 0;
    while (!quit) {
        if (declare &&
            view(ui, renderer, &note, (int)device->display_width, (int)device->display_height))
            goto done;
        declare = 0;
        int painted = hpclang_ui_surface_render(renderer, ui);
        if (painted < 0)
            goto done;
        publish |= painted > 0;
        if (publish) {
            if (hpclang_ui_surface_copy(renderer, hpclang_display_surface()))
                goto done;
            if (!hpclang_display_present(100000))
                publish = 0;
            else if (errno != ETIMEDOUT)
                goto done;
        }
        struct hpclang_keyboard_event key;
        int available = hpclang_keyboard_event_poll(&key);
        if (available < 0)
            goto done;
        if (available) {
            struct hpclang_ui_event event;
            int emitted = hpclang_ui_key(ui, &mode, &key, &event);
            if (emitted < 0 && errno != ENOSPC)
                goto done;
            if (emitted > 0) {
                declare = 1;
                switch (event.action.type) {
                case EDIT:
                    if (event.kind == HPCLANG_UI_CHANGE && event.text &&
                        strlen(event.text) < sizeof(note.text)) {
                        strcpy(note.text, event.text);
                        strcpy(note.status, "Unsaved");
                    }
                    break;
                case SAVE:
                    if (notebook_save(note.text))
                        snprintf(note.status, sizeof(note.status), "Save error %d", errno);
                    else
                        strcpy(note.status, "Saved; exit normally");
                    break;
                case LOAD:
                    load(&note);
                    break;
                case CLOCK:
                    clock_read(&note);
                    break;
                case BACKUP:
                    if (notebook_backup_save(event.action.arg, note.text))
                        snprintf(note.status, sizeof(note.status), "Backup error %d", errno);
                    else
                        strcpy(note.status, "Backup saved");
                    break;
                case RESTORE:
                    if (notebook_backup_load(event.action.arg, note.text))
                        snprintf(note.status, sizeof(note.status), "Restore error %d", errno);
                    else
                        strcpy(note.status, "Restored; Save to keep here");
                    break;
                case EXPORT:
                    if (notebook_export_file("/HPCNOTE.TXT", note.text)) {
                        if (errno == EEXIST)
                            strcpy(note.status, "HPCNOTE.TXT already exists");
                        else
                            snprintf(note.status, sizeof(note.status), "Export error %d", errno);
                    } else
                        strcpy(note.status, "Exported HPCNOTE.TXT");
                    break;
                case EXIT:
                    quit = 1;
                    break;
                }
            }
            if (key.key == HPCLANG_KEY_ON && key.type == HPCLANG_KEYBOARD_EVENT_DOWN && !emitted)
                quit = 1;
        }
        hpclang_delay_us(20000);
    }
    result = 0;
done:
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    if (display)
        hpclang_display_end();
    free(storage);
    return result;
}
