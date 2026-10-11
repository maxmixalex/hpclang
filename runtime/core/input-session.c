#include <errno.h>
#include <hpclang/input.h>

static int failure(void)
{
    if (!errno)
        errno = EIO;
    return -1;
}

static int update(const struct hpclang_text_input_callbacks *io,
                  const struct hpclang_text_input *input, int result, int error)
{
    if (!io->update)
        return 0;
    errno = 0;
    int status = io->update(io->context, input, result, error);
    if (status == 0)
        return 0;
    if (status != -1)
        errno = EINVAL;
    return failure();
}

int hpclang_text_input_run(struct hpclang_text_input *input,
                           const struct hpclang_text_input_callbacks *callbacks)
{
    if (!input || !input->buffer || !input->capacity || input->length >= input->capacity ||
        input->cursor > input->length || input->buffer[input->length] != '\0' || !callbacks ||
        !callbacks->next) {
        errno = EINVAL;
        return -1;
    }
    int saved_errno = errno;
    const struct hpclang_text_input_callbacks io = *callbacks;
    if (update(&io, input, HPCLANG_INPUT_EDITING, 0) != 0)
        return -1;
    for (;;) {
        struct hpclang_keyboard_event event = {0};
        errno = 0;
        int status = io.next(io.context, &event);
        if (status == 0)
            continue;
        if (status != 1) {
            if (status != -1)
                errno = EINVAL;
            return failure();
        }
        errno = 0;
        int result =
            io.edit ? io.edit(io.context, input, &event) : hpclang_text_input_key(input, &event);
        int error = 0;
        if (result == -1) {
            if (errno != ENOSPC)
                return failure();
            error = ENOSPC;
            result = HPCLANG_INPUT_EDITING;
        } else if (result < HPCLANG_INPUT_EDITING || result > HPCLANG_INPUT_CANCELLED) {
            errno = EINVAL;
            return -1;
        }
        if (result == HPCLANG_INPUT_ACCEPTED && io.accept) {
            errno = 0;
            status = io.accept(io.context, input);
            if (status == 1) {
                error = errno ? errno : EIO;
                result = HPCLANG_INPUT_EDITING;
            } else if (status != 0) {
                if (status != -1)
                    errno = EINVAL;
                return failure();
            }
        }
        if (update(&io, input, result, error) != 0)
            return -1;
        if (result != HPCLANG_INPUT_EDITING) {
            errno = saved_errno;
            return result;
        }
    }
}
