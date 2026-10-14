#include "../../../examples/notebook/app.h"
#include <hpclang/object-store.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

volatile uint32_t hpclang_e2e_result;
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
    struct notebook note;
    notebook_init(&note);
    unsigned failure = 1, phase = 0;
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
