#include "rpl-recovery-identity.h"
#include <string.h>

struct hash_state
{
    uint32_t h[8];
    uint8_t block[64];
    unsigned used;
    uint32_t bytes;
};

static uint32_t rotate(uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32u - bits));
}

static void compress(struct hash_state *state)
{
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    uint32_t words[16];
    for (unsigned i = 0; i < 16; ++i) {
        const uint8_t *p = state->block + 4 * i;
        words[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    }
    uint32_t a = state->h[0], b = state->h[1], c = state->h[2], d = state->h[3];
    uint32_t e = state->h[4], f = state->h[5], g = state->h[6], h = state->h[7];
    for (unsigned i = 0; i < 64; ++i) {
        if (i >= 16) {
            uint32_t x = words[(i - 15) & 15], y = words[(i - 2) & 15];
            words[i & 15] += (rotate(x, 7) ^ rotate(x, 18) ^ (x >> 3)) + words[(i - 7) & 15] +
                             (rotate(y, 17) ^ rotate(y, 19) ^ (y >> 10));
        }
        uint32_t first = h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) +
                         k[i] + words[i & 15];
        uint32_t second =
            (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }
    state->h[0] += a;
    state->h[1] += b;
    state->h[2] += c;
    state->h[3] += d;
    state->h[4] += e;
    state->h[5] += f;
    state->h[6] += g;
    state->h[7] += h;
}

static void append(struct hash_state *state, uint8_t byte)
{
    state->block[state->used++] = byte;
    ++state->bytes;
    if (state->used == 64) {
        compress(state);
        state->used = 0;
    }
}

static uint32_t little_word(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void finish(struct hash_state *hash, uint8_t digest[32])
{
    uint32_t bits = hash->bytes * 8;
    append(hash, 0x80);
    while (hash->used != 56)
        append(hash, 0);
    for (unsigned i = 0; i < 4; ++i)
        append(hash, 0);
    for (unsigned i = 0; i < 4; ++i)
        append(hash, (uint8_t)(bits >> (24 - 8 * i)));
    for (unsigned i = 0; i < 32; ++i)
        digest[i] = (uint8_t)(hash->h[i / 4] >> (24 - 8 * (i & 3)));
}

static uint8_t packed_nibble(const uint8_t *object, uint32_t at)
{
    return (object[at / 2] >> (4 * (at & 1))) & 15;
}

static void put_nibble(uint8_t *object, uint32_t at, uint8_t value)
{
    unsigned shift = 4 * (at & 1);
    object[at / 2] = (uint8_t)((object[at / 2] & ~(15u << shift)) | value << shift);
}

int hpclang_rpl_recovery_identity_seal(uint8_t *object, uint32_t bytes, uint32_t at)
{
    if (!object || bytes < 5)
        return 0;
    uint32_t prologue = 0, length = 0;
    for (unsigned i = 0; i < 5; ++i) {
        prologue |= (uint32_t)packed_nibble(object, i) << (4 * i);
        length |= (uint32_t)packed_nibble(object, i + 5) << (4 * i);
    }
    uint32_t span = length + 5;
    if (prologue != 0x2b40 || span < 110 || (span + 1) / 2 > bytes || at < 10 || at > span - 100)
        return 0;
    const uint8_t marker[8] = "HPCROOT1";
    for (unsigned i = 0; i < 96; ++i) {
        uint8_t wanted = i < 16 ? (marker[i / 2] >> (4 * (i & 1))) & 15 : 0;
        if (packed_nibble(object, at + i) != wanted)
            return 0;
    }
    /* Every failure check precedes edits to this owned memory buffer. */
    for (unsigned i = 0; i < 8; ++i) {
        put_nibble(object, at + 16 + i, (uint8_t)((span >> (4 * i)) & 15));
        put_nibble(object, at + 24 + i, (uint8_t)((at >> (4 * i)) & 15));
    }
    struct hash_state hash = {.h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f,
                                    0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    uint8_t packed = 0;
    for (uint32_t i = 0; i < span; ++i) {
        uint8_t nibble = i >= span - 4 ? 0 : packed_nibble(object, i);
        if (!(i & 1))
            packed = nibble;
        else
            append(&hash, (uint8_t)(packed | nibble << 4));
    }
    if (span & 1)
        append(&hash, packed);
    uint8_t digest[32];
    finish(&hash, digest);
    for (unsigned i = 0; i < 64; ++i)
        put_nibble(object, at + 32 + i, (digest[i / 2] >> (4 * (i & 1))) & 15);
    uint32_t crc = 0;
    for (uint32_t i = 5; i < span - 4; ++i)
        crc = (crc >> 4) ^ (((crc ^ packed_nibble(object, i)) & 15) * 0x1081);
    for (unsigned i = 0; i < 4; ++i)
        put_nibble(object, span - 4 + i, (crc >> (4 * i)) & 15);
    return 1;
}

int hpclang_rpl_recovery_identity_match(const struct hpclang_rpl_reader *reader, uint32_t object,
                                        const uint8_t expected[48])
{
    if (!reader || !reader->read_nibble || !expected || memcmp(expected, "HPCROOT1", 8))
        return -1;
    uint32_t span = little_word(expected + 8), at = little_word(expected + 12);
    if (span < 110 || span > 0x100004u || object > UINT32_MAX - span || at < 10 || at > span - 100)
        return -1;
    struct hash_state hash = {.h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f,
                                    0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    uint32_t prologue = 0, length = 0, crc = 0, stored_crc = 0;
    uint8_t packed = 0;
    for (uint32_t i = 0; i < span; ++i) {
        uint8_t value;
        if (!reader->read_nibble(reader->context, object + i, &value) || value > 15)
            return -1;
        if (i < 5)
            prologue |= (uint32_t)value << (4 * i);
        else if (i < 10)
            length |= (uint32_t)value << (4 * (i - 5));
        if (i >= at && i - at < 96 &&
            value != ((expected[(i - at) / 2] >> (4 * ((i - at) & 1))) & 15))
            return 0;
        if (i >= 5 && i < span - 4)
            crc = (crc >> 4) ^ (((crc ^ value) & 15) * 0x1081);
        if (i >= span - 4)
            stored_crc |= (uint32_t)value << (4 * (i - (span - 4)));
        if ((i >= at + 32 && i < at + 96) || i >= span - 4)
            value = 0;
        if (!(i & 1))
            packed = value;
        else
            append(&hash, (uint8_t)(packed | value << 4));
    }
    if (prologue != 0x2b40 || length + 5 != span || crc != stored_crc)
        return 0;
    if (span & 1)
        append(&hash, packed);
    uint8_t digest[32];
    finish(&hash, digest);
    for (unsigned i = 0; i < 32; ++i)
        if (expected[16 + i] != digest[i])
            return 0;
    return 1;
}
