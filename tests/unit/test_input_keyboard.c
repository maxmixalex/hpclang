#include <assert.h>
#include <errno.h>
#include <string.h>
#include <hpclang/input.h>

static int lookup(uint32_t model, uint16_t key, enum hpclang_text_plane plane, uint32_t *cp)
{
    return hpclang_text_key_lookup(model, key, plane, 0, cp);
}

static int key(struct hpclang_text_input *input, struct hpclang_text_mode *mode, uint16_t k)
{
    struct hpclang_keyboard_event event = {.type = HPCLANG_KEYBOARD_EVENT_DOWN, .key = k};
    int result = hpclang_text_input_event(input, mode, &event);
    event.type = HPCLANG_KEYBOARD_EVENT_UP;
    assert(hpclang_text_input_event(input, mode, &event) == HPCLANG_INPUT_EDITING);
    return result;
}

static int next(void *context, struct hpclang_keyboard_event *event)
{
    unsigned *index = context;
    const uint16_t keys[] = {HPCLANG_KEY_ALPHA, HPCLANG_KEY_RIGHT_SHIFT, HPCLANG_KEY_F1,
                             HPCLANG_KEY_2, HPCLANG_KEY_ENTER};
    assert(*index < sizeof(keys) / sizeof(keys[0]));
    *event = (struct hpclang_keyboard_event){.key = keys[(*index)++],
                                             .type = HPCLANG_KEYBOARD_EVENT_DOWN};
    return 1;
}

int main(void)
{
    for (uint32_t model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        struct hpclang_text_input input;
        struct hpclang_text_mode mode;
        char text[32];
        assert(hpclang_text_input_init(&input, text, sizeof(text)) == 0);
        assert(hpclang_text_mode_init(&mode, model) == 0);
        int hp39 = model <= HPCLANG_MODEL_HP40GS;
        uint16_t a = hp39 ? HPCLANG_KEY_HP39_VARS : HPCLANG_KEY_F1;
        errno = EDOM;
        assert(key(&input, &mode, HPCLANG_KEY_ALPHA) == 0);
        assert(key(&input, &mode, a) == 0 && !strcmp(text, "A"));
        assert(key(&input, &mode, HPCLANG_KEY_2) == 0 && !strcmp(text, "A2"));
        assert(key(&input, &mode, HPCLANG_KEY_ENTER) == HPCLANG_INPUT_ACCEPTED);
        assert(key(&input, &mode, HPCLANG_KEY_ON) == HPCLANG_INPUT_CANCELLED);
        assert(errno == EDOM);
        assert(hpclang_text_mode_set_lock(&mode, HPCLANG_TEXT_LOCK_UPPER) == 0);
        if (hp39) {
            assert(key(&input, &mode, HPCLANG_KEY_HP39_SHIFT) == 0);
            assert(key(&input, &mode, a) == 0 && !strcmp(text, "A2a"));
        } else {
            assert(key(&input, &mode, HPCLANG_KEY_RIGHT_SHIFT) == 0);
            assert(key(&input, &mode, a) == 0 && !strcmp(text, "A2\xce\xb1"));
            assert(key(&input, &mode, HPCLANG_KEY_LEFT) == 0 && input.cursor == 2);
            assert(key(&input, &mode, HPCLANG_KEY_F2) == 0 && !strcmp(text, "A2B\xce\xb1"));
        }
        struct hpclang_text_mode before = mode;
        assert(hpclang_text_input_event(NULL, &mode, NULL) == -1 && errno == EINVAL);
        assert(!memcmp(&before, &mode, sizeof(mode)));
        uint32_t cp = 42;
        assert(lookup(model, HPCLANG_KEY_F1, (enum hpclang_text_plane)99, &cp) == -1);
        assert(errno == EINVAL && cp == 42);
        assert(lookup(model, HPCLANG_KEY_UNKNOWN, HPCLANG_TEXT_PLAIN, &cp) == 0);
        assert(cp == 42);
    }
    uint32_t cp = 42;
    assert(lookup(0, HPCLANG_KEY_F1, HPCLANG_TEXT_PLAIN, &cp) == -1);
    assert(errno == ENOSYS && cp == 42);
    assert(lookup(6, HPCLANG_KEY_F1, HPCLANG_TEXT_PLAIN, NULL) == -1);
    assert(errno == EINVAL);
    assert(lookup(6, HPCLANG_KEY_HP4950_SYMB, HPCLANG_TEXT_ALPHA_SYMBOL, &cp) == 1);
    assert(cp == 0x03a0);
    assert(lookup(4, HPCLANG_KEY_HP48_TOOL, HPCLANG_TEXT_ALPHA_SYMBOL, &cp) == 1);
    assert(cp == '|');
    assert(lookup(1, HPCLANG_KEY_HP39_COMMA, HPCLANG_TEXT_PLAIN, &cp) == 1);
    assert(cp == ',');
    assert(lookup(6, HPCLANG_KEY_HP39_COMMA, HPCLANG_TEXT_PLAIN, &cp) == 0);
    assert(cp == ',');
    struct hpclang_text_input input;
    struct hpclang_text_mode mode;
    char small[2];
    assert(hpclang_text_input_init(&input, small, sizeof(small)) == 0);
    assert(hpclang_text_mode_init(&mode, HPCLANG_MODEL_HP50G) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_ALPHA) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_RIGHT_SHIFT) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_F1) == -1 && errno == ENOSPC && !small[0]);
    /* Capacity rejection consumes one-shot Alpha: the next physical 2 is a digit. */
    assert(key(&input, &mode, HPCLANG_KEY_2) == 0 && !strcmp(small, "2"));
    char line[16];
    unsigned index = 0;
    assert(hpclang_text_input_init(&input, line, sizeof(line)) == 0);
    assert(hpclang_text_mode_init(&mode, HPCLANG_MODEL_HP50G) == 0);
    const struct hpclang_text_input_callbacks callbacks = {
        .next = next, .context = &index, .mode = &mode};
    errno = EDOM;
    assert(hpclang_text_input_run(&input, &callbacks) == HPCLANG_INPUT_ACCEPTED);
    assert(!strcmp(line, "\xce\xb1"
                         "2") &&
           errno == EDOM && index == 5);
    assert(hpclang_text_key_lookup(6, HPCLANG_KEY_6, HPCLANG_TEXT_ALPHA_SYMBOL,
                                   HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT, &cp) == 1 &&
           cp == 0x00b0);
    assert(lookup(6, HPCLANG_KEY_6, HPCLANG_TEXT_ALPHA_SYMBOL, &cp) == 1 && cp == 0x2220);
    assert(lookup(1, HPCLANG_KEY_0, HPCLANG_TEXT_ALPHA_UPPER, &cp) == 1 && cp == 0x03b8);
    assert(hpclang_text_input_init(&input, line, sizeof(line)) == 0);
    assert(hpclang_text_mode_init(&mode, HPCLANG_MODEL_HP50G) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_LEFT_SHIFT) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_MULTIPLY) == 0);
    assert(!strcmp(line, "[]") && input.cursor == 1);
    assert(key(&input, &mode, HPCLANG_KEY_2) == 0 && !strcmp(line, "[2]"));
    assert(hpclang_text_input_init(&input, line, sizeof(line)) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_RIGHT_SHIFT) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_PLUS) == 0);
    assert(!strcmp(line, "\xc2\xab\xc2\xbb") && input.cursor == 2);
    assert(hpclang_text_input_init(&input, line, sizeof(line)) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_RIGHT_SHIFT) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_MULTIPLY) == 0);
    assert(!strcmp(line, "\"\"") && input.cursor == 1);
    assert(hpclang_text_input_init(&input, small, sizeof(small)) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_LEFT_SHIFT) == 0);
    assert(key(&input, &mode, HPCLANG_KEY_MULTIPLY) == -1 && errno == ENOSPC);
    assert(!small[0] && input.length == 0 && input.cursor == 0);
    return 0;
}
