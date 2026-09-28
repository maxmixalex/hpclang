#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <hpclang/keyboard.h>
#include <hpclang/target.h>

#include "keyboard-key-map.h"

struct mapping
{
    uint8_t scan;
    uint16_t key;
};

static const struct mapping common[] = {
    {35u, HPCLANG_KEY_F1},     {36u, HPCLANG_KEY_F2},    {37u, HPCLANG_KEY_F3},
    {38u, HPCLANG_KEY_F4},     {39u, HPCLANG_KEY_F5},    {40u, HPCLANG_KEY_F6},
    {42u, HPCLANG_KEY_UP},     {44u, HPCLANG_KEY_DOWN},  {43u, HPCLANG_KEY_LEFT},
    {45u, HPCLANG_KEY_RIGHT},  {6u, HPCLANG_KEY_ENTER},  {27u, HPCLANG_KEY_0},
    {26u, HPCLANG_KEY_1},      {19u, HPCLANG_KEY_2},     {12u, HPCLANG_KEY_3},
    {25u, HPCLANG_KEY_4},      {18u, HPCLANG_KEY_5},     {11u, HPCLANG_KEY_6},
    {24u, HPCLANG_KEY_7},      {17u, HPCLANG_KEY_8},     {10u, HPCLANG_KEY_9},
    {20u, HPCLANG_KEY_PERIOD}, {2u, HPCLANG_KEY_DIVIDE}, {3u, HPCLANG_KEY_MULTIPLY},
    {4u, HPCLANG_KEY_MINUS},   {5u, HPCLANG_KEY_PLUS},   {56u, HPCLANG_KEY_ON},
};

static int expect_map(uint32_t model, const struct mapping *map, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        uint16_t key = hpclang_keyboard_key_from_scan(model, map[i].scan);
        if (key != map[i].key) {
            fprintf(stderr, "model %u scan %u: key %u, expected %u\n", model, map[i].scan, key,
                    map[i].key);
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    const uint32_t verified[] = {
        HPCLANG_MODEL_HP39GPLUS, HPCLANG_MODEL_HP39GS,    HPCLANG_MODEL_HP40GS,
        HPCLANG_MODEL_HP48GII,   HPCLANG_MODEL_HP49GPLUS, HPCLANG_MODEL_HP50G,
    };

    for (size_t i = 0; i < sizeof(verified) / sizeof(verified[0]); ++i)
        if (!expect_map(verified[i], common, sizeof(common) / sizeof(common[0])))
            return EXIT_FAILURE;

    static const struct mapping hp39[] = {
        {0u, HPCLANG_KEY_HP39_DEL},         {1u, HPCLANG_KEY_HP39_LOG},
        {7u, HPCLANG_KEY_HP39_X_THETA_T},   {8u, HPCLANG_KEY_HP39_LN},
        {9u, HPCLANG_KEY_HP39_RIGHT_PAREN}, {13u, HPCLANG_KEY_HP39_NEGATE},
        {14u, HPCLANG_KEY_HP39_DERIVATIVE}, {15u, HPCLANG_KEY_HP39_TAN},
        {16u, HPCLANG_KEY_HP39_LEFT_PAREN}, {21u, HPCLANG_KEY_HP39_MATH},
        {22u, HPCLANG_KEY_HP39_COS},        {23u, HPCLANG_KEY_HP39_POWER},
        {28u, HPCLANG_KEY_HP39_VARS},       {29u, HPCLANG_KEY_HP39_SIN},
        {30u, HPCLANG_KEY_HP39_SQUARE},     {41u, HPCLANG_KEY_HP39_SYMB},
        {46u, HPCLANG_KEY_HP39_PLOT},       {47u, HPCLANG_KEY_HP39_NUM},
        {48u, HPCLANG_KEY_HP39_HOME},       {49u, HPCLANG_KEY_HP39_APLET},
        {50u, HPCLANG_KEY_HP39_VIEWS},      {57u, HPCLANG_KEY_HP39_COMMA},
        {58u, HPCLANG_KEY_ALPHA},           {59u, HPCLANG_KEY_HP39_SHIFT},
    };
    if (!expect_map(HPCLANG_MODEL_HP39GPLUS, hp39, sizeof(hp39) / sizeof(hp39[0])) ||
        !expect_map(HPCLANG_MODEL_HP39GS, hp39, sizeof(hp39) / sizeof(hp39[0])) ||
        !expect_map(HPCLANG_MODEL_HP40GS, hp39, sizeof(hp39) / sizeof(hp39[0])))
        return EXIT_FAILURE;

    static const struct mapping hp48[] = {
        {0u, HPCLANG_KEY_HP48_BACKSPACE},  {1u, HPCLANG_KEY_HP48_TAN},
        {7u, HPCLANG_KEY_HP48_SYMB},       {8u, HPCLANG_KEY_HP48_COS},
        {9u, HPCLANG_KEY_HP48_RECIPROCAL}, {13u, HPCLANG_KEY_HP48_SPACE},
        {14u, HPCLANG_KEY_HP48_QUOTE},     {15u, HPCLANG_KEY_HP48_SIN},
        {16u, HPCLANG_KEY_HP48_X},         {21u, HPCLANG_KEY_HP48_EVAL},
        {22u, HPCLANG_KEY_HP48_SQRT},      {23u, HPCLANG_KEY_HP48_NEGATE},
        {28u, HPCLANG_KEY_HP48_HIST},      {29u, HPCLANG_KEY_HP48_POWER},
        {30u, HPCLANG_KEY_HP48_EEX},       {41u, HPCLANG_KEY_HP48_APPS},
        {46u, HPCLANG_KEY_HP48_MODE},      {47u, HPCLANG_KEY_HP48_TOOL},
        {48u, HPCLANG_KEY_HP48_VAR},       {49u, HPCLANG_KEY_HP48_STO},
        {50u, HPCLANG_KEY_HP48_NXT},       {57u, HPCLANG_KEY_ALPHA},
        {58u, HPCLANG_KEY_LEFT_SHIFT},     {59u, HPCLANG_KEY_RIGHT_SHIFT},
    };
    if (!expect_map(HPCLANG_MODEL_HP48GII, hp48, sizeof(hp48) / sizeof(hp48[0])))
        return EXIT_FAILURE;

    static const struct mapping hp4950_shared[] = {
        {0u, HPCLANG_KEY_HP4950_BACKSPACE},  {1u, HPCLANG_KEY_HP4950_TAN},
        {7u, HPCLANG_KEY_HP4950_SYMB},       {8u, HPCLANG_KEY_HP4950_COS},
        {9u, HPCLANG_KEY_HP4950_RECIPROCAL}, {13u, HPCLANG_KEY_HP4950_SPACE},
        {15u, HPCLANG_KEY_HP4950_SIN},       {16u, HPCLANG_KEY_HP4950_X},
        {22u, HPCLANG_KEY_HP4950_SQRT},      {23u, HPCLANG_KEY_HP4950_NEGATE},
        {28u, HPCLANG_KEY_HP4950_HIST},      {29u, HPCLANG_KEY_HP4950_POWER},
        {30u, HPCLANG_KEY_HP4950_EEX},       {41u, HPCLANG_KEY_HP4950_APPS},
        {46u, HPCLANG_KEY_HP4950_MODE},      {47u, HPCLANG_KEY_HP4950_TOOL},
        {48u, HPCLANG_KEY_HP4950_VAR},       {49u, HPCLANG_KEY_HP4950_STO},
        {50u, HPCLANG_KEY_HP4950_NXT},       {57u, HPCLANG_KEY_ALPHA},
        {58u, HPCLANG_KEY_LEFT_SHIFT},       {59u, HPCLANG_KEY_RIGHT_SHIFT},
    };
    if (!expect_map(HPCLANG_MODEL_HP49GPLUS, hp4950_shared,
                    sizeof(hp4950_shared) / sizeof(hp4950_shared[0])) ||
        !expect_map(HPCLANG_MODEL_HP50G, hp4950_shared,
                    sizeof(hp4950_shared) / sizeof(hp4950_shared[0])))
        return EXIT_FAILURE;

    static const struct mapping hp49_unique[] = {
        {14u, HPCLANG_KEY_HP49_EQW},
        {21u, HPCLANG_KEY_HP49_CAT},
    };
    static const struct mapping hp50_unique[] = {
        {14u, HPCLANG_KEY_HP50_QUOTE},
        {21u, HPCLANG_KEY_HP50_EVAL},
    };
    if (!expect_map(HPCLANG_MODEL_HP49GPLUS, hp49_unique,
                    sizeof(hp49_unique) / sizeof(hp49_unique[0])) ||
        !expect_map(HPCLANG_MODEL_HP50G, hp50_unique, sizeof(hp50_unique) / sizeof(hp50_unique[0])))
        return EXIT_FAILURE;

    uint64_t hp39_bits =
        (UINT64_C(1) << HPCLANG_KEYBOARD_ON) | (UINT64_C(1) << HPCLANG_KEYBOARD_GPF4) |
        (UINT64_C(1) << HPCLANG_KEYBOARD_GPF5) | (UINT64_C(1) << HPCLANG_KEYBOARD_GPF6);
    const uint32_t hp39_models[] = {
        HPCLANG_MODEL_HP39GPLUS,
        HPCLANG_MODEL_HP39GS,
        HPCLANG_MODEL_HP40GS,
    };
    for (size_t i = 0; i < sizeof(hp39_models) / sizeof(hp39_models[0]); ++i) {
        uint32_t hp39_mod = hpclang_keyboard_modifiers_from_bits(hp39_models[i], hp39_bits);
        if (hp39_mod !=
            (HPCLANG_KEYBOARD_MOD_ON | HPCLANG_KEYBOARD_MOD_ALPHA | HPCLANG_KEYBOARD_MOD_SHIFT))
            return EXIT_FAILURE;
    }

    uint32_t hp50_mod = hpclang_keyboard_modifiers_from_bits(HPCLANG_MODEL_HP50G, hp39_bits);
    if (hp50_mod !=
        (HPCLANG_KEYBOARD_MOD_ON | HPCLANG_KEYBOARD_MOD_ALPHA | HPCLANG_KEYBOARD_MOD_SHIFT |
         HPCLANG_KEYBOARD_MOD_LEFT_SHIFT | HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT))
        return EXIT_FAILURE;

    if (hpclang_keyboard_key_from_scan(HPCLANG_MODEL_UNKNOWN, 35u) != HPCLANG_KEY_UNKNOWN ||
        hpclang_keyboard_key_from_scan(HPCLANG_MODEL_HP50G, 31u) != HPCLANG_KEY_UNKNOWN ||
        hpclang_keyboard_key_from_scan(HPCLANG_MODEL_HP50G, 60u) != HPCLANG_KEY_UNKNOWN)
        return EXIT_FAILURE;

    puts("PASS: complete validated keyboard key maps.");
    return EXIT_SUCCESS;
}
