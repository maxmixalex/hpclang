#include <errno.h>

/*
 * Keep the canonical Stock launcher implementation in one place. This wrapper
 * only adds a host-completion gate to sequence scheduling, then reuses every
 * existing launcher helper and return-state check unchanged.
 */
#define main hpclang_stock_main_base
#define schedule_sequence hpclang_stock_schedule_sequence_base
#include "stock-launcher-main-base.c"
#undef schedule_sequence
#undef main

#define HOST_GATE_POLL_US 100000LL

struct hpclang_host_gate_event {
    struct state *state;
    char post_sequence[768];
};

static void hpclang_host_gate_cb(void *arg)
{
    struct hpclang_host_gate_event *event = arg;
    struct state *s = event->state;
    const char *marker = getenv("HPCLANG_E2E_HOST_GATE_FILE");
    FILE *ready;

    if (marker == NULL || *marker == '\0') {
        free(event);
        fail(s, "host-gate-config");
        return;
    }

    ready = fopen(marker, "rb");
    if (ready == NULL) {
        if (errno != ENOENT) {
            free(event);
            fail(s, "host-gate-read");
            return;
        }
        if (!later(hpclang_host_gate_cb, event, HOST_GATE_POLL_US)) {
            free(event);
            fail(s, "host-gate-timer");
        }
        return;
    }
    fclose(ready);

    fprintf(stderr, "stock launcher: host gate released: %s\n", marker);
    if (!hpclang_stock_schedule_sequence_base(s, event->post_sequence)) {
        remove(event->post_sequence);
        free(event);
        fail(s, "host-gate-sequence");
        return;
    }

    remove(event->post_sequence);
    free(event);
}

static int hpclang_schedule_sequence_with_host_gate(
    struct state *s, const char *path)
{
    FILE *source = NULL;
    FILE *pre = NULL;
    FILE *post = NULL;
    char line[256];
    char pre_path[768];
    char post_path[768];
    unsigned line_no = 0;
    unsigned long long gate_ms = 0;
    int found_gate = 0;
    int ok = 0;

    snprintf(pre_path, sizeof(pre_path), "%s-sequence-pre.seq", s->prefix);
    snprintf(post_path, sizeof(post_path), "%s-sequence-post.seq", s->prefix);

    source = fopen(path, "r");
    if (source == NULL) {
        fprintf(stderr, "cannot open E2E sequence %s\n", path);
        goto out;
    }
    pre = fopen(pre_path, "w");
    if (pre == NULL)
        goto out;

    while (fgets(line, sizeof(line), source)) {
        unsigned long long ms = 0;
        char op[32] = {0};
        char first[96] = {0};
        char second[96] = {0};
        char *p = line;
        int fields;

        ++line_no;
        while (*p == ' ' || *p == '\t') ++p;
        fields = (*p == '\0' || *p == '\n' || *p == '#')
            ? 0
            : sscanf(p, "%llu %31s %95s %95s",
                     &ms, op, first, second);

        if (!found_gate && fields >= 2 && strcmp(op, "host-gate") == 0) {
            const char *marker = getenv("HPCLANG_E2E_HOST_GATE_FILE");
            if (fields != 2 || ms > 600000ULL ||
                marker == NULL || *marker == '\0') {
                fprintf(stderr,
                        "%s:%u: invalid host gate or missing "
                        "HPCLANG_E2E_HOST_GATE_FILE\n",
                        path, line_no);
                goto out;
            }
            found_gate = 1;
            gate_ms = ms;
            post = fopen(post_path, "w");
            if (post == NULL)
                goto out;
            continue;
        }

        if (found_gate && fields >= 2 && strcmp(op, "host-gate") == 0) {
            fprintf(stderr, "%s:%u: only one host gate is supported\n",
                    path, line_no);
            goto out;
        }

        if (fputs(line, found_gate ? post : pre) == EOF)
            goto out;
    }

    if (ferror(source))
        goto out;

    fclose(source);
    source = NULL;
    fclose(pre);
    pre = NULL;
    if (post != NULL) {
        fclose(post);
        post = NULL;
    }

    if (!found_gate) {
        remove(pre_path);
        return hpclang_stock_schedule_sequence_base(s, path);
    }

    if (!hpclang_stock_schedule_sequence_base(s, pre_path))
        goto out;
    remove(pre_path);

    struct hpclang_host_gate_event *event = calloc(1, sizeof(*event));
    if (event == NULL)
        goto out;
    event->state = s;
    if (snprintf(
            event->post_sequence, sizeof(event->post_sequence),
            "%s", post_path) >= (int)sizeof(event->post_sequence)) {
        free(event);
        goto out;
    }
    if (!later(hpclang_host_gate_cb, event, (int64_t)gate_ms * 1000LL)) {
        free(event);
        goto out;
    }

    fprintf(stderr,
            "stock launcher: host gate armed at %llu ms\n",
            gate_ms);
    ok = 1;

out:
    if (source != NULL) fclose(source);
    if (pre != NULL) fclose(pre);
    if (post != NULL) fclose(post);
    if (!ok) {
        remove(pre_path);
        remove(post_path);
    }
    return ok;
}

int main(int argc, char **argv)
{
    struct state s;
    const char *mode = getenv("HPCLANG_STOCK_MODE");
    const char *sequence = getenv("HPCLANG_E2E_SEQUENCE");
    const char *result_text = getenv("HPCLANG_STOCK_RESULT_ADDR");
    const char *lcd_height_text = getenv("HPCLANG_X50NG_LCD_HEIGHT");

    memset(&s, 0, sizeof(s));
    s.mode = mode && strcmp(mode, "run") == 0 ? MODE_RUN : MODE_INSTALL;
    s.prefix = getenv("HPCLANG_STOCK_PREFIX");
    s.json = getenv("HPCLANG_STOCK_JSON");
    if (!s.prefix) s.prefix = "stock";
    if (!s.json) s.json = "stock.json";
    s.lcd_height = lcd_height_text ? atoi(lcd_height_text) : 80;
    if (s.lcd_height != 64 && s.lcd_height != 80) {
        fprintf(stderr, "invalid stock LCD height\n");
        return EXIT_FAILURE;
    }

    if (s.mode == MODE_RUN) {
        s.before_pgm = getenv("HPCLANG_X50NG_BEFORE_PGM");
        s.pass_pgm = getenv("HPCLANG_X50NG_PASS_PGM");
        s.return_pgm = getenv("HPCLANG_X50NG_RETURN_PGM");
        s.runtime_json = getenv("HPCLANG_X50NG_E2E_JSON");
        if (!parse_address(result_text, &s.result_addr) ||
            !s.before_pgm || !s.pass_pgm || !s.return_pgm ||
            !s.runtime_json) {
            fprintf(stderr, "stock run mode needs canonical runtime outputs\n");
            return EXIT_FAILURE;
        }
    }

    config_t config = hpclang_x50ng_headless_config(argc, argv);
    s.hdw = emulator_init(&config);

    if (!sequence) {
        fprintf(stderr, "missing HPCLANG_E2E_SEQUENCE\n");
        return EXIT_FAILURE;
    }
    if (!hpclang_schedule_sequence_with_host_gate(&s, sequence)) {
        fprintf(stderr, "cannot schedule stock launcher sequence\n");
        return EXIT_FAILURE;
    }

    main_loop(s.hdw);
    save_modules(s.hdw);
    exit_modules(s.hdw);

    return s.failed ||
           (s.mode == MODE_RUN &&
            (!s.saw_pass || !s.returned || !return_is_settled(&s)))
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
