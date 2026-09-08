#include <stdint.h>

#include <hpclang/keyboard.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define GPFDAT REG32(0x07a00054u)
#define GPGCON REG32(0x07a00060u)
#define GPGDAT REG32(0x07a00064u)

#define MATRIX_ROWS_MASK 0x7fu
#define MATRIX_CONFIG_BASE 0x0000aaa9u

static void settle(void)
{
    for (volatile unsigned n = 100u; n != 0u; --n)
        __asm__ volatile("nop");
}

void hpclang_keyboard_read(struct hpclang_keyboard_state *state)
{
    uint32_t saved_gpgcon = GPGCON;
    uint64_t bits = 0u;

    for (unsigned column = 0u; column < HPCLANG_KEYBOARD_COLUMNS; ++column) {
        /*
         * GPG[8..15] are the eight matrix columns. Select one column as an
         * output while the row pins keep the stock keyboard input setup.
         */
        GPGCON =
            MATRIX_CONFIG_BASE |
            (UINT32_C(1) << ((column + 8u) * 2u));
        settle();

        uint32_t rows = ((~GPGDAT) >> 1u) & MATRIX_ROWS_MASK;
        bits |= (uint64_t)rows << (column * HPCLANG_KEYBOARD_ROWS);
    }

    GPGCON = saved_gpgcon;
    settle();

    /*
     * ON/Alpha/left Shift/right Shift are dedicated GPF inputs on this
     * calculator generation and read high while pressed.
     */
    uint32_t gpf = GPFDAT;
    if ((gpf & (1u << 0)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_ON;
    if ((gpf & (1u << 4)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_ALPHA;
    if ((gpf & (1u << 5)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_LEFT_SHIFT;
    if ((gpf & (1u << 6)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_RIGHT_SHIFT;

    state->bits = bits;
}

int hpclang_keyboard_any_is_down(
    const struct hpclang_keyboard_state *state)
{
    return state->bits != 0u;
}

int hpclang_keyboard_matrix_is_down(
    const struct hpclang_keyboard_state *state,
    unsigned column,
    unsigned row)
{
    if (column >= HPCLANG_KEYBOARD_COLUMNS ||
        row >= HPCLANG_KEYBOARD_ROWS)
        return 0;

    unsigned bit = column * HPCLANG_KEYBOARD_ROWS + row;
    return (int)((state->bits >> bit) & UINT64_C(1));
}

int hpclang_keyboard_dedicated_is_down(
    const struct hpclang_keyboard_state *state,
    enum hpclang_keyboard_dedicated_key key)
{
    if ((unsigned)key < HPCLANG_KEYBOARD_ON ||
        (unsigned)key > HPCLANG_KEYBOARD_RIGHT_SHIFT)
        return 0;

    return (int)((state->bits >> (unsigned)key) & UINT64_C(1));
}
