#include "../../../examples/notebook/app.h"
#include <hpclang/aplet.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <hpclang/port.h>
#include <errno.h>
#include <string.h>

volatile uint32_t hpclang_e2e_result;

static void hold(void)
{
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
}

static int cleanup(void)
{
    if (hpclang_device_capability(HPCLANG_CAPABILITY_APLET_DATA) == HPCLANG_CAPABILITY_SUPPORTED)
        return hpclang_aplet_data_clear();
    struct hpclang_object_store *home = hpclang_object_store_home();
    if (!home)
        return -1;
    int result = hpclang_object_store_remove(home, (const uint8_t *)"HPCNOTE", 7);
    hpclang_object_store_close(home);
    return result;
}

static int backup_cycle(struct notebook *note, unsigned phase)
{
    struct hpclang_port_info port = {.size = sizeof(port), .version = HPCLANG_PORT_INFO_VERSION};
    if (hpclang_port_query(0, &port))
        return -1;
    if (port.medium == HPCLANG_PORT_ABSENT)
        return -1;
    unsigned required = HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_CREATE_BACKUP |
                        HPCLANG_PORT_REPLACE_BACKUP | HPCLANG_PORT_REMOVE_BACKUP;
    if ((port.available & required) != required)
        return -1;
    char value[NOTEBOOK_CAPACITY];
    if (phase == 2) {
        if (notebook_backup_load(0, value) || strcmp(value, "Second saved note"))
            return -1;
        return hpclang_port_backup_remove(0, (const uint8_t *)"HPCNOTE", 7);
    }
    if (phase == 1) {
        struct hpclang_ui_event restore = {.kind = HPCLANG_UI_ACTIVATE, .action = {RESTORE, 0}};
        if (notebook_update(note, &restore) || strcmp(note->text, "First saved note") ||
            notebook_load(value) || strcmp(value, "Second saved note"))
            return -1;
        /* Restore changes the edit buffer, while the primary save survives. */
        struct hpclang_ui_event reload = {.kind = HPCLANG_UI_ACTIVATE, .action = {LOAD, 0}};
        if (notebook_update(note, &reload) || strcmp(note->text, "Second saved note"))
            return -1;
    }
    struct hpclang_ui_event backup = {.kind = HPCLANG_UI_ACTIVATE, .action = {BACKUP, 0}};
    if (notebook_update(note, &backup) || strcmp(note->status, "Backup saved") ||
        notebook_backup_load(0, value) || strcmp(value, note->text))
        return -1;
    return 0;
}

int main(void)
{
    struct notebook note;
    notebook_init(&note);
    unsigned phase = 0, failure = 1;
    char previous[NOTEBOOK_CAPACITY];
    if (notebook_load(previous)) {
        if (errno != ENOENT)
            goto done;
        strcpy(note.text, "First saved note");
    } else if (!strcmp(previous, "First saved note")) {
        phase = 1;
        strcpy(note.text, "Second saved note");
    } else if (!strcmp(previous, "Second saved note")) {
        phase = 2;
    } else {
        goto done;
    }
    failure = 2;
    if (phase < 2) {
        struct hpclang_ui_event save = {.kind = HPCLANG_UI_ACTIVATE, .action = {SAVE, 0}};
        if (notebook_update(&note, &save) || strcmp(note.status, "Saved; exit normally"))
            goto done;
    }
    failure = 3;
    if (backup_cycle(&note, phase))
        goto done;
    failure = 4;
    if (phase == 2 && cleanup())
        goto done;
    hpclang_e2e_result = UINT32_C(0x4f53b000) | phase;
    hold();
    failure = 0;
done:
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hold();
    return 0;
}
