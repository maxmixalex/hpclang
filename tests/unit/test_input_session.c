#include <assert.h>
#include <errno.h>
#include <string.h>
#include <hpclang/input.h>

struct session
{
    const unsigned *keys;
    unsigned count, offset, updates, full, rejected, idle;
    int next_error, update_error, accept_error;
    int64_t value;
};
static int next(void *context, struct hpclang_keyboard_event *event)
{
    struct session *s = context;
    if (s->idle) {
        --s->idle;
        return 0;
    }
    if (s->offset == s->count) {
        errno = s->next_error;
        return -1;
    }
    *event = (struct hpclang_keyboard_event){.type = HPCLANG_KEYBOARD_EVENT_DOWN,
                                             .key = s->keys[s->offset++]};
    return 1;
}
static int update(void *context, const struct hpclang_text_input *input, int result, int error)
{
    struct session *s = context;
    assert(input->buffer[input->length] == 0);
    assert(result >= HPCLANG_INPUT_EDITING && result <= HPCLANG_INPUT_CANCELLED);
    ++s->updates;
    s->full += error == ENOSPC;
    s->rejected += error == EINVAL;
    if (s->update_error) {
        errno = s->update_error;
        return -1;
    }
    return 0;
}
static int accept(void *context, const struct hpclang_text_input *input)
{
    struct session *s = context;
    if (s->accept_error) {
        errno = s->accept_error;
        return -1;
    }
    return hpclang_text_input_parse_int64(input, &s->value) == 0 ? 0 : 1;
}
static int invalid_next(void *context, struct hpclang_keyboard_event *event)
{
    (void)context;
    (void)event;
    return 2;
}
static int unicode_edit(void *context, struct hpclang_text_input *input,
                        const struct hpclang_keyboard_event *event)
{
    (void)context;
    if (event->key == HPCLANG_KEY_F1)
        return hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, 0x03a9);
    return hpclang_text_input_key(input, event);
}
int main(void)
{
    char buffer[4];
    struct hpclang_text_input input;
    assert(hpclang_text_input_init(&input, buffer, sizeof(buffer)) == 0);
    const unsigned keys[] = {HPCLANG_KEY_MINUS, HPCLANG_KEY_ENTER, HPCLANG_KEY_1,
                             HPCLANG_KEY_2,     HPCLANG_KEY_3,     HPCLANG_KEY_HP48_BACKSPACE,
                             HPCLANG_KEY_3,     HPCLANG_KEY_ENTER};
    struct session s = {.keys = keys, .count = sizeof(keys) / sizeof(keys[0]), .idle = 3};
    struct hpclang_text_input_callbacks io = {next, NULL, accept, update, &s};
    errno = EDOM;
    assert(hpclang_text_input_run(&input, &io) == HPCLANG_INPUT_ACCEPTED);
    assert(errno == EDOM && s.value == -13 && !strcmp(buffer, "-13"));
    assert(s.full == 1 && s.rejected == 1 && s.updates == s.count + 1);
    const unsigned cancel[] = {HPCLANG_KEY_ON};
    s = (struct session){.keys = cancel, .count = 1};
    assert(hpclang_text_input_run(&input, &io) == HPCLANG_INPUT_CANCELLED);
    assert(!strcmp(buffer, "-13") && errno == EDOM);
    s = (struct session){.next_error = ECANCELED};
    assert(hpclang_text_input_run(&input, &io) == -1 && errno == ECANCELED);
    s = (struct session){0};
    assert(hpclang_text_input_run(&input, &io) == -1 && errno == EIO);
    s.update_error = ENODEV;
    assert(hpclang_text_input_run(&input, &io) == -1 && errno == ENODEV && s.offset == 0);
    const unsigned enter[] = {HPCLANG_KEY_ENTER};
    s = (struct session){.keys = enter, .count = 1, .accept_error = ERANGE};
    assert(hpclang_text_input_run(&input, &io) == -1 && errno == ERANGE);
    io.next = invalid_next;
    s.update_error = 0;
    assert(hpclang_text_input_run(&input, &io) == -1 && errno == EINVAL);
    io.next = NULL;
    assert(hpclang_text_input_run(&input, &io) == -1 && errno == EINVAL);
    assert(hpclang_text_input_run(NULL, &io) == -1 && errno == EINVAL);
    assert(hpclang_text_input_run(&input, NULL) == -1 && errno == EINVAL);
    assert(hpclang_text_input_init(&input, buffer, sizeof(buffer)) == 0);
    const unsigned unicode[] = {HPCLANG_KEY_F1, HPCLANG_KEY_ENTER};
    s = (struct session){.keys = unicode, .count = 2};
    io = (struct hpclang_text_input_callbacks){next, unicode_edit, NULL, NULL, &s};
    assert(hpclang_text_input_run(&input, &io) == HPCLANG_INPUT_ACCEPTED);
    assert(!strcmp(buffer, "\xce\xa9"));
    return 0;
}
