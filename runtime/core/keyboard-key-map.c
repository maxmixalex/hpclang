#include <stdint.h>

#include <hpclang/keyboard.h>
#include <hpclang/target.h>

#include "keyboard-key-map.h"

static const uint16_t common_key_map[60] = {
    [2] = HPCLANG_KEY_DIVIDE,
    [3] = HPCLANG_KEY_MULTIPLY,
    [4] = HPCLANG_KEY_MINUS,
    [5] = HPCLANG_KEY_PLUS,
    [6] = HPCLANG_KEY_ENTER,
    [10] = HPCLANG_KEY_9,
    [11] = HPCLANG_KEY_6,
    [12] = HPCLANG_KEY_3,
    [17] = HPCLANG_KEY_8,
    [18] = HPCLANG_KEY_5,
    [19] = HPCLANG_KEY_2,
    [20] = HPCLANG_KEY_PERIOD,
    [24] = HPCLANG_KEY_7,
    [25] = HPCLANG_KEY_4,
    [26] = HPCLANG_KEY_1,
    [27] = HPCLANG_KEY_0,
    [35] = HPCLANG_KEY_F1,
    [36] = HPCLANG_KEY_F2,
    [37] = HPCLANG_KEY_F3,
    [38] = HPCLANG_KEY_F4,
    [39] = HPCLANG_KEY_F5,
    [40] = HPCLANG_KEY_F6,
    [42] = HPCLANG_KEY_UP,
    [43] = HPCLANG_KEY_LEFT,
    [44] = HPCLANG_KEY_DOWN,
    [45] = HPCLANG_KEY_RIGHT,
    [56] = HPCLANG_KEY_ON,
    [57] = HPCLANG_KEY_ALPHA,
    [58] = HPCLANG_KEY_LEFT_SHIFT,
    [59] = HPCLANG_KEY_RIGHT_SHIFT,
};

static int model_has_verified_key_map(uint32_t model)
{
    return model == HPCLANG_MODEL_HP39GS ||
           model == HPCLANG_MODEL_HP40GS ||
           model == HPCLANG_MODEL_HP48GII ||
           model == HPCLANG_MODEL_HP49GPLUS ||
           model == HPCLANG_MODEL_HP50G;
}

uint16_t hpclang_keyboard_key_from_scan(
    uint32_t model,
    uint8_t scan_code)
{
    if (!model_has_verified_key_map(model) ||
        scan_code >= sizeof(common_key_map) / sizeof(common_key_map[0]))
        return HPCLANG_KEY_UNKNOWN;

    return common_key_map[scan_code];
}
