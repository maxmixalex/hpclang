#ifndef HPCLANG_TIMER_ENGINE_H
#define HPCLANG_TIMER_ENGINE_H
#include <hpclang/timer.h>
int hpclang_timer_start_at(struct hpclang_timer *timer, uint64_t now, uint64_t delay,
                           uint64_t period);
int hpclang_timer_poll_at(struct hpclang_timer *timer, uint64_t now, uint64_t *count);
#endif
