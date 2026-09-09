#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <hpclang/keyboard.h>
#include <hpclang/target.h>

#include "keyboard-key-map.h"

struct mapping {
    uint8_t scan;
    uint16_t key;
};

static const struct mapping measured[] = {
    {35u, HPCLANG_KEY_F1},
    {36u, HPCLANG_KEY_F2},
    {37u, HPCLANG_KEY_F3},
    {38u, HPCLANG_KEY_F4},
    {39u, HPCLANG_KEY_F5},
    {40u, HPCLANG_KEY_F6},
    {42u, HPCLANG_KEY_UP},
    {44u, HPCLANG_KEY_DOWN},
    {43u, HPCLANG_KEY_LEFT},
    {45u, HPCLANG_KEY_RIGHT},
    {6u, HPCLANG_KEY_ENTER},
    {27u, HPCLANG_KEY_0},
    {26u, HPCLANG_KEY_1},
    {19u, HPCLANG_KEY_2},
    {12u, HPCLANG_KEY_3},
    {25u, HPCLANG_KEY_4},
    {18u, HPCLANG_KEY_5},
    {11u, HPCLANG_KEY_6},
    {24u, HPCLANG_KEY_7},
    {17u, HPCLANG_KEY_8},
    {10u, HPCLANG_KEY_9},
    {20u, HPCLANG_KEY_PERIOD},
    {2u, HPCLANG_KEY_DIVIDE},
    {3u, HPCLANG_KEY_MULTIPLY},
    {4u, HPCLANG_KEY_MINUS},
    {5u, HPCLANG_KEY_PLUS},
    {57u, HPCLANG_KEY_ALPHA},
    {58u, HPCLANG_KEY_LEFT_SHIFT},
    {59u, HPCLANG_KEY_RIGHT_SHIFT},
    {56u, HPCLANG_KEY_ON},
};

static int verified_model(uint32_t model)
{
    for (size_t i = 0; i < sizeof(measured) / sizeof(measured[0]); ++i) {
        uint16_t key = hpclang_keyboard_key_from_scan(model, measured[i].scan);
        if (key != measured[i].key) {
            fprintf(
                stderr,
                "model %u scan %u: key %u, expected %u\n",
                model,
                measured[i].scan,
                key,
                measured[i].key);
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    const uint32_t verified[] = {
        HPCLANG_MODEL_HP39GS,
        HPCLANG_MODEL_HP40GS,
        HPCLANG_MODEL_HP48GII,
        HPCLANG_MODEL_HP49GPLUS,
        HPCLANG_MODEL_HP50G,
    };

    for (size_t i = 0; i < sizeof(verified) / sizeof(verified[0]); ++i)
        if (!verified_model(verified[i]))
            return EXIT_FAILURE;

    if (hpclang_keyboard_key_from_scan(HPCLANG_MODEL_HP39GPLUS, 35u) !=
            HPCLANG_KEY_UNKNOWN ||
        hpclang_keyboard_key_from_scan(HPCLANG_MODEL_UNKNOWN, 35u) !=
            HPCLANG_KEY_UNKNOWN ||
        hpclang_keyboard_key_from_scan(HPCLANG_MODEL_HP50G, 0u) !=
            HPCLANG_KEY_UNKNOWN ||
        hpclang_keyboard_key_from_scan(HPCLANG_MODEL_HP50G, 60u) !=
            HPCLANG_KEY_UNKNOWN)
        return EXIT_FAILURE;

    puts("PASS: measured model-neutral keyboard key map.");
    return EXIT_SUCCESS;
}
