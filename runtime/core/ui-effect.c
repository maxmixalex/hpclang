#include "ui-internal.h"
#include <errno.h>
#include <stdlib.h>

enum
{
    QUEUED = 1,
    ACTIVE,
    CANCELLED
};
static struct hpclang_ui *exit_head;
static int exit_registered, exiting;

static int owner_exists(const struct hpclang_ui *ui, uint32_t owner)
{
    if (!owner)
        return 1;
    const struct frame *frame = &ui->frames[ui->current];
    for (uint32_t i = 0; i < frame->count; ++i)
        if (frame->nodes[i].view.id == owner)
            return 1;
    return 0;
}

static void unlink_context(struct hpclang_ui *ui)
{
    if (!ui->registered)
        return;
    struct hpclang_ui **link = &exit_head;
    while (*link && *link != ui)
        link = &(*link)->exit_next;
    if (*link)
        *link = ui->exit_next;
    ui->registered = 0;
    ui->exit_next = NULL;
}

static void unlink_idle(struct hpclang_ui *ui)
{
    for (uint32_t i = 0; i < ui->capacity.nodes; ++i)
        if (ui->effects[i].state)
            return;
    unlink_context(ui);
}

static void finish(struct ui_effect *effect, int cancelled)
{
    struct hpclang_ui_effect_ops ops = effect->ops;
    void *context = effect->context;
    effect->state = 0;
    effect->token = 0;
    ops.end(context, cancelled);
}

static void close_context(struct hpclang_ui *ui)
{
    unlink_context(ui);
    ui->building = 0;
    ui->dispatching = 1;
    for (uint32_t i = 0; i < ui->capacity.nodes; ++i)
        if (ui->effects[i].state)
            finish(&ui->effects[i], 1);
    ui->dispatching = 0;
    ui->closed = 1;
}

static void exit_cleanup(void)
{
    exiting = 1;
    while (exit_head)
        close_context(exit_head);
}

static int ready(struct hpclang_ui *ui)
{
    if (!ui || ui->closed)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->building || ui->painting || ui->dispatching)
        return hpclang_ui_fail(NULL, EBUSY);
    return 0;
}

int hpclang_ui_effect_start(struct hpclang_ui *ui, uint32_t owner, struct hpclang_ui_action action,
                            const struct hpclang_ui_effect_ops *ops, void *context, uint32_t *token)
{
    if (ready(ui))
        return -1;
    if (!ops || !ops->begin || !ops->poll || !ops->end || !token)
        return hpclang_ui_fail(NULL, EINVAL);
    if (exiting)
        return hpclang_ui_fail(NULL, EPERM);
    if (!owner_exists(ui, owner))
        return hpclang_ui_fail(NULL, ENOENT);
    if (ui->effect_serial == UINT32_MAX)
        return hpclang_ui_fail(NULL, EOVERFLOW);
    struct ui_effect *slot = NULL;
    for (uint32_t i = 0; i < ui->capacity.nodes; ++i)
        if (!ui->effects[i].state) {
            slot = &ui->effects[i];
            break;
        }
    if (!slot)
        return hpclang_ui_fail(NULL, ENOSPC);
    int saved_errno = errno;
    if (!exit_registered) {
        if (atexit(exit_cleanup))
            return hpclang_ui_fail(NULL, ENOMEM);
        exit_registered = 1;
    }
    if (!ui->registered) {
        ui->exit_next = exit_head;
        exit_head = ui;
        ui->registered = 1;
    }
    *slot = (struct ui_effect){.ops = *ops,
                               .context = context,
                               .action = action,
                               .token = ++ui->effect_serial,
                               .owner = owner,
                               .state = QUEUED};
    *token = slot->token;
    errno = saved_errno;
    return 0;
}

int hpclang_ui_effect_cancel(struct hpclang_ui *ui, uint32_t token)
{
    if (ready(ui))
        return -1;
    for (uint32_t i = 0; i < ui->capacity.nodes; ++i)
        if (ui->effects[i].state && ui->effects[i].token == token) {
            ui->effects[i].state = CANCELLED;
            return 0;
        }
    return hpclang_ui_fail(NULL, ENOENT);
}

void hpclang_ui_effect_reconcile(struct hpclang_ui *ui)
{
    for (uint32_t i = 0; i < ui->capacity.nodes; ++i)
        if (ui->effects[i].state && !owner_exists(ui, ui->effects[i].owner))
            ui->effects[i].state = CANCELLED;
}

static int failure(int result)
{
    return result == -1 ? (errno ? errno : EIO) : EINVAL;
}

int hpclang_ui_effect_poll(struct hpclang_ui *ui, struct hpclang_ui_event *event)
{
    if (ready(ui))
        return -1;
    if (!event)
        return hpclang_ui_fail(NULL, EINVAL);
    int saved_errno = errno;
    ui->dispatching = 1;
    for (uint32_t visited = 0; visited < ui->capacity.nodes; ++visited) {
        struct ui_effect *effect = &ui->effects[ui->effect_cursor];
        ui->effect_cursor = ui->effect_cursor + 1 == ui->capacity.nodes ? 0 : ui->effect_cursor + 1;
        if (!effect->state)
            continue;
        if (effect->state == CANCELLED) {
            finish(effect, 1);
            continue;
        }
        int error = 0;
        if (effect->state == QUEUED) {
            errno = 0;
            int result = effect->ops.begin(effect->context, effect->token);
            if (result)
                error = failure(result);
            else
                effect->state = ACTIVE;
        }
        uint32_t value = 0;
        if (!error) {
            errno = 0;
            int result = effect->ops.poll(effect->context, &value);
            if (!result)
                continue;
            if (result != 1)
                error = failure(result);
        }
        struct hpclang_ui_event completion = {.source = effect->owner,
                                              .action = effect->action,
                                              .kind = error ? HPCLANG_UI_EFFECT_ERROR
                                                            : HPCLANG_UI_EFFECT_READY,
                                              .effect = effect->token,
                                              .value = error ? 0 : value,
                                              .error = error};
        finish(effect, 0);
        unlink_idle(ui);
        ui->dispatching = 0;
        *event = completion;
        errno = saved_errno;
        return 1;
    }
    unlink_idle(ui);
    ui->dispatching = 0;
    errno = saved_errno;
    return 0;
}

int hpclang_ui_shutdown(struct hpclang_ui *ui)
{
    if (!ui)
        return hpclang_ui_fail(NULL, EINVAL);
    if (ui->closed)
        return 0;
    if (ui->painting || ui->measuring || ui->dispatching)
        return hpclang_ui_fail(NULL, EBUSY);
    int saved_errno = errno;
    close_context(ui);
    errno = saved_errno;
    return 0;
}
