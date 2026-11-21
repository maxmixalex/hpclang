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
    if (!rec) {
        /* The private result on the RPL stack already indicates admission refusal. */
        control[1] = hpclang_irq_save_disable() | 0xc0;
        return;
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
    u32 status = rec[40];
    out[3] = stock_call(201, 0x100, 1048576);
    if (out[3])
        for (;;) {
        }
    rec[0] = 0; /* Retire the live-record marker before returning its block to KOS. */
    for (unsigned i = count; i; i--)
        out[10 + 4 * (i - 1)] = stock_call(101, out[8 + 4 * (i - 1)], 0);
    for (unsigned i = 0; i < count; i++)
        out[11 + 4 * i] = stock_call(201, 0x100, 4095 + 4096 * out[9 + 4 * i]);
    for (unsigned i = count; i; i--)
        if (!out[11 + 4 * (i - 1)] || stock_call(101, out[11 + 4 * (i - 1)], 0))
            for (;;) {
            }
    for (unsigned i = 0; i < count; i++)
        if (out[10 + 4 * i])
            for (;;) {
            }
    /* Check the application's exit status after restoring and releasing leases. */
    if (status != NOTEBOOK_EXPECTED_STATUS)
        for (;;) {
        }
    /* Publish completion in the fresh exact integer created by NEWOB. */
    u32 stack, obj, type, length, digits;
    if (!rd((void *)map, 0x806f8, 5, &stack) || !rd((void *)map, stack, 5, &obj) || obj < 0x80000 ||
        obj > 0xfffef || !rd((void *)map, obj, 5, &type) || type != 0x2614 ||
        !rd((void *)map, obj + 5, 5, &length) || length != 12 ||
        !rd((void *)map, obj + 10, 5, &digits) || digits != 0x24277)
        for (;;) {
        }
    u32 addr = obj + 10, page = map[addr >> 12];
    if (page != map[257 + (addr >> 12)] || page < 0x08040000 || page > 0x0807f800)
        for (;;) {
        }
    volatile unsigned char *byte = (void *)(page + ((addr & 4095) >> 1));
    unsigned shift = 4 * (addr & 1);
    *byte = (*byte & ~(15u << shift)) | ((5u + status) << shift);
    out[4] = 0xd04ec105;
    control[1] = hpclang_irq_save_disable() | 0xc0;
}
