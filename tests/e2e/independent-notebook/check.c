#include "loader.h"
void exercise(u32 *out, volatile u32 *control, const u32 *map, u32 *frame)
{
    hpclang_irq_restore(control[0]);
    volatile u32 *rec = 0;
    for (u32 p = 0x08000010; p < 0x0807fff0; p += 4) {
        volatile u32 *q = (volatile u32 *)p;
        if (q[0] == 0x4b4c4541 && q[1] > 0 && q[1] <= 12 && q[4] == p &&
            owner(p, 4095 + q[5] * 4096)) {
            if (rec)
                for (;;) {
                }
            rec = q;
        }
    }
    if (!rec)
        for (;;) {
        }
    unsigned count = rec[1], total = rec[2];
    out[0] = 0x47434b4cu;
    out[1] = count;
    out[2] = total;
    for (unsigned i = 0; i < count; i++) {
        u32 ptr = rec[4 + 2 * i], n = rec[5 + 2 * i];
        out[8 + 4 * i] = ptr;
        out[9 + 4 * i] = n;
        if (n > 8 || !n || !owner(ptr, n * 4096 + 4095))
            for (;;) {
            }
        for (unsigned k = 0; k < n; k++) {
            u32 p = ((ptr + 4095) & ~4095u) + 4096 * k;
            if (desc(p) != rec[3])
                for (;;) {
                }
            for (unsigned j = 0; j < 1024; j++)
                if (((volatile u32 *)p)[j] != mark(p, j))
                    for (;;) {
                    }
        }
    }
    out[60] = split(rec, 1, map, frame);
    out[3] = stock_call(201, 0x100, 1048576);
    if (out[3])
        for (;;) {
        }
    for (unsigned i = count; i; i--)
        out[10 + 4 * (i - 1)] = stock_call(101, out[8 + 4 * (i - 1)], 0);
    for (unsigned i = 0; i < count; i++)
        out[11 + 4 * i] = stock_call(201, 0x100, 4095 + 4096 * out[9 + 4 * i]);
    for (unsigned i = count; i; i--)
        if (!out[11 + 4 * (i - 1)] || stock_call(101, out[11 + 4 * (i - 1)], 0))
            for (;;) {
            }
    out[4] = 0xd04ec105;
    control[1] = hpclang_irq_save_disable() | 0xc0;
}
