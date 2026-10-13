#ifndef HPCLANG_INPUT_EVENT_PLAN_H
#define HPCLANG_INPUT_EVENT_PLAN_H
#include <hpclang/input.h>

/* Shared native decoding for the line editor and controlled UI. Planning
 * consumes modifier state once; applying a delimiter pair is atomic. */
struct hpclang_input_plan
{
    enum hpclang_input_action action;
    uint32_t codepoint, closing;
};
int hpclang_input_plan_event(struct hpclang_text_mode *mode,
                             const struct hpclang_keyboard_event *event,
                             struct hpclang_input_plan *plan);
int hpclang_input_plan_apply(struct hpclang_text_input *input,
                             const struct hpclang_input_plan *plan);
#endif
