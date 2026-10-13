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
void keyb_getmatrix(keymatrix *ptr)
{
    if (!ptr) {
        errno = EINVAL;
        return;
    }
    ptr->full = 0;
    if (!model())
        return;
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    for (unsigned col = 0; col < HPCLANG_KEYBOARD_COLUMNS; ++col)
        ptr->full |= ((state.bits >> (col * 7)) & UINT64_C(0x7f)) << (col * 8 + 1);
    ptr->full |= ((state.bits >> HPCLANG_KEYBOARD_GPF4) & UINT64_C(7)) << 60;
    ptr->full |= ((state.bits >> HPCLANG_KEYBOARD_ON) & UINT64_C(1)) << 63;
}
int keyb_isKeyPrM(int keycode)
{
    if (keycode < 0 || keycode > 63) {
        errno = EINVAL;
        return 0;
    }
    keymatrix matrix;
    keyb_getmatrix(&matrix);
    return (int)((matrix.full >> keycode) & UINT64_C(1));
}
int keyb_isShiftKeyPrM(int shiftcode)
{
    unsigned key = KEYCODE((uint32_t)shiftcode);
    if (key > 63) {
        errno = EINVAL;
        return 0;
    }
    keymatrix matrix;
    keyb_getmatrix(&matrix);
    return (matrix.full & KB_MASK64(key)) != 0 &&
           (matrix.hiword & SHFTPLANE((uint32_t)shiftcode)) != 0;
}
int keyb_getnextkey(keymatrix *ptr, int previous)
{
    if (!ptr || previous < -1 || previous > 63) {
        errno = EINVAL;
        return 0;
    }
    for (int key = previous < 1 ? 1 : previous + 1; key < 60; ++key)
        if (ptr->full & KB_MASK64(key))
            return key;
    return 0;
}
int keyb_getkeyM(int wait)
{
    if (!model())
        return 0;
    const uint64_t ordinary = UINT64_C(0x8fffffffffffffff);
    keymatrix matrix;
    keyb_getmatrix(&matrix);
    while (wait && !(matrix.full & ordinary)) {
        hpclang_delay_us(1000);
        keyb_getmatrix(&matrix);
    }
    unsigned key = 0;
    while (key < 64 && !(matrix.full & KB_MASK64(key)))
        ++key;
    uint32_t plane = SHFTPLANE(matrix.hiword);
    while (wait && (matrix.full & ordinary)) {
        hpclang_delay_us(1000);
        keyb_getmatrix(&matrix);
    }
    if (wait && key >= 60)
        return KB_ON;
    if (key < 60)
        return (int)(key | plane);
    return key < 64 ? (int)key : 0;
}
int keyb_getkey(int wait)
{
    unsigned id = model();
    if (!id)
        return -1;
    const uint64_t ordinary = (UINT64_C(1) << 56) - 1;
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    while (wait && (state.bits & ordinary)) {
        hpclang_delay_us(1000);
        hpclang_keyboard_read(&state);
    }
    while (!(state.bits & ordinary)) {
        if (!wait)
            return -1;
        hpclang_delay_us(1000);
        hpclang_keyboard_read(&state);
    }
    unsigned scan = 0;
    while (!(state.bits & (UINT64_C(1) << scan)))
        ++scan;
    int code = (int)((scan % 7) * 16 + scan / 7);
    if (wait) {
        uint32_t modifiers = hpclang_keyboard_modifiers_from_bits(id, state.bits);
        if (modifiers & HPCLANG_KEYBOARD_MOD_ON)
            code |= 1 << 8;
        if (modifiers & HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT)
            code |= 1 << 9;
        if ((modifiers & HPCLANG_KEYBOARD_MOD_LEFT_SHIFT) ||
            (id <= HPCLANG_MODEL_HP40GS && (modifiers & HPCLANG_KEYBOARD_MOD_SHIFT)))
            code |= 1 << 10;
        if (modifiers & HPCLANG_KEYBOARD_MOD_ALPHA)
            code |= 1 << 11;
        do {
            hpclang_delay_us(1000);
            hpclang_keyboard_read(&state);
        } while (state.bits & ordinary);
    }
    return code;
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
