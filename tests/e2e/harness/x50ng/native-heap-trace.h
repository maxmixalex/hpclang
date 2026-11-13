#ifndef HPCLANG_NATIVE_HEAP_TRACE_H
#define HPCLANG_NATIVE_HEAP_TRACE_H

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "native-heap-observer.h"

void hpclang_native_heap_phase(const char *phase, uint64_t time_us);

/* Host-only, fixed-size phase summaries. No guest memory writes and no
 * per-allocation files. The harness must supply a ROM-validated profile. */
struct native_heap_phase
{
    char name[64];
    uint64_t start_us, end_us, failed_allocations;
    struct native_heap_observation observation;
};

static struct native_heap_phase native_heap_phases[64];
static unsigned native_heap_phase_count, native_heap_model;
static uint32_t native_heap_roots, native_heap_end, native_heap_pc;
static uint64_t native_heap_ignored;
static FILE *native_heap_file;
static int native_heap_initialized;

static void native_heap_trace_fail(void)
{
    fputs("native heap trace configuration/output failure\n", stderr);
    exit(1);
}

static uint32_t native_heap_setting(const char *name)
{
    const char *text = getenv(name);
    char *end;
    if (!text || !*text || *text == '-')
        native_heap_trace_fail();
    errno = 0;
    unsigned long value = strtoul(text, &end, 0);
    if (errno || *end || value > UINT32_MAX)
        native_heap_trace_fail();
    return (uint32_t)value;
}

static int native_heap_write_capacity(const struct native_heap_capacity value[2])
{
    return fprintf(native_heap_file,
                   "[{\"usable\":%u,\"largest\":%u,\"metadata\":%u,\"blocks\":%u},"
                   "{\"usable\":%u,\"largest\":%u,\"metadata\":%u,\"blocks\":%u}]",
                   value[0].usable_bytes, value[0].largest_usable_bytes, value[0].metadata_bytes,
                   value[0].blocks, value[1].usable_bytes, value[1].largest_usable_bytes,
                   value[1].metadata_bytes, value[1].blocks) < 0;
}

static void native_heap_trace_finish(void)
{
    if (!native_heap_file)
        return;
    int failed = fprintf(native_heap_file,
                         "{\"version\":1,\"model\":%u,\"return_pc\":%u,\"roots\":%u,"
                         "\"end\":%u,\"ignored_before_first_phase\":%" PRIu64 ",\"phases\":[",
                         native_heap_model, native_heap_pc, native_heap_roots, native_heap_end,
                         native_heap_ignored) < 0;
    for (unsigned i = 0; i < native_heap_phase_count; ++i) {
        const struct native_heap_phase *phase = &native_heap_phases[i];
        const struct native_heap_observation *s = &phase->observation;
        failed |= fprintf(native_heap_file,
                          "%s{\"phase\":\"%s\",\"start_us\":%" PRIu64 ",\"end_us\":", i ? "," : "",
                          phase->name, phase->start_us) < 0;
        failed |=
            (i + 1u < native_heap_phase_count ? fprintf(native_heap_file, "%" PRIu64, phase->end_us)
                                              : fprintf(native_heap_file, "null")) < 0;
        failed |=
            fprintf(native_heap_file,
                    ",\"accepted\":%" PRIu64 ",\"rejected\":%" PRIu64
                    ",\"failed_allocations\":%" PRIu64 ",\"minimum_sample\":%" PRIu64
                    ",\"minimum_total_usable\":",
                    s->accepted, s->rejected, phase->failed_allocations, s->minimum_sample) < 0;
        failed |= (s->accepted ? fprintf(native_heap_file, "%u", s->minimum_total_usable)
                               : fprintf(native_heap_file, "null")) < 0;
        failed |= fprintf(native_heap_file, ",\"initial\":") < 0;
        failed |= native_heap_write_capacity(s->initial);
        failed |= fprintf(native_heap_file, ",\"minimum\":") < 0;
        failed |= native_heap_write_capacity(s->at_minimum);
        failed |= fprintf(native_heap_file, ",\"latest\":") < 0;
        failed |= native_heap_write_capacity(s->latest);
        failed |= fprintf(native_heap_file, "}") < 0;
    }
    failed |= fprintf(native_heap_file, "]}\n") < 0;
    failed |= fclose(native_heap_file) != 0;
    native_heap_file = NULL;
    if (failed) {
        fputs("native heap trace write failed\n", stderr);
        _Exit(1);
    }
}

static void native_heap_trace_init(void)
{
    if (native_heap_initialized)
        return;
    native_heap_initialized = 1;
    const char *path = getenv("HPCLANG_X50NG_NATIVE_HEAP");
    if (!path || !*path)
        return;
    native_heap_model = native_heap_setting("HPCLANG_X50NG_NATIVE_HEAP_MODEL");
    native_heap_pc = native_heap_setting("HPCLANG_X50NG_NATIVE_HEAP_PC");
    native_heap_roots = native_heap_setting("HPCLANG_X50NG_NATIVE_HEAP_ROOTS");
    native_heap_end = native_heap_model == 4 ? 0x08040000u : 0x08080000u;
    if (native_heap_model < 1 || native_heap_model > 6 || (native_heap_pc & 3u) ||
        native_heap_pc >= 0x00200000u || (native_heap_roots & 3u) ||
        native_heap_roots < 0x08000000u || native_heap_roots > native_heap_end - 32u)
        native_heap_trace_fail();
    native_heap_file = fopen(path, "wx");
    if (!native_heap_file || atexit(native_heap_trace_finish))
        native_heap_trace_fail();
}

static void native_heap_trace_mark(const char *name, uint64_t time_us)
{
    native_heap_trace_init();
    if (!native_heap_file)
        return;
    if (!name || !*name || strlen(name) > 63 || native_heap_phase_count == 64)
        native_heap_trace_fail();
    for (const char *p = name; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '-' || *p == '_'))
            native_heap_trace_fail();
    if (native_heap_phase_count) {
        struct native_heap_phase *previous = &native_heap_phases[native_heap_phase_count - 1u];
        if (time_us < previous->start_us)
            native_heap_trace_fail();
        previous->end_us = time_us;
    }
    struct native_heap_phase *phase = &native_heap_phases[native_heap_phase_count++];
    strcpy(phase->name, name);
    phase->start_us = time_us;
}

static void native_heap_trace_observe(native_heap_read32_t read32, void *context, uint32_t pc,
                                      uint32_t result)
{
    native_heap_trace_init();
    if (!native_heap_file)
        return;
    if (pc != native_heap_pc)
        native_heap_trace_fail();
    if (!native_heap_phase_count) {
        ++native_heap_ignored;
        return;
    }
    struct native_heap_phase *phase = &native_heap_phases[native_heap_phase_count - 1u];
    if (!result)
        ++phase->failed_allocations;
    (void)native_heap_observe(&phase->observation, read32, context, 0x08000000u, native_heap_end,
                              native_heap_roots);
}

#endif
