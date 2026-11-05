#ifndef HPCLANG_EXECUTION_SNAPSHOT_H
#define HPCLANG_EXECUTION_SNAPSHOT_H

#include "resource-snapshot.h"

static int execution_snapshot_read(void *context, uint32_t address, void *bytes, size_t count)
{
    return cpu_memory_rw_debug(context, address, bytes, count, 0);
}

/* Test-only, one-shot SRAM observation before the selected instruction.
 * The translated PC is authoritative; env->regs[15] may still name the TB start.
 * No frontend clock is available here, so time_us is explicitly zero. */
static void execution_snapshot(CPUState *env, uint32_t pc)
{
    static int captured;
    if (captured)
        return;
    const char *prefix = getenv("HPCLANG_X50NG_EXECUTION_SNAPSHOT");
    const char *target = getenv("TARGET");
    static const char *const models[] = {"hp39gplus", "hp39gs",    "hp40gs",
                                         "hp48gii",   "hp49gplus", "hp50g"};
    unsigned model = 0;
    for (unsigned i = 0; target && i < 6; ++i)
        if (!strcmp(target, models[i]))
            model = i + 1;
    if (!prefix || !*prefix ||
        resource_snapshot(prefix, "entry", model, 0, pc, cpsr_read(env), execution_snapshot_read,
                          env) != 1) {
        fputs("execution snapshot failed\n", stderr);
        exit(1);
    }
    captured = 1;
}
#endif
