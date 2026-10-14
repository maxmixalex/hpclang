#include "app.h"
#include <hpclang/datetime.h>
#include <hpclang/device.h>
#include <hpclang/port.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static unsigned port_operations(unsigned port)
{
    struct hpclang_port_info info = {.size = sizeof(info), .version = HPCLANG_PORT_INFO_VERSION};
    return hpclang_port_query(port, &info) ? 0u : info.available;
}

struct hpclang_ui_config notebook_ui_config(void)
{
    /* These capabilities describe the launch context, not whether a card is
       currently inserted. Reserve each control that the view can declare. */
    struct hpclang_ui_config config = {10, 512, 0};
    if (hpclang_device_capability(HPCLANG_CAPABILITY_STREAM_STORAGE) ==
        HPCLANG_CAPABILITY_SUPPORTED)
        ++config.nodes;
    for (unsigned port = 0; port < 3; ++port)
        if (port_operations(port) & HPCLANG_PORT_READ_BACKUP)
            config.nodes += 2;
    return config;
}

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

int notebook_view(struct hpclang_ui *ui, struct hpclang_ui_surface *renderer,
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
            unsigned operations = port_operations(port);
            if (!(operations & HPCLANG_PORT_READ_BACKUP))
                continue;
            char label[24];
            snprintf(label, sizeof(label), "Port %u backup", port);
            hpclang_ui_button(
                ui, 10 + port * 2, 0, label, (struct hpclang_ui_action){BACKUP, port},
                !!(operations & (HPCLANG_PORT_CREATE_BACKUP | HPCLANG_PORT_REPLACE_BACKUP)));
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

void notebook_init(struct notebook *note)
{
    *note = (struct notebook){0};
    clock_read(note);
    load(note);
}

int notebook_update(struct notebook *note, const struct hpclang_ui_event *event)
{
    switch (event->action.type) {
    case EDIT:
        if (event->kind == HPCLANG_UI_CHANGE && event->text &&
            strlen(event->text) < sizeof(note->text)) {
            strcpy(note->text, event->text);
            strcpy(note->status, "Unsaved");
        }
        break;
    case SAVE:
        if (notebook_save(note->text))
            snprintf(note->status, sizeof(note->status), "Save error %d", errno);
        else
            strcpy(note->status, "Saved; exit normally");
        break;
    case LOAD:
        load(note);
        break;
    case CLOCK:
        clock_read(note);
        break;
    case BACKUP:
        if (notebook_backup_save(event->action.arg, note->text))
            snprintf(note->status, sizeof(note->status), "Backup error %d", errno);
        else
            strcpy(note->status, "Backup saved");
        break;
    case RESTORE:
        if (notebook_backup_load(event->action.arg, note->text))
            snprintf(note->status, sizeof(note->status), "Restore error %d", errno);
        else
            strcpy(note->status, "Restored; Save to keep here");
        break;
    case EXPORT:
        if (notebook_export_file("/HPCNOTE.TXT", note->text)) {
            if (errno == EEXIST)
                strcpy(note->status, "HPCNOTE.TXT already exists");
            else
                snprintf(note->status, sizeof(note->status), "Export error %d", errno);
        } else
            strcpy(note->status, "Exported HPCNOTE.TXT");
        break;
    case EXIT:
        return 1;
    }
    return 0;
}
