#include "loader.h"
void exercise(u32 *out, volatile u32 *control, const u32 *map, u32 *frame)
{
    hpclang_irq_restore(control[0]);
    unsigned count = 0, total = 0;
    for (unsigned n = 8; n; n /= 2)
        while (count < 12 && total + n <= 35) {
            unsigned ptr = stock_call(201, 0x100, n * 4096 + 4095);
            if (!ptr)
                break;
            if (!owner(ptr, n * 4096 + 4095))
                for (;;) {
                }
            out[8 + 2 * count] = ptr;
            out[9 + 2 * count] = n;
            u32 first = (ptr + 4095) & ~4095u;
            for (unsigned i = 0; i < n; i++) {
                u32 p = first + i * 4096;
                if ((desc(p) & 0xfff00003u) != 0x08000002u)
                    for (;;) {
                    }
                for (unsigned j = 0; j < 1024; j++)
                    ((volatile u32 *)p)[j] = mark(p, j);
            }
            total += n;
            count++;
        }
    if (!count)
        for (;;) {
        }
    volatile u32 *record = (volatile u32 *)out[8]; // leading alignment slack
    if ((((out[8] + 4095) & ~4095u) - out[8]) < 168)
        for (;;) {
        }
    record[0] = 0x4b4c4541;
    record[1] = count;
    record[2] = total;
    record[3] = desc(out[8]);
    for (unsigned i = 0; i < count * 2; i++)
        record[4 + i] = out[8 + i];
    split(record, 0, map, frame);
    control[1] = hpclang_irq_save_disable() | 0xc0;
}
