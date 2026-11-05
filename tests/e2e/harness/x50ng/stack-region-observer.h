#ifndef HPCLANG_STACK_REGION_OBSERVER_H
#define HPCLANG_STACK_REGION_OBSERVER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Host-only observations of translated SP assignments at any PC. Keep distinct
 * CPU banks and 1 MiB address regions separate: a switch between stacks is not
 * an allocation of all memory between their addresses. This is executed-path
 * evidence, not a bound on unobserved writes or implicit bank switches. */
struct stack_region_observation
{
    uint64_t writes;
    uint32_t minimum, maximum, minimum_pc, maximum_pc;
};

static struct stack_region_observation *stack_regions;
static FILE *stack_region_file;
static int stack_region_initialized;

static void stack_region_finish(void)
{
    if (!stack_region_file)
        return;
    int failed = 0;
    for (unsigned mode = 0; mode < 32; ++mode) {
        for (unsigned region = 0; region < 4096; ++region) {
            const struct stack_region_observation *s = &stack_regions[mode * 4096 + region];
            if (s->writes)
                failed |= fprintf(stack_region_file,
                                  "{\"version\":1,\"mode\":%u,\"region\":%u,\"writes\":%llu,"
                                  "\"minimum\":%u,\"maximum\":%u,\"minimum_pc\":%u,"
                                  "\"maximum_pc\":%u}\n",
                                  mode, region, (unsigned long long)s->writes, s->minimum,
                                  s->maximum, s->minimum_pc, s->maximum_pc) < 0;
        }
    }
    failed |= fclose(stack_region_file) != 0;
    stack_region_file = NULL;
    free(stack_regions);
    stack_regions = NULL;
    if (failed) {
        fputs("stack region trace write failed\n", stderr);
        _Exit(1);
    }
}

static void stack_region_observe(uint32_t sp, uint32_t pc, uint32_t mode)
{
    if (!stack_region_initialized) {
        stack_region_initialized = 1;
        const char *path = getenv("HPCLANG_X50NG_STACK_REGIONS");
        if (path && *path) {
            stack_region_file = fopen(path, "wx");
            stack_regions = calloc(32u * 4096u, sizeof *stack_regions);
            if (!stack_region_file || !stack_regions || atexit(stack_region_finish)) {
                fputs("stack region trace initialization failed\n", stderr);
                exit(1);
            }
        }
    }
    if (!stack_region_file)
        return;
    struct stack_region_observation *s = &stack_regions[(mode & 31u) * 4096u + (sp >> 20)];
    if (!s->writes || sp < s->minimum) {
        s->minimum = sp;
        s->minimum_pc = pc;
    }
    if (!s->writes || sp > s->maximum) {
        s->maximum = sp;
        s->maximum_pc = pc;
    }
    ++s->writes;
}
#endif
