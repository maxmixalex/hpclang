#ifndef HPCLANG_KEYBOARD_KEY_MAP_H
#define HPCLANG_KEYBOARD_KEY_MAP_H

#include <stdint.h>

uint16_t hpclang_keyboard_key_from_scan(
    uint32_t model,
    uint8_t scan_code);

uint32_t hpclang_keyboard_modifiers_from_bits(
    uint32_t model,
    uint64_t bits);

#endif
