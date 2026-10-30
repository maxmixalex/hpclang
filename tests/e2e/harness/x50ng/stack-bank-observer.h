#ifndef HPCLANG_STACK_BANK_OBSERVER_H
#define HPCLANG_STACK_BANK_OBSERVER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Host-only mode-switch observations. Incoming/outgoing SP ranges are kept
 * separate; their difference is not stack usage. User/System share a bank.
 * This records executed switches, not reservations or unexecuted paths. */
struct stack_bank_observation
{
    uint64_t switches;
    uint32_t outgoing_min, outgoing_max, incoming_min, incoming_max;
};

static struct stack_bank_observation stack_banks[32][32];
static FILE *stack_bank_file;
static int stack_bank_initialized;

static void stack_bank_finish(void)
{
    if (!stack_bank_file)
        return;
    int failed = 0;
    for (unsigned from = 0; from < 32; ++from) {
        for (unsigned to = 0; to < 32; ++to) {
            const struct stack_bank_observation *s = &stack_banks[from][to];
            if (s->switches)
                failed |= fprintf(stack_bank_file,
                                  "{\"version\":1,\"from_mode\":%u,\"to_mode\":%u,"
                                  "\"switches\":%llu,\"outgoing_min\":%u,\"outgoing_max\":%u,"
                                  "\"incoming_min\":%u,\"incoming_max\":%u}\n",
                                  from, to, (unsigned long long)s->switches, s->outgoing_min,
                                  s->outgoing_max, s->incoming_min, s->incoming_max) < 0;
        }
    }
    failed |= fclose(stack_bank_file) != 0;
    stack_bank_file = NULL;
    if (failed) {
        fputs("stack bank trace write failed\n", stderr);
        _Exit(1);
    }
}

static void stack_bank_observe(uint32_t from, uint32_t to, uint32_t outgoing, uint32_t incoming)
{
    if (!stack_bank_initialized) {
        stack_bank_initialized = 1;
        const char *path = getenv("HPCLANG_X50NG_STACK_BANKS");
        if (path && *path) {
            stack_bank_file = fopen(path, "wx");
            if (!stack_bank_file || atexit(stack_bank_finish)) {
                fputs("stack bank trace initialization failed\n", stderr);
                exit(1);
            }
        }
    }
    if (!stack_bank_file)
        return;
    struct stack_bank_observation *s = &stack_banks[from & 31u][to & 31u];
    if (!s->switches || outgoing < s->outgoing_min)
        s->outgoing_min = outgoing;
    if (!s->switches || outgoing > s->outgoing_max)
        s->outgoing_max = outgoing;
    if (!s->switches || incoming < s->incoming_min)
        s->incoming_min = incoming;
    if (!s->switches || incoming > s->incoming_max)
        s->incoming_max = incoming;
    ++s->switches;
}
#endif
