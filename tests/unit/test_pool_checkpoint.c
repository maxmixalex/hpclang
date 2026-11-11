#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stage-layout.h"

typedef struct
{
    void *cpu;
} hdw_t;
static uint32_t model = 6, count = 3, table = 0x08020000, state = 0x08001000;
static uint32_t pages[5] = {0x08020ffe, 0x08021ffe, 0x08022ffe, 0x08023ffe, 0x08024ffe};
static int unreadable;
static uint32_t block = 0x08030000, footer = 4096;
static int cpu_memory_rw_debug(void *cpu, uint32_t address, uint8_t *bytes, unsigned size,
                               int write)
{
    (void)cpu;
    assert(!write && size == 4);
    if (unreadable)
        return -1;
    uint32_t value;
    switch (address) {
    case 0x01100000:
        value = model;
        break;
    case 0x01100004:
        value = 36864;
        break;
    case 0x01100008:
        value = state;
        break;
    case 0x01101000:
        value = 2;
        break;
    case 0x08001914:
        value = count;
        break;
    case 0x08001918:
        value = block;
        break;
    case 0x0802fff0:
    case 0x08031000:
        value = footer;
        break;
    case 0x0802fff4:
        value = 0x2102;
        break;
    case 0x0802fff8:
    case 0x0802fffc:
        value = 0;
        break;
    case 0x08001920:
        value = table;
        break;
    case HPCLANG_STAGE_L1_SLOT:
        value = 0x08020011;
        break;
    default:
        if (address < HPCLANG_STAGE_WINDOW_BASE || address >= HPCLANG_STAGE_WINDOW_BASE + 20)
            return -1;
        value = pages[(address - HPCLANG_STAGE_WINDOW_BASE) / 4];
    }
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8));
    return 0;
}
#include "pool-checkpoint.h"

int main(void)
{
    hdw_t hdw = {0};
    char directory[] = "/tmp/hpclang-pool-XXXXXX", prefix[256], path[280], output[1024];
    assert(mkdtemp(directory));
    snprintf(prefix, sizeof prefix, "%s/frame", directory);
    snprintf(path, sizeof path, "%s-pool-2.json", prefix);
    unsetenv("HPCLANG_E2E_CAPTURE_STAGE_POOL");
    unsetenv("HPCLANG_E2E_CAPTURE_NATIVE_ALLOCATION");
    assert(pool_checkpoint(&hdw, 0x4d101000) == 0);
    setenv("HPCLANG_E2E_CAPTURE_STAGE_POOL", "1", 1);
    setenv("HPCLANG_E2E_FRAME_PREFIX", prefix, 1);
    assert(pool_checkpoint(&hdw, 0x50415353) == 0);
    assert(pool_checkpoint(&hdw, 0x4d101000) == 1);
    FILE *file = fopen(path, "r");
    assert(file && fgets(output, sizeof output, file));
    fclose(file);
    assert(strstr(output, "\"stage_pool_pages\":3,\"native_workspace_pages\":0"));
    assert(strstr(output, "\"physical_pages\":[134348800,134352896,134356992]"));
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1); /* immutable observation */
    assert(!unlink(path));
    char allocation_path[280];
    snprintf(allocation_path, sizeof allocation_path, "%s-allocation-2.json", prefix);
    setenv("HPCLANG_E2E_CAPTURE_NATIVE_ALLOCATION", "1", 1);
    assert(pool_checkpoint(&hdw, 0x4d101000) == 1);
    file = fopen(allocation_path, "r");
    assert(file && fgets(output, sizeof output, file));
    fclose(file);
    assert(strstr(output, "\"usable_bytes\":4096"));
    assert(strstr(output, "\"metadata_bytes\":20,\"mapped_pages\":0"));
    assert(!unlink(path));
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1); /* immutable allocation report */
    assert(!unlink(allocation_path));
    block = 0;
    assert(pool_checkpoint(&hdw, 0x4d101000) == 1);
    file = fopen(allocation_path, "r");
    assert(file && fgets(output, sizeof output, file));
    fclose(file);
    assert(strstr(output, "\"pointer\":0,\"usable_bytes\":0"));
    assert(!unlink(path) && !unlink(allocation_path));
    block = 0x08030000;
    unsetenv("HPCLANG_E2E_CAPTURE_NATIVE_ALLOCATION");
    model = 1;
    assert(pool_checkpoint(&hdw, 0x4d101000) == 1);
    assert(!unlink(path));
    count = 255;
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1);
    count = 3;
    pages[1] = pages[0];
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1);
    pages[1] = 0x08021fff; /* wrong descriptor type */
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1);
    pages[1] = 0x08021ffe;
    table += 1024; /* native call repurposed the table register */
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1);
    table -= 1024;
    state = 0;
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1);
    state = 0xfffffff0;
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1);
    state = 0x08001000;
    unreadable = 1;
    assert(pool_checkpoint(&hdw, 0x4d101000) == -1);
    unreadable = 0;
    assert(pool_checkpoint(&hdw, 0x4d101001) == -1);
    assert(pool_checkpoint(&hdw, 0x4d1ffffc) == -1);
    assert(access(path, F_OK) != 0);
    assert(!rmdir(directory));
    return 0;
}
