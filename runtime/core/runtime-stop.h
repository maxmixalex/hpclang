#ifndef HPCLANG_RUNTIME_STOP_H
#define HPCLANG_RUNTIME_STOP_H

/* Terminal native commit failure: retain ownership, mask IRQ/FIQ and stop.
 * Never call exit hooks, release mappings, retry or return to Stock. */
__attribute__((noreturn)) void hpclang_runtime_stop(void);

#endif
