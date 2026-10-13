#include "../../examples/notebook/app.h"
#include <hpclang/bootstrap.h>
#include <hpclang/datetime.h>
#include <hpclang/device.h>
#include <hpclang/font-package.h>
#include <hpclang/port.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static char saved[NOTEBOOK_CAPACITY];
static int load_error, save_error, clock_error;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return NULL;
}
int hpclang_device_capability(enum hpclang_capability capability)
{
    (void)capability;
    return HPCLANG_CAPABILITY_SUPPORTED;
}
int hpclang_port_query(uint32_t port, struct hpclang_port_info *info)
{
    assert(port < 3);
    info->available = HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_CREATE_BACKUP;
    return 0;
}
int hpclang_datetime_read(struct hpclang_datetime *out)
{
    if (clock_error) {
        errno = clock_error;
        return -1;
    }
    *out = (struct hpclang_datetime){2026, 10, 14, 12, 34, 56};
    return 0;
}
int notebook_load(char text[NOTEBOOK_CAPACITY])
{
    if (load_error) {
        errno = load_error;
        return -1;
    }
    strcpy(text, saved);
    return 0;
}
int notebook_save(const char *text)
{
    if (save_error) {
        errno = save_error;
        return -1;
    }
    strcpy(saved, text);
    return 0;
}
int notebook_backup_load(unsigned port, char text[NOTEBOOK_CAPACITY])
{
    assert(port < 3);
    return notebook_load(text);
}
int notebook_backup_save(unsigned port, const char *text)
{
    assert(port < 3);
    return notebook_save(text);
}
int notebook_export_file(const char *path, const char *text)
{
    assert(!strcmp(path, "/HPCNOTE.TXT") && text);
    errno = EEXIST;
    return -1;
}

static uint32_t control(struct hpclang_ui *ui, unsigned action)
{
    for (size_t i = 0; i < hpclang_ui_node_count(ui); ++i) {
        const struct hpclang_ui_node *node = hpclang_ui_node_at(ui, i);
        if (node->action.type == action)
            return node->id;
    }
    assert(0);
    return 0;
}
static int activate(struct notebook *note, struct hpclang_ui *ui, unsigned action)
{
    assert(hpclang_ui_focus(ui, control(ui, action)) == 0);
    struct hpclang_ui_event event;
    assert(hpclang_ui_activate(ui, &event) == 1);
    return notebook_update(note, &event);
}

static void run(unsigned height)
{
    static const uint8_t bits[] = {0xe0, 0xa0, 0xe0};
    static const struct hpclang_bitmap_glyph glyph = {
        .codepoint = '?', .bitmap = bits, .width = 3, .height = 3, .advance = 4, .stride = 1};
    static const struct hpclang_bitmap_glyph *const glyphs[] = {&glyph};
    static const struct hpclang_bitmap_font_data data = {.glyphs = glyphs,
                                                         .glyph_count = 1,
                                                         .fallback_codepoint = '?',
                                                         .ascent = 3,
                                                         .line_height = 4,
                                                         .nominal_advance = 4};
    static const struct hpclang_font font = HPCLANG_BITMAP_FONT(data);
    struct hpclang_ui_surface_style style = {
        .font = &font, .foreground = 255, .accent = 255, .gap = 2};
    struct hpclang_ui_surface *renderer = hpclang_ui_surface_create(131, height, 2, &style, 1, 0);
    const struct hpclang_ui_config config = NOTEBOOK_UI_CONFIG;
    size_t bytes = hpclang_ui_storage_size(config);
    void *storage = malloc(bytes);
    struct hpclang_ui *ui = hpclang_ui_init(storage, bytes, config);
    assert(renderer && ui);
    strcpy(saved, "saved");
    load_error = save_error = clock_error = 0;
    struct notebook note;
    notebook_init(&note);
    assert(!strcmp(note.text, "saved") && !strcmp(note.clock, "2026-10-14 12:34:56"));
    memset(note.text, 'x', sizeof(note.text) - 1);
    note.text[sizeof(note.text) - 1] = 0;
    assert(notebook_view(ui, renderer, &note, 131, (int)height) == 0);
    assert(hpclang_ui_surface_render(renderer, ui) == 1);
    assert(!activate(&note, ui, SAVE) && !strcmp(saved, note.text));
    assert(!activate(&note, ui, EXPORT) && !strcmp(note.status, "HPCNOTE.TXT already exists"));
    load_error = EIO;
    assert(!activate(&note, ui, LOAD) && strlen(note.text) == NOTEBOOK_CAPACITY - 1);
    load_error = 0;
    strcpy(saved, "restored");
    assert(!activate(&note, ui, RESTORE) && !strcmp(note.text, "restored"));
    assert(notebook_view(ui, renderer, &note, 131, (int)height) == 0);
    assert(hpclang_ui_focus(ui, control(ui, EDIT)) == 0);
    struct hpclang_ui_event event;
    assert(hpclang_ui_edit(ui, HPCLANG_INPUT_INSERT, '!', &event) == 1);
    assert(!notebook_update(&note, &event) && !strcmp(note.text, "restored!"));
    assert(!strcmp(saved, "restored"));
    assert(notebook_view(ui, renderer, &note, 131, (int)height) == 0);
    save_error = ENOSPC;
    assert(!activate(&note, ui, SAVE) && !strcmp(saved, "restored"));
    assert(!strcmp(note.text, "restored!"));
    clock_error = EIO;
    assert(!activate(&note, ui, CLOCK) && strstr(note.clock, "Clock error"));
    assert(activate(&note, ui, EXIT) == 1);
    hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    free(storage);
}
int main(void)
{
    run(64);
    run(80);
    return 0;
}
