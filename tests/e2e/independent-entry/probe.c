typedef unsigned int u32;
struct workspace
{
    u32 marker, checksum, local_address, sp, base, end;
    u32 values[128];
};
__attribute__((noinline)) void exercise(struct workspace *out, volatile u32 *control)
{
    u32 cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    control[1] = cpsr;
    volatile u32 local[32];
    u32 sum = 0x12345678u, stack;
    __asm__ volatile("mov %0, sp" : "=r"(stack));
    for (u32 i = 0; i < 32; ++i)
        local[i] = i * 0x10203u + 7u;
    for (u32 i = 0; i < 128; ++i) {
        u32 value = local[i % 32] ^ (i * 0x01010101u);
        out->values[i] = value;
        sum = (sum << 3 | sum >> 29) ^ value;
    }
    out->checksum = sum;
    out->local_address = (u32)local;
    out->sp = stack;
    out->base = (u32)out;
    out->end = (u32)(out + 1);
    out->marker = 0x48504343u;
}
