#include <errno.h>
#include <hpclang/device.h>
#include <hpclang/input.h>
#include <hpclang/stdio.h>
#include <hpclang/time.h>
#include "hp-character.h"
#include "stdio-routing.h"

/* Process-wide text state; never reset the physical event queue or timings. */
static struct hpclang_text_mode mode;

int hpclang_hpgcc_console_read(void *context)
{
    (void)context;
    uint32_t model = hpclang_device_info()->model;
    if (mode.model != model || !mode.model)
        if (hpclang_text_mode_init(&mode, model))
            return HPCLANG_STDIO_ERROR;
    for (;;) {
        struct hpclang_keyboard_event event;
        int result = hpclang_keyboard_event_poll(&event);
        if (result < 0)
            return HPCLANG_STDIO_ERROR;
        if (!result) {
            hpclang_delay_us(1000);
            continue;
        }
        enum hpclang_text_plane plane;
        result = hpclang_text_mode_event(&mode, &event, &plane);
        if (result < 0)
            return HPCLANG_STDIO_ERROR;
        if (!result)
            continue;
        switch (event.key) {
        case HPCLANG_KEY_ENTER:
            return '\n';
        case HPCLANG_KEY_ON:
            errno = ECANCELED;
            return HPCLANG_STDIO_ERROR;
        case HPCLANG_KEY_HP39_DEL:
        case HPCLANG_KEY_HP48_BACKSPACE:
        case HPCLANG_KEY_HP4950_BACKSPACE:
            return '\b';
        default:
            break;
        }
        uint32_t codepoint;
        result = hpclang_text_key_lookup(model, event.key, plane, event.modifiers, &codepoint);
        if (result < 0)
            return HPCLANG_STDIO_ERROR;
        if (!result)
            continue;
        int byte = hpclang_hp_byte_from_unicode(codepoint);
        if (byte < 0) {
            errno = EILSEQ;
            return HPCLANG_STDIO_ERROR;
        }
        return byte;
    }
}
