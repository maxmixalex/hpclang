#ifndef HPCLANG_RESOURCE_SNAPSHOT_H
#define HPCLANG_RESOURCE_SNAPSHOT_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Host-only observation. Exclusive output creation preserves prior evidence.
 * A JSON sidecar is complete only if both writes and closes succeed. */
static int resource_snapshot(const char *prefix, const char *phase, unsigned model,
                             uint64_t time_us, uint32_t pc, uint32_t cpsr,
                             int (*read_memory)(void *, uint32_t, void *, size_t), void *context)
{
    if (!prefix || !*prefix)
        return 0;
    if (!phase || !*phase || strlen(phase) > 63 || model < 1 || model > 6 || !read_memory)
        return -1;
    for (const char *p = phase; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '-' || *p == '_'))
            return -1;
    char raw_path[1024], json_path[1024];
    if (snprintf(raw_path, sizeof raw_path, "%s-%s.sram", prefix, phase) >= (int)sizeof raw_path ||
        snprintf(json_path, sizeof json_path, "%s-%s.json", prefix, phase) >= (int)sizeof json_path)
        return -1;
    size_t size = model == 4 ? 0x40000u : 0x80000u;
    unsigned char *bytes = malloc(size);
    if (!bytes)
        return -1;
    if (read_memory(context, 0x08000000u, bytes, size)) {
        free(bytes);
        return -1;
    }
    FILE *raw = fopen(raw_path, "wx");
    if (!raw) {
        free(bytes);
        return -1;
    }
    FILE *json = fopen(json_path, "wx");
    if (!json) {
        fclose(raw);
        remove(raw_path);
        free(bytes);
        return -1;
    }
    int error = fwrite(bytes, 1, size, raw) != size;
    free(bytes);
    error |= fclose(raw) != 0;
    if (!error)
        error = fprintf(json,
                        "{\"version\":1,\"phase\":\"%s\",\"model\":%u,\"time_us\":%" PRIu64
                        ",\"pc\":%u,\"cpsr\":%u,\"base\":134217728,\"bytes\":%zu}\n",
                        phase, model, time_us, pc, cpsr, size) < 0;
    error |= fclose(json) != 0;
    if (error) {
        remove(raw_path);
        remove(json_path);
        return -1;
    }
    return 1;
}
#endif
