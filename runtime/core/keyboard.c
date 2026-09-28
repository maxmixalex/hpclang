#include <stdint.h>

#include <hpclang/keyboard.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define GPFDAT REG32(0x07a00054u)
#define GPGCON REG32(0x07a00060u)
#define GPGDAT REG32(0x07a00064u)

#define MATRIX_ROWS_MASK 0x7fu
#define GPG_SCAN_LOW_CONFIG UINT32_C(0x0000aaa9)

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
         * output while the other columns remain inputs. The row-side GPIO
         * configuration is established explicitly instead of inheriting a
         * transient stock-OS GPG0..7 state; the complete original GPGCON is
         * restored after the scan.
         */
        GPGCON = GPG_SCAN_LOW_CONFIG | (UINT32_C(1) << ((column + 8u) * 2u));
        settle();

        uint32_t rows = ((~GPGDAT) >> 1u) & MATRIX_ROWS_MASK;
        bits |= (uint64_t)rows << (column * HPCLANG_KEYBOARD_ROWS);
    }

    GPGCON = saved_gpgcon;
    settle();

    /*
     * Four keys bypass the GPG matrix. GPF0 is ON on all validated models.
     * GPF4/5/6 are preserved as raw physical inputs because their labels
     * differ: HP39 uses comma/Alpha/Shift, while HP48/49/50 use
     * Alpha/left-Shift/right-Shift.
     */
    uint32_t gpf = GPFDAT;
    if ((gpf & (1u << 0)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_ON;
    if ((gpf & (1u << 4)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_GPF4;
    if ((gpf & (1u << 5)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_GPF5;
    if ((gpf & (1u << 6)) != 0u)
        bits |= UINT64_C(1) << HPCLANG_KEYBOARD_GPF6;

    state->bits = bits;
}

int hpclang_keyboard_any_is_down(const struct hpclang_keyboard_state *state)
{
    return state->bits != 0u;
}

int hpclang_keyboard_matrix_is_down(const struct hpclang_keyboard_state *state, unsigned column,
                                    unsigned row)
{
    if (column >= HPCLANG_KEYBOARD_COLUMNS || row >= HPCLANG_KEYBOARD_ROWS)
        return 0;

    unsigned bit = column * HPCLANG_KEYBOARD_ROWS + row;
    return (int)((state->bits >> bit) & UINT64_C(1));
}

int hpclang_keyboard_dedicated_is_down(const struct hpclang_keyboard_state *state,
                                       enum hpclang_keyboard_dedicated_key key)
{
    if ((unsigned)key < HPCLANG_KEYBOARD_ON || (unsigned)key > HPCLANG_KEYBOARD_GPF6)
        return 0;

    return (int)((state->bits >> (unsigned)key) & UINT64_C(1));
}
