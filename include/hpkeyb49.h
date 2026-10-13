#ifndef HPCLANG_HPKEYB49_H
#define HPCLANG_HPKEYB49_H

#include <stdint.h>

#ifndef HPCLANG_HPGCC_LONG_TYPES_DEFINED
#define HPCLANG_HPGCC_LONG_TYPES_DEFINED
typedef unsigned long long ULONGLONG;
typedef long long LONGLONG;
#endif

/* Historical numeric namespace, not Unicode or model-neutral key labels. */
#define KB_A 41
#define KB_B 42
#define KB_C 43
#define KB_D 44
#define KB_E 45
#define KB_F 46
#define KB_G 47
#define KB_H 53
#define KB_I 54
#define KB_J 55
#define KB_K 57
#define KB_L 58
#define KB_M 33
#define KB_N 25
#define KB_O 17
#define KB_P 9
#define KB_Q 34
#define KB_R 26
#define KB_S 18
#define KB_T 10
#define KB_U 2
#define KB_V 35
#define KB_W 27
#define KB_X 19
#define KB_Y 11
#define KB_Z KB_DIV
#define KB_0 31
#define KB_1 30
#define KB_2 22
#define KB_3 14
#define KB_4 29
#define KB_5 21
#define KB_6 13
#define KB_7 28
#define KB_8 20
#define KB_9 12
#define KB_BKS 1
#define KB_DIV 3
#define KB_MUL 4
#define KB_ADD 6
#define KB_SUB 5
#define KB_DOT 23
#define KB_SPC 15
#define KB_ENT 7
#define KB_UP 49
#define KB_DN 51
#define KB_LF 50
#define KB_RT 52
#define KB_ALPHA 60
#define KB_LSHIFT 61
#define KB_RSHIFT 62
#define KB_ON 63
#define KB_MASK64(a) (UINT64_C(1) << (a))
#define KB_MASK32(a) (UINT32_C(1) << ((a) & 31))
#define KB_MASK8(a) (UINT32_C(1) << ((a) & 7))
#define KB_CURS UINT64_C(0x001e000000000000)
#define KBH_CURS32 UINT32_C(0x001e0000)
#define KB_FUNC UINT64_C(0x00007e0000000000)
#define KBH_FUNC32 UINT32_C(0x00007e00)
/* These preserve historical values, including the LS/RS naming reversal
 * relative to KB_LSHIFT/KB_RSHIFT in the raw snapshot. */
#define SHFT_LS UINT32_C(0x40000000)
#define SHFT_RS UINT32_C(0x20000000)
#define SHFT_ALPHA UINT32_C(0x10000000)
#define SHFT_ON UINT32_C(0x80000000)
#define SHFT_ANY UINT32_C(0xf0000000)
#define KEYCODE(code) ((code) & 0xff)
#define SHFTPLANE(code) ((code) & SHFT_ANY)

#ifdef __cplusplus
extern "C"
{
#endif
    /* Historical byte-column snapshot, distinct from hpclang's packed raw
     * state. Bits 60..62 retain physical GPF4/5/6, and bit 63 is ON. These
     * positions do not acquire classic-model labels on an HP39 keyboard. */
    typedef union
    {
        uint64_t full;
        struct
        {
            unsigned int loword, hiword;
        };
        unsigned char bytes[8];
    } keymatrix;
    void keyb_getmatrix(keymatrix *ptr);
    int keyb_isKeyPrM(int keycode);
    /* Requires the key and at least one requested modifier. Zero modifier
     * bits match nothing, as in HPGCC. */
    int keyb_isShiftKeyPrM(int shiftcode);
    /* Iterate snapshot bits 1..59; modifiers/ON are inspected separately.
     * Pass -1 initially. Zero means end. Invalid arguments set EINVAL. */
    int keyb_getnextkey(keymatrix *ptr, int previous);
    /* Nonblocking: first set bit, or zero. Blocking: wait for a non-modifier
     * (ON included), then for all non-modifiers to be released. Modifiers may
     * stay held. Low-key results include the original raw high-word plane. */
    int keyb_getkeyM(int wait);
    /* Historical implemented numbering: row * 16 + column. Nonblocking
     * returns -1 without an ordinary matrix key. Blocking waits for a fresh key,
     * waits for all ordinary matrix keys to be released, and includes held
     * ON/RS/LS/Alpha in bits 8/9/10/11. No RTC configuration is changed. */
    int keyb_getkey(int wait);
    /* Historical physical-state queries. Matrix coordinates are zero-based
     * hardware column/row, not character positions. Named queries follow the
     * current model: HP39's single Shift is LS; RS is absent. No shift/alpha latch
     * or translated text state is installed or changed by this adapter.
     * Invalid coordinates return 0/EINVAL; unknown models return 0/ENOSYS. */
    int keyb_isKeyPressed(unsigned char col, unsigned char row);
    int keyb_isAnyKeyPressed(void);
    int keyb_isLeft(void);
    int keyb_isRight(void);
    int keyb_isUp(void);
    int keyb_isDown(void);
    int keyb_isAlpha(void);
    int keyb_isLS(void);
    int keyb_isRS(void);
    int keyb_isON(void);
    /* Wait for any press followed by all keys released. An already-held key
     * qualifies. Does not consume the clean event queue or change RTC settings.
     * Unknown model returns immediately with ENOSYS. */
    void keyb_waitKeyPressed(void);
#ifdef __cplusplus
}
#endif
#endif
