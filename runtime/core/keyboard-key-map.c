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
};

static const uint16_t hp39_key_map[60] = {
    [0] = HPCLANG_KEY_HP39_DEL,
    [1] = HPCLANG_KEY_HP39_LOG,
    [7] = HPCLANG_KEY_HP39_X_THETA_T,
    [8] = HPCLANG_KEY_HP39_LN,
    [9] = HPCLANG_KEY_HP39_RIGHT_PAREN,
    [13] = HPCLANG_KEY_HP39_NEGATE,
    [14] = HPCLANG_KEY_HP39_DERIVATIVE,
    [15] = HPCLANG_KEY_HP39_TAN,
    [16] = HPCLANG_KEY_HP39_LEFT_PAREN,
    [21] = HPCLANG_KEY_HP39_MATH,
    [22] = HPCLANG_KEY_HP39_COS,
    [23] = HPCLANG_KEY_HP39_POWER,
    [28] = HPCLANG_KEY_HP39_VARS,
    [29] = HPCLANG_KEY_HP39_SIN,
    [30] = HPCLANG_KEY_HP39_SQUARE,
    [41] = HPCLANG_KEY_HP39_SYMB,
    [46] = HPCLANG_KEY_HP39_PLOT,
    [47] = HPCLANG_KEY_HP39_NUM,
    [48] = HPCLANG_KEY_HP39_HOME,
    [49] = HPCLANG_KEY_HP39_APLET,
    [50] = HPCLANG_KEY_HP39_VIEWS,
    [57] = HPCLANG_KEY_HP39_COMMA,
    [58] = HPCLANG_KEY_ALPHA,
    [59] = HPCLANG_KEY_HP39_SHIFT,
};

static const uint16_t hp48_key_map[60] = {
    [0] = HPCLANG_KEY_HP48_BACKSPACE,
    [1] = HPCLANG_KEY_HP48_TAN,
    [7] = HPCLANG_KEY_HP48_SYMB,
    [8] = HPCLANG_KEY_HP48_COS,
    [9] = HPCLANG_KEY_HP48_RECIPROCAL,
    [13] = HPCLANG_KEY_HP48_SPACE,
    [14] = HPCLANG_KEY_HP48_QUOTE,
    [15] = HPCLANG_KEY_HP48_SIN,
    [16] = HPCLANG_KEY_HP48_X,
    [21] = HPCLANG_KEY_HP48_EVAL,
    [22] = HPCLANG_KEY_HP48_SQRT,
    [23] = HPCLANG_KEY_HP48_NEGATE,
    [28] = HPCLANG_KEY_HP48_HIST,
    [29] = HPCLANG_KEY_HP48_POWER,
    [30] = HPCLANG_KEY_HP48_EEX,
    [41] = HPCLANG_KEY_HP48_APPS,
    [46] = HPCLANG_KEY_HP48_MODE,
    [47] = HPCLANG_KEY_HP48_TOOL,
    [48] = HPCLANG_KEY_HP48_VAR,
    [49] = HPCLANG_KEY_HP48_STO,
    [50] = HPCLANG_KEY_HP48_NXT,
    [57] = HPCLANG_KEY_ALPHA,
    [58] = HPCLANG_KEY_LEFT_SHIFT,
    [59] = HPCLANG_KEY_RIGHT_SHIFT,
};

static const uint16_t hp4950_key_map[60] = {
    [0] = HPCLANG_KEY_HP4950_BACKSPACE,
    [1] = HPCLANG_KEY_HP4950_TAN,
    [7] = HPCLANG_KEY_HP4950_SYMB,
    [8] = HPCLANG_KEY_HP4950_COS,
    [9] = HPCLANG_KEY_HP4950_RECIPROCAL,
    [13] = HPCLANG_KEY_HP4950_SPACE,
    [14] = HPCLANG_KEY_HP4950_QUOTE,
    [15] = HPCLANG_KEY_HP4950_SIN,
    [16] = HPCLANG_KEY_HP4950_X,
    [21] = HPCLANG_KEY_HP4950_EVAL,
    [22] = HPCLANG_KEY_HP4950_SQRT,
    [23] = HPCLANG_KEY_HP4950_NEGATE,
    [28] = HPCLANG_KEY_HP4950_HIST,
    [29] = HPCLANG_KEY_HP4950_POWER,
    [30] = HPCLANG_KEY_HP4950_EEX,
    [41] = HPCLANG_KEY_HP4950_APPS,
    [46] = HPCLANG_KEY_HP4950_MODE,
    [47] = HPCLANG_KEY_HP4950_TOOL,
    [48] = HPCLANG_KEY_HP4950_VAR,
    [49] = HPCLANG_KEY_HP4950_STO,
    [50] = HPCLANG_KEY_HP4950_NXT,
    [57] = HPCLANG_KEY_ALPHA,
    [58] = HPCLANG_KEY_LEFT_SHIFT,
    [59] = HPCLANG_KEY_RIGHT_SHIFT,
};

static const uint16_t *model_specific_map(uint32_t model)
{
    if (model == HPCLANG_MODEL_HP39GS || model == HPCLANG_MODEL_HP40GS)
        return hp39_key_map;
    if (model == HPCLANG_MODEL_HP48GII)
        return hp48_key_map;
    if (model == HPCLANG_MODEL_HP49GPLUS || model == HPCLANG_MODEL_HP50G)
        return hp4950_key_map;
    return 0;
}

uint16_t hpclang_keyboard_key_from_scan(
    uint32_t model,
    uint8_t scan_code)
{
    if (scan_code >= 60u)
        return HPCLANG_KEY_UNKNOWN;

    const uint16_t *specific = model_specific_map(model);
    if (!specific)
        return HPCLANG_KEY_UNKNOWN;

    if (common_key_map[scan_code] != HPCLANG_KEY_UNKNOWN)
        return common_key_map[scan_code];

    return specific[scan_code];
}

uint32_t hpclang_keyboard_modifiers_from_bits(
    uint32_t model,
    uint64_t bits)
{
    uint32_t value = 0u;

    if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_ON)) != 0u)
        value |= HPCLANG_KEYBOARD_MOD_ON;

    if (model == HPCLANG_MODEL_HP39GS || model == HPCLANG_MODEL_HP40GS) {
        if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_GPF5)) != 0u)
            value |= HPCLANG_KEYBOARD_MOD_ALPHA;
        if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_GPF6)) != 0u)
            value |= HPCLANG_KEYBOARD_MOD_SHIFT;
        return value;
    }

    if (model == HPCLANG_MODEL_HP48GII ||
        model == HPCLANG_MODEL_HP49GPLUS ||
        model == HPCLANG_MODEL_HP50G) {
        if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_GPF4)) != 0u)
            value |= HPCLANG_KEYBOARD_MOD_ALPHA;
        if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_GPF5)) != 0u)
            value |= HPCLANG_KEYBOARD_MOD_LEFT_SHIFT;
        if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_GPF6)) != 0u)
            value |= HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT;
    }

    return value;
}
