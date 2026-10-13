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
    return 0;
}
