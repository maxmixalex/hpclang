#ifndef HPCLANG_HP_CHARACTER_H
#define HPCLANG_HP_CHARACTER_H
#include <stdint.h>

/* HP's byte repertoire uses Latin-1 plus calculator symbols in 0x80..0xa0,
 * and beta at 0xdf. Keep text input and native/packaged glyph selection aligned. */
static const uint16_t hpclang_hp_symbols[] = {
    0x2220, 0x0078, 0x2207, 0x221a, 0x222b, 0x03a3, 0x25b6, 0x03c0, 0x2202, 0x2264, 0x2265,
    0x2260, 0x03b1, 0x2192, 0x2190, 0x2193, 0x2191, 0x03b3, 0x03b4, 0x03b5, 0x03b7, 0x03b8,
    0x03bb, 0x03c1, 0x03c3, 0x03c4, 0x03c9, 0x0394, 0x03a0, 0x03a9, 0x25a0, 0x221e, 0x20ac};

static inline uint32_t hpclang_hp_unicode_from_byte(unsigned char byte)
{
    if (byte >= 0x80 && byte <= 0xa0)
        return hpclang_hp_symbols[byte - 0x80];
    return byte == 0xdf ? 0x03b2u : byte;
}
/* Prefer ordinary byte spellings for aliases such as x; return -1 rather than
 * silently replacing an unrepresentable input scalar with a question mark. */
static inline int hpclang_hp_byte_from_unicode(uint32_t codepoint)
{
    if (codepoint <= 0xff)
        return (int)codepoint;
    if (codepoint == 0x03b2)
        return 0xdf;
    if (codepoint == 0x03bc)
        return 0xb5;
    for (unsigned i = 0; i < sizeof hpclang_hp_symbols / sizeof hpclang_hp_symbols[0]; ++i)
        if (hpclang_hp_symbols[i] == codepoint)
            return (int)i + 0x80;
    return -1;
}
#endif
