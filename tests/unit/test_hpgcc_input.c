#include <assert.h>
#include <errno.h>
#include <hpclang/device.h>
#include <hpclang/input.h>
#include <hpclang/stdio.h>
#include <hpclang/time.h>
#include "hp-character.h"
#include "stdio-routing.h"

static struct hpclang_device_info device;
static struct hpclang_keyboard_event events[16];
static unsigned count, cursor, idle, delays;
const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
void hpclang_delay_us(uint64_t duration)
{
    assert(duration == 1000);
    ++delays;
}
int hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event)
{
    if (idle) {
        --idle;
        return 0;
    }
    if (cursor == count) {
        errno = EIO;
        return -1;
    }
    *event = events[cursor++];
    return 1;
}
static void add(uint16_t key, unsigned type, uint32_t held)
{
    assert(count < sizeof events / sizeof events[0]);
    events[count++] = (struct hpclang_keyboard_event){.key = key, .type = type, .modifiers = held};
}
static void read_key(uint16_t key, int expected)
{
    count = cursor = 0;
    add(key, HPCLANG_KEYBOARD_EVENT_DOWN, 0);
    assert(hpclang_hpgcc_console_read(NULL) == expected);
}
int main(void)
{
    assert(hpclang_hpgcc_console_read(NULL) == HPCLANG_STDIO_ERROR && errno == ENOSYS);
    for (unsigned model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        device.model = model;
        idle = 1;
        unsigned previous = delays;
        read_key(HPCLANG_KEY_7, '7');
        assert(delays == previous + 1);
        uint16_t alpha_a = model <= HPCLANG_MODEL_HP40GS ? HPCLANG_KEY_HP39_VARS : HPCLANG_KEY_F1;
        count = cursor = 0;
        add(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA);
        add(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0);
        add(alpha_a, HPCLANG_KEYBOARD_EVENT_DOWN, 0);
        errno = EDOM;
        assert(hpclang_hpgcc_console_read(NULL) == 'A' && errno == EDOM);
        read_key(HPCLANG_KEY_2, '2');
        count = cursor = 0;
        add(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA);
        add(alpha_a, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA);
        assert(hpclang_hpgcc_console_read(NULL) == 'A');
        count = cursor = 0;
        add(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0);
        add(HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, 0);
        assert(hpclang_hpgcc_console_read(NULL) == '2');
        read_key(HPCLANG_KEY_ENTER, '\n');
        read_key(model <= HPCLANG_MODEL_HP40GS    ? HPCLANG_KEY_HP39_DEL
                 : model == HPCLANG_MODEL_HP48GII ? HPCLANG_KEY_HP48_BACKSPACE
                                                  : HPCLANG_KEY_HP4950_BACKSPACE,
                 '\b');
        read_key(HPCLANG_KEY_ON, HPCLANG_STDIO_ERROR);
        assert(errno == ECANCELED);
        count = cursor = 0;
        assert(hpclang_hpgcc_console_read(NULL) == HPCLANG_STDIO_ERROR && errno == EIO);
        if (model >= HPCLANG_MODEL_HP48GII) {
            count = cursor = 0;
            for (unsigned i = 0; i < 2; ++i) {
                add(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA);
                add(HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0);
            }
            add(HPCLANG_KEY_F1, HPCLANG_KEYBOARD_EVENT_DOWN, 0);
            assert(hpclang_hpgcc_console_read(NULL) == 'A');
            read_key(HPCLANG_KEY_F2, 'B');
            count = cursor = 0;
            add(HPCLANG_KEY_LEFT_SHIFT, HPCLANG_KEYBOARD_EVENT_DOWN,
                HPCLANG_KEYBOARD_MOD_LEFT_SHIFT);
            add(HPCLANG_KEY_LEFT_SHIFT, HPCLANG_KEYBOARD_EVENT_UP, 0);
            add(HPCLANG_KEY_F1, HPCLANG_KEYBOARD_EVENT_DOWN, 0);
            assert(hpclang_hpgcc_console_read(NULL) == 'a');
            read_key(HPCLANG_KEY_F2, 'B');
        }
    }
    for (unsigned byte = 0x80; byte <= 0xa0; ++byte) {
        int expected = byte == 0x81 ? 'x' : (int)byte;
        assert(hpclang_hp_byte_from_unicode(hpclang_hp_unicode_from_byte((unsigned char)byte)) ==
               expected);
    }
    assert(hpclang_hp_byte_from_unicode(0x03b2) == 0xdf);
    assert(hpclang_hp_byte_from_unicode(0x03bc) == 0xb5);
    assert(hpclang_hp_byte_from_unicode(0x2190) == 0x8e);
    assert(hpclang_hp_byte_from_unicode(0x1f600) == -1);
    return 0;
}
