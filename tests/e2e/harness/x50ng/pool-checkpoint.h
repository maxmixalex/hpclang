#ifndef HPCLANG_POOL_CHECKPOINT_H
#define HPCLANG_POOL_CHECKPOINT_H

#include "stage-layout.h"
#include "native-allocation.h"

static int pool_read32(hdw_t *hdw, uint32_t address, uint32_t *value)
{
    uint8_t bytes[4];
    if (cpu_memory_rw_debug(hdw->cpu, address, bytes, sizeof bytes, 0))
        return -1;
    *value = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
             ((uint32_t)bytes[3] << 24);
    return 0;
}

static int pool_native_read32(void *context, uint32_t address, uint32_t *value)
{
    return pool_read32(context, address, value);
}

static int pool_native_allocation(hdw_t *hdw, const char *prefix, uint32_t id, uint32_t model,
                                  uint32_t block, const uint32_t *pages, uint32_t count)
{
    const char *enabled = getenv("HPCLANG_E2E_CAPTURE_NATIVE_ALLOCATION");
    if (!enabled || strcmp(enabled, "1"))
        return 0;
    struct native_allocation value = {0};
    uint32_t end = model == 4u ? 0x08040000u : 0x08080000u;
    if (block && native_allocation_read(pool_native_read32, hdw, block, 0x08000000u, end, pages,
                                        count, &value))
        return -1;
    char path[1024];
    if (snprintf(path, sizeof path, "%s-allocation-%u.json", prefix, id) >= (int)sizeof path)
        return -1;
    FILE *file = fopen(path, "wx");
    if (!file)
        return -1;
    fprintf(file,
            "{\"version\":1,\"id\":%u,\"model\":%u,\"pointer\":%u,\"usable_bytes\":%u,"
            "\"flags\":%u,\"next\":%u,\"previous\":%u,\"metadata_bytes\":%u,"
            "\"mapped_pages\":%u,\"unmapped_usable_bytes\":%u}\n",
            id, model, value.pointer, value.usable_bytes, value.flags, value.next, value.previous,
            value.metadata_bytes, value.mapped_pages, value.unmapped_usable_bytes);
    int error = ferror(file);
    return fclose(file) || error ? -1 : 0;
}

/* Stock-only, opt-in observation before acknowledging an ordinary memory
 * checkpoint. Read the existing boot/native state and original coarse table;
 * do not inject guest storage or infer a pool count from trailing stale entries.
 * A native call may repurpose ARMSYS registers. Reject inconsistent state
 * rather than interpreting it as an allocation measurement. */
static int pool_checkpoint(hdw_t *hdw, uint32_t request)
{
    const char *enabled = getenv("HPCLANG_E2E_CAPTURE_STAGE_POOL");
    if (!enabled || strcmp(enabled, "1") || (request & 0xfff00000u) != 0x4d100000u)
        return 0;
    uint32_t offset = request & 0xfffffu;
    uint32_t model, rw, state, count, table, slot, id, block;
    if ((offset & 3u) || offset < 4096u || pool_read32(hdw, 0x01100000u, &model) || model < 1u ||
        model > 6u || pool_read32(hdw, 0x01100004u, &rw) || rw < 4096u || rw > 0x100000u ||
        (rw & 4095u) || offset > rw - 8u || pool_read32(hdw, 0x01100008u, &state) || !state ||
        state > UINT32_MAX - 0x924u || pool_read32(hdw, state + 0x914u, &count) ||
        pool_read32(hdw, state + 0x918u, &block) || pool_read32(hdw, state + 0x920u, &table) ||
        pool_read32(hdw, HPCLANG_STAGE_L1_SLOT, &slot) || (slot & 3u) != 1u || (table & 1023u) ||
        (slot & 0xfffffc00u) != table || pool_read32(hdw, 0x01100000u + offset, &id) || id > 65535u)
        return -1;
    uint32_t workspace = model <= 3u ? 2u : 0u;
    if (!count || count > 256u - workspace)
        return -1;
    uint32_t pages[256];
    for (uint32_t i = 0; i < count + workspace; ++i) {
        uint32_t descriptor;
        if (pool_read32(hdw, HPCLANG_STAGE_WINDOW_BASE + i * 4u, &descriptor) ||
            (descriptor & 3u) != 2u)
            return -1;
        pages[i] = descriptor & 0xfffff000u;
        for (uint32_t j = 0; j < i; ++j)
            if (pages[j] == pages[i])
                return -1;
    }
    const char *prefix = getenv("HPCLANG_E2E_FRAME_PREFIX");
    char path[1024];
    if (!prefix || snprintf(path, sizeof path, "%s-pool-%u.json", prefix, id) >= (int)sizeof path)
        return -1;
    if (pool_native_allocation(hdw, prefix, id, model, block, pages, count + workspace))
        return -1;
    FILE *file = fopen(path, "wx");
    if (!file)
        return -1;
    fprintf(file,
            "{\"version\":1,\"id\":%u,\"model\":%u,\"rw_bytes\":%u,"
            "\"stage_pool_pages\":%u,\"native_workspace_pages\":%u,"
            "\"native_os_block\":%u,\"pool_l2_physical\":%u,\"physical_pages\":[",
            id, model, rw, count, workspace, block, table);
    for (uint32_t i = 0; i < count + workspace; ++i)
        fprintf(file, "%s%u", i ? "," : "", pages[i]);
    fputs("]}\n", file);
    int error = ferror(file);
    return fclose(file) || error ? -1 : 1;
}
#endif
