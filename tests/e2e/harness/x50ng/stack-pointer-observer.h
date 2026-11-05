#ifndef HPCLANG_STACK_POINTER_OBSERVER_H
#define HPCLANG_STACK_POINTER_OBSERVER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Host-only trace of translated application SP writes. Unlike a fill-pattern
 * watermark this sees reserved but untouched frames. It is not a bound on
 * unexecuted paths, native firmware helpers or banked exception stacks. */
struct stack_pointer_observation
{
    uint64_t writes;
    uint32_t minimum, maximum, minimum_pc;
};

static struct stack_pointer_observation stack_pointer_modes[32];
static FILE *stack_pointer_file;
static int stack_pointer_initialized;

static void stack_pointer_finish(void)
{
    if (!stack_pointer_file)
        return;
    int failed = 0;
    for (unsigned mode = 0; mode < 32; ++mode) {
        const struct stack_pointer_observation *s = &stack_pointer_modes[mode];
        if (s->writes)
            failed |= fprintf(stack_pointer_file,
                              "{\"version\":1,\"mode\":%u,\"writes\":%llu,\"minimum\":%u,"
                              "\"maximum\":%u,\"minimum_pc\":%u}\n",
                              mode, (unsigned long long)s->writes, s->minimum, s->maximum,
                              s->minimum_pc) < 0;
    }
    failed |= fclose(stack_pointer_file) != 0;
    stack_pointer_file = NULL;
    if (failed) {
        fputs("stack pointer trace write failed\n", stderr);
        _Exit(1);
    }
}

static void stack_pointer_observe(uint32_t sp, uint32_t pc, uint32_t mode)
{
    if (!stack_pointer_initialized) {
        stack_pointer_initialized = 1;
        const char *path = getenv("HPCLANG_X50NG_STACK_POINTER");
        if (path && *path) {
            stack_pointer_file = fopen(path, "wx");
            if (!stack_pointer_file || atexit(stack_pointer_finish)) {
                fputs("stack pointer trace initialization failed\n", stderr);
                exit(1);
            }
        }
    }
    if (!stack_pointer_file || pc < 0x01200000u || pc >= 0x01300000u)
        return;
    struct stack_pointer_observation *s = &stack_pointer_modes[mode & 31u];
    if (!s->writes || sp < s->minimum) {
        s->minimum = sp;
        s->minimum_pc = pc;
    }
    if (!s->writes || sp > s->maximum)
        s->maximum = sp;
    ++s->writes;
}
#endif
