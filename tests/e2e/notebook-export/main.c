#include "../../../examples/notebook/app.h"
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

volatile uint32_t hpclang_e2e_result;
extern const struct hpclang_font notebook_font;
extern volatile uint32_t hpclang_notebook_stack_witness[3];
uint32_t hpclang_notebook_stack_sample(void);
static const char text[] = "Notebook αβ UTF-8";

static void hold(void)
{
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
}

static int check_file(void)
{
    char bytes[sizeof(text) + 1];
    int fd = open("/HPCNOTE.TXT", O_RDONLY);
    if (fd < 0)
        return -1;
    ssize_t count = read(fd, bytes, sizeof(bytes));
    int closed = close(fd);
    return closed || count != (ssize_t)sizeof(text) || memcmp(bytes, text, sizeof(text) - 1) ||
                   bytes[sizeof(text) - 1] != '\n'
               ? -1
               : 0;
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
    unsigned failure = 5, phase = 0;
    int display = 0;
    if (!ui || !renderer || hpclang_display_begin(2))
        goto done;
    display = 1;
    struct notebook note;
    notebook_init(&note);
    failure = 1;
    char previous[NOTEBOOK_CAPACITY];
    if (notebook_load(previous)) {
        if (errno != ENOENT)
            goto done;
        strcpy(note.text, text);
        struct hpclang_ui_event save = {.kind = HPCLANG_UI_ACTIVATE, .action = {SAVE, 0}};
        if (notebook_update(&note, &save) || strcmp(note.status, "Saved; exit normally"))
            goto done;
        struct hpclang_ui_event export = {.kind = HPCLANG_UI_ACTIVATE, .action = {EXPORT, 0}};
        failure = 2;
        if (notebook_update(&note, &export) || strcmp(note.status, "Exported HPCNOTE.TXT"))
            goto done;
    } else {
        phase = 1;
        if (strcmp(previous, text))
            goto done;
    }
    failure = 3;
    if (check_file())
        goto done;
    /* Both the first session and a fresh launch must refuse replacement. */
    if (notebook_export_file("/HPCNOTE.TXT", "Must not replace") != -1 || errno != EEXIST ||
        check_file())
        goto done;
    failure = 4;
    if (notebook_load(previous) || strcmp(previous, text))
        goto done;
    if (phase == 1) {
        struct hpclang_object_store *home = hpclang_object_store_home();
        if (!home)
            goto done;
        int removed = hpclang_object_store_remove(home, (const uint8_t *)"HPCNOTE", 7);
        hpclang_object_store_close(home);
        if (removed)
            goto done;
    }
    /* Keep the application's UI/renderer alive across maximum-size export,
     * refusal to replace, and readback on both independent launches. */
    failure = 6;
    memset(note.text, 'N', sizeof(note.text) - 1);
    note.text[sizeof(note.text) - 1] = 0;
    if (!phase && notebook_export_file("/MAXNOTE.TXT", note.text))
        goto done;
    if (notebook_export_file("/MAXNOTE.TXT", "Must not replace") != -1 || errno != EEXIST)
        goto done;
    int fd = open("/MAXNOTE.TXT", O_RDONLY);
    if (fd < 0)
        goto done;
    int bad = 0;
    for (unsigned i = 0; i < NOTEBOOK_CAPACITY; ++i) {
        char byte;
        if (read(fd, &byte, 1) != 1 || byte != (i + 1 == NOTEBOOK_CAPACITY ? '\n' : 'N')) {
            bad = 1;
            break;
        }
    }
    char extra;
    if (!bad && read(fd, &extra, 1) != 0)
        bad = 1;
    if (close(fd) || bad)
        goto done;
    strcpy(note.status, "Export verified");
    failure = 7;
    if (notebook_view(ui, renderer, &note, (int)device->display_width,
                      (int)device->display_height) ||
        hpclang_ui_surface_render(renderer, ui) < 0 ||
        hpclang_ui_surface_copy(renderer, hpclang_display_surface()) ||
        hpclang_display_present(100000))
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
    if (ui)
        hpclang_ui_shutdown(ui);
    hpclang_ui_surface_destroy(renderer);
    if (display)
        hpclang_display_end();
    free(storage);
    hpclang_notebook_stack_witness[1] = hpclang_notebook_stack_sample();
    return 0;
}
