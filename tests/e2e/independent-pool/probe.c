/* Stock-only allocation experiment. No HPC1 mapping is installed here. */
typedef unsigned int u32;
u32 hpclang_irq_save_disable(void);
void hpclang_irq_restore(u32);
u32 stock_call(u32, u32, u32);

__asm__(".arm\n"
        ".global stock_call\n"
        "stock_call:\n"
        "stmdb sp!, {r4-r12,lr}\n"
        "adr lr, 1f\n"
        "swi 0\n"
        "1: ldmia sp!, {r4-r12,lr}\n"
        "bx lr\n");

void exercise(u32 *out, volatile u32 *control)
{
    for (u32 i = 0; i < 134; ++i)
        out[i] = 0;
    out[0] = 0x504f4f4cu;
    out[1] = 1;
    /* Allocation/release run in the original Stock mappings and IRQ state.
       Only the existing entry's final return expects masked IRQ/FIQ again. */
    hpclang_irq_restore(control[0]);
    for (u32 trial = 0; trial < 5; ++trial) {
        u32 bytes = trial < 3 ? (32768u << trial) + 4095u : trial == 3 ? 1048576u : 32768u + 4095u;
        u32 *record = out + 4 + trial * 8;
        record[0] = bytes;
        u32 allocation = stock_call(201, 0x100, bytes);
        record[1] = allocation;
        if (!allocation)
            continue;
        /* Unexpected return values are recorded, never used as RAM pointers. */
        if (allocation < 0x08000000u || allocation >= 0x0a000000u ||
            bytes > 0x0a000000u - allocation) {
            out[1] = 2;
            break;
        }
        u32 aligned = (allocation + 4095u) & ~4095u;
        u32 pages = (allocation + bytes - aligned) >> 12;
        record[2] = aligned;
        record[3] = pages;
        record[4] = ((volatile u32 *)0x07f00000u)[aligned >> 20];
        for (u32 i = 0; i < pages; ++i) {
            volatile u32 *page = (void *)(aligned + i * 4096u);
            page[0] = 0x13570000u + i;
            page[1023] = 0x24680000u + i;
        }
        for (u32 i = 0; i < pages; ++i) {
            volatile u32 *page = (void *)(aligned + i * 4096u);
            if (page[0] != 0x13570000u + i || page[1023] != 0x24680000u + i)
                record[6]++;
        }
        record[5] = stock_call(101, allocation, 0);
        __asm__ volatile("mrs %0, cpsr" : "=r"(record[7]));
    }
    if (out[1] == 1)
        out[1] = 0;
    control[1] = hpclang_irq_save_disable() | 0xc0u;
}
