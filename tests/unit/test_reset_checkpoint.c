#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    uint32_t regs[16];
    int thumb;
} CPUARMState;
static unsigned reads;
static int fail_read;
static uint32_t instruction = 0xef000000;
static uint32_t cpsr_read(CPUARMState *env)
{
    (void)env;
    return 0x13;
}
static int cpu_memory_rw_debug(CPUARMState *env, uint32_t address, uint8_t *data, size_t count,
                               int write)
{
    assert(!write);
    ++reads;
    if (fail_read)
        return -1;
    if (count == 4) {
        assert(address == env->regs[15] - 4);
        for (unsigned i = 0; i < 4; ++i)
            data[i] = (uint8_t)(instruction >> (8 * i));
    } else {
        assert(address == 0x08000000 && count == 0x80000);
        for (size_t i = 0; i < count; ++i)
            data[i] = (uint8_t)i;
    }
    return 0;
}
#include "reset-checkpoint.h"

int main(void)
{
    CPUARMState cpu = {.regs = {[0] = 1, [15] = 0x08001000}};
    unsetenv("HPCLANG_X50NG_RESET_CHECKPOINT");
    assert(hpclang_reset_checkpoint(&cpu) == 0 && !reads);
    char directory[] = "/tmp/hpclang-reset-XXXXXX";
    assert(mkdtemp(directory));
    char prefix[128], ram_path[160], metadata_path[160];
    snprintf(prefix, sizeof prefix, "%s/snapshot", directory);
    snprintf(ram_path, sizeof ram_path, "%s.sram", prefix);
    snprintf(metadata_path, sizeof metadata_path, "%s.json", prefix);
    assert(!setenv("HPCLANG_X50NG_RESET_CHECKPOINT", prefix, 1));
    cpu.thumb = 1;
    assert(hpclang_reset_checkpoint(&cpu) == 0 && !reads);
    cpu.thumb = 0;
    cpu.regs[0] = 0;
    assert(hpclang_reset_checkpoint(&cpu) == 0 && !reads);
    cpu.regs[0] = 1;
    cpu.regs[15] = 0x1000;
    assert(hpclang_reset_checkpoint(&cpu) == 0 && !reads);
    cpu.regs[15] = 0x08001000;
    instruction |= 1;
    assert(hpclang_reset_checkpoint(&cpu) == 0 && reads == 1);
    instruction &= ~1u;
    fail_read = 1;
    assert(hpclang_reset_checkpoint(&cpu) == -1);
    assert(access(ram_path, F_OK) && access(metadata_path, F_OK));
    fail_read = 0;
    CPUARMState before = cpu;
    assert(hpclang_reset_checkpoint(&cpu) == 1);
    assert(!memcmp(&cpu, &before, sizeof cpu));
    assert(hpclang_reset_checkpoint(&cpu) == -1); /* Never replace the first witness. */
    FILE *file = fopen(ram_path, "rb");
    assert(file);
    for (unsigned i = 0; i < 0x80000; ++i)
        assert(fgetc(file) == (int)(i & 255u));
    assert(fgetc(file) == EOF && !ferror(file));
    assert(!fclose(file));
    file = fopen(metadata_path, "rb");
    char metadata[160] = {0};
    assert(file && fread(metadata, 1, sizeof metadata - 1, file) > 0);
    assert(strstr(metadata, "\"pc\":134221824") && strstr(metadata, "\"bytes\":524288"));
    assert(strstr(metadata, "\"cpsr\":19") && strstr(metadata, "\"r0\":1"));
    assert(!fclose(file));
    assert(!remove(ram_path) && !remove(metadata_path) && !rmdir(directory));
    return 0;
}
