#include <hpkeyb49.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/keyboard.h>
#include <hpclang/target.h>
#include <hpclang/time.h>
#include <errno.h>
#include "keyboard-key-map.h"

static unsigned model(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    unsigned id = boot ? boot->model : 0;
    if (id < HPCLANG_MODEL_HP39GPLUS || id > HPCLANG_MODEL_HP50G) {
        errno = ENOSYS;
        return 0;
    }
    return id;
}
int keyb_isKeyPressed(unsigned char col, unsigned char row)
{
    if (col >= HPCLANG_KEYBOARD_COLUMNS || row >= HPCLANG_KEYBOARD_ROWS) {
        errno = EINVAL;
        return 0;
    }
    if (!model())
        return 0;
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    return (int)((state.bits >> (col * HPCLANG_KEYBOARD_ROWS + row)) & 1u);
}
int keyb_isAnyKeyPressed(void)
{
    if (!model())
        return 0;
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    return state.bits != 0;
}
static int named(unsigned key)
{
    unsigned id = model();
    if (!id)
        return 0;
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    for (unsigned scan = 0; scan < 60; ++scan)
        if ((state.bits & (UINT64_C(1) << scan)) &&
            hpclang_keyboard_key_from_scan(id, (uint8_t)scan) == key)
            return 1;
    return 0;
}
int keyb_isLeft(void)
{
    return named(HPCLANG_KEY_LEFT);
}
int keyb_isRight(void)
{
    return named(HPCLANG_KEY_RIGHT);
}
int keyb_isUp(void)
{
    return named(HPCLANG_KEY_UP);
}
int keyb_isDown(void)
{
    return named(HPCLANG_KEY_DOWN);
}
int keyb_isAlpha(void)
{
    return named(HPCLANG_KEY_ALPHA);
}
int keyb_isLS(void)
{
    unsigned id = model();
    if (!id)
        return 0;
    return named(id <= HPCLANG_MODEL_HP40GS ? HPCLANG_KEY_HP39_SHIFT : HPCLANG_KEY_LEFT_SHIFT);
}
int keyb_isRS(void)
{
    return named(HPCLANG_KEY_RIGHT_SHIFT);
}
int keyb_isON(void)
{
    return named(HPCLANG_KEY_ON);
}
void keyb_waitKeyPressed(void)
{
    if (!model())
        return;
    int pressed = 0;
    for (;;) {
        struct hpclang_keyboard_state state;
        hpclang_keyboard_read(&state);
        if (state.bits)
            pressed = 1;
        else if (pressed)
            return;
        hpclang_delay_us(1000);
    }
}
