#include <hpclang/ui.h>
#include <assert.h>
#include <stdlib.h>

struct task
{
    unsigned begins, polls, ends;
};
static struct task tasks[2];
static void *storage[2];

static int begin(void *context, uint32_t token)
{
    assert(token);
    ++((struct task *)context)->begins;
    return 0;
}
static int poll(void *context, uint32_t *value)
{
    (void)value;
    ++((struct task *)context)->polls;
    return 0;
}
static void end(void *context, int cancelled)
{
    assert(cancelled);
    ++((struct task *)context)->ends;
}
static void verify(void)
{
    assert(tasks[0].begins == 0 && tasks[0].ends == 1);
    assert(tasks[1].begins == 1 && tasks[1].polls == 1 && tasks[1].ends == 1);
    free(storage[0]);
    free(storage[1]);
}
int main(void)
{
    /* Register observation first so it runs after the UI's actual atexit hook. */
    assert(atexit(verify) == 0);
    const struct hpclang_ui_effect_ops ops = {begin, poll, end};
    const struct hpclang_ui_config config = {1, 1, 1};
    for (unsigned i = 0; i < 2; ++i) {
        size_t bytes = hpclang_ui_storage_size(config);
        storage[i] = malloc(bytes);
        assert(storage[i]);
        struct hpclang_ui *ui = hpclang_ui_init(storage[i], bytes, config);
        uint32_t token;
        assert(hpclang_ui_effect_start(ui, 0, (struct hpclang_ui_action){0}, &ops, &tasks[i],
                                       &token) == 0);
        if (i) {
            struct hpclang_ui_event event;
            assert(hpclang_ui_effect_poll(ui, &event) == 0);
        }
    }
    return 0;
}
