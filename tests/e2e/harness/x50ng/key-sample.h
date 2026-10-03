#ifndef HPCLANG_KEY_SAMPLE_H
#define HPCLANG_KEY_SAMPLE_H
#include <stdint.h>

/* A driven scan line and an input line must exist before pressing a matrix key.
 * During cold boot Stock temporarily configures every line as an output. */
static inline int hpclang_key_ready(int port, uint32_t config, uint32_t value, unsigned column,
                                    unsigned row, int eint)
{
    if (port == 0 && eint >= 0)
        return ((config >> (eint * 2)) & 3) != 1;
    if (port != 1 || eint >= 0)
        return 0;
    unsigned col = column + 8;
    return ((((config >> (col * 2)) & 3) == 1 && !(value & (1u << col)) &&
             ((config >> (row * 2)) & 3) != 1) ||
            (((config >> (row * 2)) & 3) == 1 && !(value & (1u << row)) &&
             ((config >> (col * 2)) & 3) != 1));
}

/* GPIO sampling proof only: this does not claim that Stock accepted an action. */
static inline int hpclang_key_sampled(int port, uint32_t config, uint32_t value, unsigned column,
                                      unsigned row, int eint, int pressed)
{
    if (port == 0 && eint >= 0)
        return ((config >> (eint * 2)) & 3) != 1 && !!(value & (1u << eint)) == !!pressed;
    if (port != 1 || eint >= 0)
        return 0;
    unsigned col = column + 8;
    if (((config >> (col * 2)) & 3) == 1 && !(value & (1u << col)) &&
        ((config >> (row * 2)) & 3) != 1)
        return !(value & (1u << row)) == !!pressed;
    if (((config >> (row * 2)) & 3) == 1 && !(value & (1u << row)) &&
        ((config >> (col * 2)) & 3) != 1)
        return !(value & (1u << col)) == !!pressed;
    return 0;
}
#endif
