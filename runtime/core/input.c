#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/input.h>

static int valid(const struct hpclang_text_input *input)
{
    return input && input->buffer && input->capacity && input->length < input->capacity &&
           input->cursor <= input->length;
}

static int digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

/* Validate before conversion so a numeric prefix can never hide bad suffixes.
 * Read at most length + 1 bytes, including the editor's terminating NUL. */
static int number_text(const struct hpclang_text_input *input, int real, int *nonzero)
{
    if (!valid(input) || input->length == 0u || input->buffer[input->length] != '\0')
        return 0;
    const unsigned char *text = (const unsigned char *)input->buffer;
    size_t i = 0u, digits = 0u;
    *nonzero = 0;
    if (text[i] == '+' || text[i] == '-')
        ++i;
    while (i < input->length && digit(text[i])) {
        *nonzero |= text[i] != '0';
        ++i;
        ++digits;
    }
    if (real && i < input->length && text[i] == '.') {
        ++i;
        while (i < input->length && digit(text[i])) {
            *nonzero |= text[i] != '0';
            ++i;
            ++digits;
        }
    }
    if (digits == 0u)
        return 0;
    if (real && i < input->length && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        if (i < input->length && (text[i] == '+' || text[i] == '-'))
            ++i;
        size_t begin = i;
        while (i < input->length && digit(text[i]))
            ++i;
        if (i == begin)
            return 0;
    }
    return i == input->length;
}

int hpclang_text_input_parse_int64(const struct hpclang_text_input *input, int64_t *value)
{
    int nonzero;
    if (value == NULL || !number_text(input, 0, &nonzero)) {
        errno = EINVAL;
        return -1;
    }
    int negative = input->buffer[0] == '-';
    size_t i = negative || input->buffer[0] == '+' ? 1u : 0u;
    uint64_t magnitude = 0u;
    uint64_t limit = (uint64_t)INT64_MAX + (unsigned)negative;
    for (; i < input->length; ++i) {
        unsigned d = (unsigned)(input->buffer[i] - '0');
        if (magnitude > (limit - d) / 10u) {
            errno = ERANGE;
            return -1;
        }
        magnitude = magnitude * 10u + d;
    }
    *value = negative ? (magnitude == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)magnitude)
                      : (int64_t)magnitude;
    return 0;
}

int hpclang_text_input_parse_double(const struct hpclang_text_input *input, double *value)
{
    int nonzero;
    if (value == NULL || !number_text(input, 1, &nonzero)) {
        errno = EINVAL;
        return -1;
    }
    int saved_errno = errno;
    char *end;
    double converted = strtod(input->buffer, &end);
    if (end != input->buffer + input->length) {
        errno = EINVAL;
        return -1;
    }
    if (!isfinite(converted) || (nonzero && converted == 0.0)) {
        errno = ERANGE;
        return -1;
    }
    /* Libcs differ on setting ERANGE for representable subnormals. The clean
     * contract accepts those and reports only overflow or rounding to zero. */
    *value = converted;
    errno = saved_errno;
    return 0;
}
static size_t previous(const struct hpclang_text_input *input)
{
    size_t cursor = input->cursor;
    if (cursor)
        --cursor;
    while (cursor && ((unsigned char)input->buffer[cursor] & 0xc0) == 0x80)
        --cursor;
    return cursor;
}
static size_t next(const struct hpclang_text_input *input)
{
    size_t cursor = input->cursor;
    if (cursor < input->length)
        ++cursor;
    while (cursor < input->length && ((unsigned char)input->buffer[cursor] & 0xc0) == 0x80)
        ++cursor;
    return cursor;
}
int hpclang_text_input_init(struct hpclang_text_input *input, char *buffer, size_t capacity)
{
    if (!input || !buffer || !capacity) {
        errno = EINVAL;
        return -1;
    }
    *input = (struct hpclang_text_input){buffer, capacity, 0, 0};
    buffer[0] = 0;
    return 0;
}
int hpclang_text_input_edit(struct hpclang_text_input *input, enum hpclang_input_action action,
                            uint32_t codepoint)
{
    if (!valid(input)) {
        errno = EINVAL;
        return -1;
    }
    size_t begin = input->cursor, end = begin;
    switch (action) {
    case HPCLANG_INPUT_INSERT: {
        if (codepoint < 0x20 || (codepoint >= 0x7f && codepoint <= 0x9f) || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            errno = EINVAL;
            return -1;
        }
        unsigned count = codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
        if (count > input->capacity - input->length - 1) {
            errno = ENOSPC;
            return -1;
        }
        char bytes[4];
        uint32_t value = codepoint;
        for (unsigned i = count - 1; i; --i) {
            bytes[i] = (char)(0x80 | (value & 0x3f));
            value >>= 6;
        }
        static const unsigned prefixes[] = {0, 0, 0xc0, 0xe0, 0xf0};
        bytes[0] = (char)(prefixes[count] | value);
        memmove(input->buffer + begin + count, input->buffer + begin, input->length - begin + 1);
        memcpy(input->buffer + begin, bytes, count);
        input->length += count;
        input->cursor += count;
        return HPCLANG_INPUT_EDITING;
    }
    case HPCLANG_INPUT_LEFT:
        input->cursor = previous(input);
        return HPCLANG_INPUT_EDITING;
    case HPCLANG_INPUT_RIGHT:
        input->cursor = next(input);
        return HPCLANG_INPUT_EDITING;
    case HPCLANG_INPUT_HOME:
        input->cursor = 0;
        return HPCLANG_INPUT_EDITING;
    case HPCLANG_INPUT_END:
        input->cursor = input->length;
        return HPCLANG_INPUT_EDITING;
    case HPCLANG_INPUT_BACKSPACE:
        begin = previous(input);
        break;
    case HPCLANG_INPUT_DELETE:
        end = next(input);
        break;
    case HPCLANG_INPUT_ACCEPT:
        return HPCLANG_INPUT_ACCEPTED;
    case HPCLANG_INPUT_CANCEL:
        return HPCLANG_INPUT_CANCELLED;
    default:
        errno = EINVAL;
        return -1;
    }
    memmove(input->buffer + begin, input->buffer + end, input->length - end + 1);
    input->length -= end - begin;
    input->cursor = begin;
    return HPCLANG_INPUT_EDITING;
}

int hpclang_text_input_key(struct hpclang_text_input *input,
                           const struct hpclang_keyboard_event *event)
{
    if (!valid(input) || !event) {
        errno = EINVAL;
        return -1;
    }
    if (event->type == HPCLANG_KEYBOARD_EVENT_UP)
        return HPCLANG_INPUT_EDITING;
    if (event->type != HPCLANG_KEYBOARD_EVENT_DOWN &&
        event->type != HPCLANG_KEYBOARD_EVENT_REPEAT) {
        errno = EINVAL;
        return -1;
    }
    enum hpclang_input_action action;
    switch (event->key) {
    case HPCLANG_KEY_ENTER:
        action = HPCLANG_INPUT_ACCEPT;
        break;
    case HPCLANG_KEY_ON:
        action = HPCLANG_INPUT_CANCEL;
        break;
    case HPCLANG_KEY_LEFT:
        action = HPCLANG_INPUT_LEFT;
        break;
    case HPCLANG_KEY_RIGHT:
        action = HPCLANG_INPUT_RIGHT;
        break;
    case HPCLANG_KEY_HP39_DEL:
    case HPCLANG_KEY_HP48_BACKSPACE:
    case HPCLANG_KEY_HP4950_BACKSPACE:
        action = HPCLANG_INPUT_BACKSPACE;
        break;
    default: {
        if (event->modifiers)
            return HPCLANG_INPUT_EDITING;
        uint32_t cp;
        if (event->key >= HPCLANG_KEY_0 && event->key <= HPCLANG_KEY_9)
            cp = '0' + event->key - HPCLANG_KEY_0;
        else
            switch (event->key) {
            case HPCLANG_KEY_PERIOD:
                cp = '.';
                break;
            case HPCLANG_KEY_PLUS:
                cp = '+';
                break;
            case HPCLANG_KEY_MINUS:
                cp = '-';
                break;
            case HPCLANG_KEY_MULTIPLY:
                cp = '*';
                break;
            case HPCLANG_KEY_DIVIDE:
                cp = '/';
                break;
            case HPCLANG_KEY_HP48_SPACE:
            case HPCLANG_KEY_HP4950_SPACE:
                cp = ' ';
                break;
            default:
                return HPCLANG_INPUT_EDITING;
            }
        return hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, cp);
    }
    }
    return hpclang_text_input_edit(input, action, 0);
}
