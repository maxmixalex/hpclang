#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    void *cpu;
} hdw_t;
static uint8_t ram[65536];
static int fail_read;
static int cpu_memory_rw_debug(void *cpu, uint32_t address, uint8_t *bytes, unsigned count,
                               int write)
{
    (void)cpu;
    if ((!write && fail_read) || address < 0x01100000u || address - 0x01100000u > sizeof ram ||
        count > sizeof ram - (address - 0x01100000u))
        return -1;
    if (write)
        memcpy(ram + address - 0x01100000u, bytes, count);
    else
        memcpy(bytes, ram + address - 0x01100000u, count);
    return 0;
}
#include "memory-checkpoint.h"

static void word(unsigned offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        ram[offset + i] = (uint8_t)(value >> (8 * i));
}

int main(void)
{
    char directory[] = "/tmp/hpclang-memory-XXXXXX";
    assert(mkdtemp(directory));
    char prefix[256], path[256];
    snprintf(prefix, sizeof prefix, "%s/frame", directory);
    snprintf(path, sizeof path, "%s/frame-memory-1.json", directory);
    assert(!setenv("HPCLANG_E2E_FRAME_PREFIX", prefix, 1));
    hdw_t hdw = {0};
    word(4, sizeof ram);
    word(0x1000, 1);
    word(0x1004, 2);
    word(0x1008, 0x12345678);
    word(0x100c, UINT32_MAX);
    assert(memory_checkpoint(&hdw, 0x01102000, 0x50415353) == 0);
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == 1);
    assert(ram[0x2000] == 0 && ram[0x2001] == 0x10 && ram[0x2002] == 0x20 && ram[0x2003] == 0x4d);
    FILE *file = fopen(path, "r");
    assert(file);
    char text[128] = {0};
    assert(fread(text, 1, sizeof text - 1, file) > 0);
    assert(!fclose(file));
    assert(!strcmp(text, "{\"version\":1,\"id\":1,\"words\":[305419896,4294967295]}\n"));
    word(0x2000, 0);
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == -1); /* immutable ID */
    assert(ram[0x2000] == 0 && ram[0x2003] == 0);
    assert(!unlink(path));
    for (unsigned i = 0; i < 4; ++i) {
        const uint32_t requests[] = {0x4d100000, 0x4d101001, 0x4d10fffc, 0x4d1ffffc};
        assert(memory_checkpoint(&hdw, 0x01102000, requests[i]) == -1);
    }
    word(0x1004, 65);
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == -1);
    word(0x1004, 0);
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == -1);
    word(0x1004, 2);
    word(4, 0x100c); /* Descriptor fits, payload does not. */
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == -1);
    word(4, 0x100001);
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == -1);
    word(4, sizeof ram);
    fail_read = 1;
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == -1);
    fail_read = 0;
    assert(!unsetenv("HPCLANG_E2E_FRAME_PREFIX"));
    assert(memory_checkpoint(&hdw, 0x01102000, 0x4d101000) == -1);
    assert(access(path, F_OK) != 0);
    assert(!rmdir(directory));
    return 0;
}
