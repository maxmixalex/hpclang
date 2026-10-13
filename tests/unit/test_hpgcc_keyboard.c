#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <hpkeyb49.h>
#include <hpclang/abi.h>
#include <hpclang/keyboard.h>
#include <hpclang/target.h>

static struct hpclang_boot_info boot;
static uint64_t samples[8];
static unsigned count = 1, index, reads, delays;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &boot;
}
void hpclang_keyboard_read(struct hpclang_keyboard_state *state)
{
    ++reads;
    state->bits = samples[index];
    if (index + 1 < count)
        ++index;
}
void hpclang_delay_us(uint64_t us)
{
    assert(us == 1000);
    ++delays;
}

int main(void)
{
    assert(!keyb_isAnyKeyPressed() && errno == ENOSYS && !reads);
    keyb_waitKeyPressed();
    assert(errno == ENOSYS && !reads && !delays);
    for (boot.model = 1; boot.model <= 6; ++boot.model) {
        for (unsigned col = 0; col < 8; ++col)
            for (unsigned row = 0; row < 7; ++row) {
                samples[0] = UINT64_C(1) << (col * 7 + row);
                assert(keyb_isAnyKeyPressed());
                assert(keyb_isKeyPressed(col, row));
                assert(!keyb_isKeyPressed(col, (row + 1) % 7));
                keymatrix matrix;
                keyb_getmatrix(&matrix);
                assert(matrix.full == (UINT64_C(1) << (col * 8 + row + 1)));
                assert(keyb_isKeyPrM((int)(col * 8 + row + 1)));
            }
        for (unsigned scan = 56; scan < 60; ++scan) {
            samples[0] = UINT64_C(1) << scan;
            keymatrix matrix;
            keyb_getmatrix(&matrix);
            assert(matrix.full == (UINT64_C(1) << (scan == 56 ? 63 : scan + 3)));
        }
        samples[0] = UINT64_C(1) << 42;
        assert(keyb_isUp() && !keyb_isDown());
        samples[0] = UINT64_C(1) << 43;
        assert(keyb_isLeft() && !keyb_isRight());
        samples[0] = UINT64_C(1) << 44;
        assert(keyb_isDown() && !keyb_isUp());
        samples[0] = UINT64_C(1) << 45;
        assert(keyb_isRight() && !keyb_isLeft());
        samples[0] = UINT64_C(1) << 56;
        assert(keyb_isON());
        samples[0] = UINT64_C(1) << (boot.model <= 3 ? 58 : 57);
        assert(keyb_isAlpha() && !keyb_isLS() && !keyb_isRS());
        samples[0] = UINT64_C(1) << (boot.model <= 3 ? 59 : 58);
        assert(keyb_isLS() && !keyb_isAlpha() && !keyb_isRS());
        samples[0] = boot.model <= 3 ? UINT64_C(1) << 57 : UINT64_C(1) << 59;
        assert(keyb_isRS() == (boot.model > 3));
        samples[0] = 0;
        assert(!keyb_isAnyKeyPressed());
    }
    boot.model = HPCLANG_MODEL_HP50G;
    unsigned before = reads;
    assert(!keyb_isKeyPressed(8, 0) && errno == EINVAL);
    assert(!keyb_isKeyPressed(0, 7) && errno == EINVAL && reads == before);
    assert(!keyb_isKeyPrM(-1) && errno == EINVAL);
    assert(!keyb_isKeyPrM(64) && errno == EINVAL && reads == before);
    keyb_getmatrix(NULL);
    assert(errno == EINVAL && reads == before);
    count = 4;
    index = 0;
    samples[0] = 0;
    samples[1] = 1;
    samples[2] = 3;
    samples[3] = 0;
    keyb_waitKeyPressed();
    assert(index == 3 && delays == 3 && reads == before + 4);
    count = 2;
    index = 0;
    samples[0] = 1;
    samples[1] = 0;
    keyb_waitKeyPressed();
    assert(index == 1 && delays == 4 && reads == before + 6);
    count = 1;
    index = 0;
    samples[0] = (UINT64_C(1) << 35) | (UINT64_C(1) << 58);
    assert(keyb_isShiftKeyPrM(KB_A | SHFT_RS));
    assert(!keyb_isShiftKeyPrM(KB_A | SHFT_LS));
    assert(keyb_isShiftKeyPrM(KB_A | SHFT_ANY));
    assert(!keyb_isShiftKeyPrM(KB_A));
    assert(!keyb_isShiftKeyPrM(255) && errno == EINVAL);
    assert(keyb_getkeyM(0) == (int)(KB_A | SHFT_RS));
    samples[0] = UINT64_C(1) << 58;
    assert(keyb_getkeyM(0) == KB_LSHIFT);
    samples[0] = 0;
    assert(!keyb_getkeyM(0));
    keymatrix matrix = {.full = KB_MASK64(KB_BKS) | KB_MASK64(KB_A) | KB_MASK64(KB_ON)};
    before = reads;
    assert(keyb_getnextkey(&matrix, -1) == KB_BKS);
    assert(keyb_getnextkey(&matrix, KB_BKS) == KB_A);
    assert(!keyb_getnextkey(&matrix, KB_A));
    assert(!keyb_getnextkey(&matrix, 63));
    assert(!keyb_getnextkey(&matrix, 64) && errno == EINVAL);
    assert(!keyb_getnextkey(&matrix, -2) && errno == EINVAL);
    assert(!keyb_getnextkey(NULL, -1) && errno == EINVAL && reads == before);
    count = 4;
    samples[0] = UINT64_C(1) << 58;
    samples[1] = samples[0] | (UINT64_C(1) << 35);
    samples[2] = samples[1] | (UINT64_C(1) << 36);
    samples[3] = samples[0];
    assert(keyb_getkeyM(1) == (int)(KB_A | SHFT_RS));
    assert(index == 3);
    index = 0;
    count = 2;
    samples[0] = (UINT64_C(1) << 58) | (UINT64_C(1) << 56);
    samples[1] = UINT64_C(1) << 58;
    assert(keyb_getkeyM(1) == KB_ON && index == 1);
    count = 1;
    index = 0;
    for (unsigned col = 0; col < 8; ++col)
        for (unsigned row = 0; row < 7; ++row) {
            samples[0] = UINT64_C(1) << (col * 7 + row);
            assert(keyb_getkey(0) == (int)(row * 16 + col));
        }
    samples[0] = UINT64_C(1) << 58;
    assert(keyb_getkey(0) == -1);
    for (boot.model = 1; boot.model <= 6; ++boot.model) {
        index = 0;
        count = 4;
        uint64_t shift = UINT64_C(1) << (boot.model <= 3 ? 59 : 58);
        samples[0] = shift;
        samples[1] = shift | (UINT64_C(1) << 35);
        samples[2] = shift | (UINT64_C(1) << 36);
        samples[3] = shift;
        assert(keyb_getkey(1) == (5 | (1 << 10)) && index == 3);
    }
    boot.model = HPCLANG_MODEL_HP50G;
    index = 0;
    count = 4;
    samples[0] = UINT64_C(1) << 36;
    samples[1] = 0;
    samples[2] = UINT64_C(1) << 35;
    samples[3] = 0;
    assert(keyb_getkey(1) == 5 && index == 3);
    boot.model = 0;
    before = reads;
    assert(!keyb_getkeyM(1) && errno == ENOSYS && reads == before);
    assert(keyb_getkey(1) == -1 && errno == ENOSYS && reads == before);
    return 0;
}
