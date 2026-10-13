#include "app.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

struct hpclang_ui_surface *demo_renderer(unsigned width, unsigned height, unsigned levels,
                                         const struct hpclang_font *font, int inverted)
{
    uint8_t foreground = inverted ? 0 : 255, background = inverted ? 255 : 0;
    struct hpclang_ui_surface_style styles[2] = {{.font = font,
                                                  .foreground = foreground,
                                                  .background = background,
                                                  .disabled = inverted ? 170 : 85,
                                                  .accent = foreground,
                                                  .gap = 2},
                                                 {.font = font,
                                                  .foreground = foreground,
                                                  .background = background,
                                                  .disabled = inverted ? 170 : 85,
                                                  .accent = foreground,
                                                  .padding_x = 2,
                                                  .padding_y = 1}};
    return hpclang_ui_surface_create(width, height, levels, styles, 2, background);
}

static const char *scan_name(enum demo_scan scan)
{
    switch (scan) {
    case DEMO_IDLE:
        return "Idle";
    case DEMO_LOADING:
        return "Loading";
    case DEMO_READY:
        return "Ready";
    case DEMO_ERROR:
        return "Error";
    }
    return "Unknown";
}

static void status_card(struct hpclang_ui *ui, uint32_t instance, const char *title,
                        const char *value)
{
    HPCLANG_UI_WITH(ui, instance, HPCLANG_UI_COMPONENT, 0)
    {
        HPCLANG_UI_WITH(ui, 1, HPCLANG_UI_ROW, 0)
        {
            hpclang_ui_text(ui, 1, 0, title);
            hpclang_ui_text(ui, 2, 0, value);
        }
    }
}

static void button(struct hpclang_ui *ui, uint32_t key, const char *title, enum demo_action action,
                   uint32_t arg, int enabled)
{
    hpclang_ui_button(ui, key, 1, title, (struct hpclang_ui_action){action, arg}, enabled);
}

static void device_row(struct hpclang_ui *ui, const struct demo_device *device)
{
    HPCLANG_UI_WITH(ui, device->id, HPCLANG_UI_ROW, 0)
    {
        hpclang_ui_text(ui, 1, 0, device->name);
        hpclang_ui_toggle(ui, 2, 1, "Enabled", device->enabled,
                          (struct hpclang_ui_action){DEMO_TOGGLE, device->id}, 1);
    }
}

static int wave(void *paint, const void *state, struct hpclang_ui_rect bounds,
                struct hpclang_ui_rect clip)
{
    (void)clip;
    const struct demo_model *model = state;
    struct hpclang_surface *surface = paint;
    hpclang_surface_clear(surface, 0);
    for (unsigned i = 1; i < 32; ++i) {
        int x0 = bounds.x + (int)((i - 1) * (unsigned)(bounds.width - 1) / 31);
        int x1 = bounds.x + (int)(i * (unsigned)(bounds.width - 1) / 31);
        int y0 = bounds.y + bounds.height - 1 - model->samples[i - 1] * (bounds.height - 1) / 1023;
        int y1 = bounds.y + bounds.height - 1 - model->samples[i] * (bounds.height - 1) / 1023;
        hpclang_surface_draw_line(surface, x0, y0, x1, y1, 255);
    }
    return 0;
}

int demo_view(struct demo_app *app, struct hpclang_ui_surface *renderer, int width, int height)
{
    struct hpclang_ui *ui = app->ui;
    const struct demo_model *m = &app->model;
    if (hpclang_ui_begin(ui))
        return -1;
    HPCLANG_UI_SCOPE(hpclang_ui_scroll_begin(ui, 1, 0, height))
    {
        hpclang_ui_text(ui, 2, 0, "Device manager");
        status_card(ui, 3, "Link:", m->connected ? "Connected" : "Offline");
        status_card(ui, 4, "Scan:", scan_name(m->scan));
        if (m->connected)
            button(ui, 5, "Disconnect", DEMO_DISCONNECT, 0, 1);
        else
            button(ui, 6, "Connect", DEMO_CONNECT, 0, 1);
        hpclang_ui_text(ui, 7, 0, "Filter (Alpha to type)");
        hpclang_ui_text_input(ui, 8, 1, m->filter, sizeof(m->filter),
                              (struct hpclang_ui_action){DEMO_FILTER, 0}, 1);
        HPCLANG_UI_WITH(ui, 9, HPCLANG_UI_COLUMN, 0)
        {
            if (m->scan == DEMO_LOADING) {
                hpclang_ui_text(ui, 1, 0, "Scanning...");
                button(ui, 2, "Cancel scan", DEMO_CANCEL, 0, 1);
            } else if (m->scan == DEMO_ERROR) {
                hpclang_ui_text(ui, 1, 0, "Scan failed (demo)");
                button(ui, 2, "Retry", DEMO_SCAN, 0, m->connected);
            } else if (m->scan == DEMO_READY) {
                HPCLANG_UI_WITH(ui, 3, HPCLANG_UI_COLUMN, 0)
                {
                    for (unsigned i = 0; i < m->count; ++i)
                        if (!m->filter[0] || strstr(m->devices[i].name, m->filter))
                            device_row(ui, &m->devices[i]);
                }
                HPCLANG_UI_WITH(ui, 4, HPCLANG_UI_ROW, 0)
                {
                    button(ui, 1, "Insert", DEMO_ADD, 0, m->count < 8);
                    button(ui, 2, "Reverse", DEMO_REVERSE, 0, m->count > 1);
                }
                button(ui, 5, "Scan again", DEMO_SCAN, 0, m->connected);
            } else
                button(ui, 2, "Scan devices", DEMO_SCAN, 0, m->connected);
        }
        hpclang_ui_canvas(ui, 10, 0, wave, m, m->revision, 24);
        button(ui, 11, "Change wave", DEMO_WAVE, 0, 1);
        button(ui, 12, "Exit", DEMO_EXIT, 0, 1);
    }
    if (hpclang_ui_layout(ui, (struct hpclang_ui_rect){0, 0, width, height},
                          hpclang_ui_surface_measure, renderer)) {
        hpclang_ui_abort(ui);
        return -1;
    }
    return hpclang_ui_commit(ui);
}

static void update_wave(struct demo_model *m)
{
    ++m->revision;
    for (unsigned i = 0; i < 32; ++i) {
        unsigned phase = (i + m->revision * 3u) % 32u;
        m->samples[i] = (uint16_t)((phase < 16 ? phase : 31 - phase) * 64);
    }
}

void demo_init(struct demo_app *app, struct hpclang_ui *ui)
{
    *app = (struct demo_app){.ui = ui, .model = {.next_id = 1}};
    update_wave(&app->model);
}

static void insert_device(struct demo_model *m)
{
    if (m->count == 8 || m->next_id == UINT32_MAX)
        return;
    memmove(m->devices + 1, m->devices, m->count * sizeof(*m->devices));
    m->devices[0] = (struct demo_device){.id = m->next_id++, .enabled = 1};
    snprintf(m->devices[0].name, sizeof(m->devices[0].name), "Device %lu",
             (unsigned long)m->devices[0].id);
    ++m->count;
}

static int scan_begin(void *context, uint32_t token)
{
    (void)token;
    struct demo_app *app = context;
    app->polls = 0;
    return 0;
}
static int scan_poll(void *context, uint32_t *value)
{
    struct demo_app *app = context;
    if (++app->polls < 20)
        return 0;
    if (app->attempts == 1) {
        errno = EIO;
        return -1;
    }
    *value = 3;
    return 1;
}
static void scan_end(void *context, int cancelled)
{
    (void)cancelled;
    ++((struct demo_app *)context)->cleanups;
}
static const struct hpclang_ui_effect_ops scan_ops = {scan_begin, scan_poll, scan_end};

int demo_update(struct demo_app *app, const struct hpclang_ui_event *event)
{
    struct demo_model *m = &app->model;
    if (event->kind == HPCLANG_UI_EFFECT_READY || event->kind == HPCLANG_UI_EFFECT_ERROR) {
        if (event->effect != app->request || m->scan != DEMO_LOADING)
            return 0;
        app->request = 0;
        m->scan = event->kind == HPCLANG_UI_EFFECT_READY ? DEMO_READY : DEMO_ERROR;
        if (m->scan == DEMO_READY && !m->count)
            for (unsigned i = 0; i < event->value && i < 8; ++i)
                insert_device(m);
        return 1;
    }
    if (event->action.type == DEMO_FILTER && event->kind == HPCLANG_UI_CHANGE) {
        if (!event->text || strlen(event->text) >= sizeof(m->filter)) {
            errno = EINVAL;
            return -1;
        }
        strcpy(m->filter, event->text);
        return 1;
    }
    if (event->action.type == DEMO_TOGGLE && event->kind == HPCLANG_UI_CHANGE) {
        if (event->value > 1) {
            errno = EINVAL;
            return -1;
        }
        for (unsigned i = 0; i < m->count; ++i)
            if (m->devices[i].id == event->action.arg)
                m->devices[i].enabled = (int)event->value;
        return 1;
    }
    if (event->kind != HPCLANG_UI_ACTIVATE)
        return 0;
    switch (event->action.type) {
    case DEMO_CONNECT:
        m->connected = 1;
        break;
    case DEMO_DISCONNECT:
    case DEMO_CANCEL:
        if (app->request) {
            if (hpclang_ui_effect_cancel(app->ui, app->request))
                return -1;
            /* Drain before the application reuses the provider context. */
            struct hpclang_ui_event ignored;
            if (hpclang_ui_effect_poll(app->ui, &ignored) < 0)
                return -1;
            app->request = 0;
        }
        m->scan = DEMO_IDLE;
        if (event->action.type == DEMO_DISCONNECT)
            m->connected = 0;
        break;
    case DEMO_SCAN:
        if (!m->connected || m->scan == DEMO_LOADING)
            return 0;
        if (hpclang_ui_effect_start(app->ui, 0, (struct hpclang_ui_action){DEMO_SCAN, 0}, &scan_ops,
                                    app, &app->request))
            return -1;
        ++app->attempts;
        m->scan = DEMO_LOADING;
        break;
    case DEMO_ADD:
        insert_device(m);
        break;
    case DEMO_REVERSE:
        for (unsigned i = 0; i < m->count / 2; ++i) {
            struct demo_device temp = m->devices[i];
            m->devices[i] = m->devices[m->count - 1 - i];
            m->devices[m->count - 1 - i] = temp;
        }
        break;
    case DEMO_WAVE:
        update_wave(m);
        break;
    case DEMO_EXIT:
        m->quit = 1;
        break;
    default:
        return 0;
    }
    return 1;
}

int demo_poll(struct demo_app *app)
{
    struct hpclang_ui_event event;
    int result = hpclang_ui_effect_poll(app->ui, &event);
    return result > 0 ? demo_update(app, &event) : result;
}
