#include "../../../examples/notebook/app.h"
#include <hpclang/aplet.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <hpclang/port.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

extern const struct hpclang_font notebook_font;
volatile uint32_t hpclang_e2e_result;
extern volatile uint32_t hpclang_notebook_stack_witness[3];
uint32_t hpclang_notebook_stack_sample(void);

static void hold(void)
{
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
}

static int action(struct notebook *note, struct hpclang_ui *ui, unsigned type)
{
    for (size_t i = 0; i < hpclang_ui_node_count(ui); ++i) {
        const struct hpclang_ui_node *node = hpclang_ui_node_at(ui, i);
        if (node->action.type != type)
            continue;
        struct hpclang_ui_event event;
        if (hpclang_ui_focus(ui, node->id) || hpclang_ui_activate(ui, &event) != 1)
            return -1;
        return notebook_update(note, &event);
    }
    return -1;
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

/* Exercise the supported optional services with a maximum-length note while
 * the UI and renderer remain live. Port 1/2 mutation acceptance is separate. */
static int optional_workload(struct hpclang_ui *ui, const struct hpclang_device_info *device)
{
    uint32_t previous_focus = hpclang_ui_focused(ui);
    struct notebook work = {0};
    struct hpclang_port_info info = {.size = sizeof(info), .version = HPCLANG_PORT_INFO_VERSION};
    const unsigned backup = HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_CREATE_BACKUP |
                            HPCLANG_PORT_REPLACE_BACKUP | HPCLANG_PORT_REMOVE_BACKUP;
    if (hpclang_port_query(0, &info))
        return -1;
    int has_backup =
        device->family == HPCLANG_FAMILY_HP48 || device->family == HPCLANG_FAMILY_HP4950;
    if (has_backup != ((info.available & backup) == backup))
        return -1;
    if (has_backup) {
        strcpy(work.text, "Short backup");
        if (action(&work, ui, BACKUP) || strcmp(work.status, "Backup saved"))
            return -1;
    }
    memset(work.text, 'N', sizeof(work.text) - 1);
    if (has_backup) {
        if (action(&work, ui, BACKUP) || strcmp(work.status, "Backup saved"))
            return -1;
        strcpy(work.text, "Unsaved edit");
        if (action(&work, ui, RESTORE) || strcmp(work.status, "Restored; Save to keep here") ||
            strlen(work.text) != sizeof(work.text) - 1)
            return -1;
        for (unsigned i = 0; i < sizeof(work.text) - 1; ++i)
            if (work.text[i] != 'N')
                return -1;
        if (hpclang_port_backup_remove(0, (const uint8_t *)"HPCNOTE", 7))
            return -1;
    }
    /* The extra button actions scroll to the backup controls. Restore the
     * original viewport/focus before the unchanged persistence pixel gate. */
    if (hpclang_ui_focus(ui, 0))
        return -1;
    for (size_t i = 0; i < hpclang_ui_node_count(ui); ++i) {
        const struct hpclang_ui_node *node = hpclang_ui_node_at(ui, i);
        if (node->kind == HPCLANG_UI_SCROLL && hpclang_ui_scroll_to(ui, node->id, 0))
            return -1;
    }
    return hpclang_ui_focus(ui, previous_focus);
}

int main(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_ui_config config = notebook_ui_config();
    size_t size = hpclang_ui_storage_size(config);
    void *storage = malloc(size);
    struct hpclang_ui *ui = storage ? hpclang_ui_init(storage, size, config) : NULL;
    const struct hpclang_ui_surface_style style = {
        .font = &notebook_font, .foreground = 255, .accent = 255, .gap = 2};
    struct hpclang_ui_surface *renderer =
        hpclang_ui_surface_create(device->display_width, device->display_height, 2, &style, 1, 0);
    unsigned failure = 1, phase = 0;
    int display = 0;
    if (!ui || !renderer || hpclang_display_begin(2))
        goto done;
    display = 1;
    struct notebook note;
    notebook_init(&note);
    if (strstr(note.clock, "error")) {
        failure = 2;
        goto done;
    }
    char previous[NOTEBOOK_CAPACITY];
    if (notebook_load(previous)) {
        if (errno != ENOENT) {
            failure = 3;
            goto done;
        }
        strcpy(note.text, "First saved note");
    } else if (!strcmp(previous, "First saved note")) {
        phase = 1;
        strcpy(note.text, "Second saved note");
    } else if (!strcmp(previous, "Second saved note")) {
        phase = 2;
    } else {
        failure = 4;
        goto done;
    }
    if (notebook_view(ui, renderer, &note, (int)device->display_width,
                      (int)device->display_height)) {
        failure = 5;
        goto done;
    }
    if (phase < 2) {
        if (action(&note, ui, SAVE) || notebook_load(previous) || strcmp(previous, note.text)) {
            failure = 6;
            goto done;
        }
    } else if (cleanup()) {
        failure = 7;
        goto done;
    } else {
        strcpy(note.status, "Persistence verified");
    }
    if (!phase && optional_workload(ui, device)) {
        failure = 9;
        goto done;
    }
    if (notebook_view(ui, renderer, &note, (int)device->display_width,
                      (int)device->display_height) ||
        hpclang_ui_surface_render(renderer, ui) < 0 ||
        hpclang_ui_surface_copy(renderer, hpclang_display_surface()) ||
        hpclang_display_present(100000)) {
        failure = 8;
        goto done;
    }
    hpclang_e2e_result = UINT32_C(0x4f53b000) | phase;
    hold();
    failure = 0;
done:
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hold();
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    if (display)
        hpclang_display_end();
    free(storage);
    hpclang_notebook_stack_witness[1] = hpclang_notebook_stack_sample();
    return 0;
}
