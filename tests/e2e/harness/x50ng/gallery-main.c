#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "emulator_api.h"
#include "for_qemu.h"
#include "hdw.h"
#include "module.h"
#include "options.h"
#include "timer.h"
#include "timed-sequence.h"
#include "key-sequence.h"
#include "direct-boot.h"
#include "ready-window.h"
#include "headless-ui-init.h"
#include "lcd-quiescence.h"
#include "s3c2410/s3c2410_lcd.h"
#include "qemu/cpu-common.h"
#include "qemu/exec-all.h"

#define LCD_W 131

#define PASS_WORD 0x50415353u
#define FAIL_PREFIX 0x46410000u
#define GALLERY_PREFIX 0x47000000u
#define GALLERY_KEY_ACK 0x474b4143u
#define GALLERY_NEXT_FACE 0x00400000u

#define TEST_RAM_SIZE 0x00200000u
#define TEST_PHYS 0x70000000u
#define STAGE_PAGES 224u
#define STAGE_PHYS TEST_PHYS
#define STAGE_SIZE (STAGE_PAGES * 0x1000u)
#define INJECT_L2_PHYS (TEST_PHYS + STAGE_SIZE)
#define INJECT_PHYS (INJECT_L2_PHYS + 0x1000u)
#define INJECT_PAGES 224u
#define INJECT_VA 0x30000000u
#define STAGE0_VA (INJECT_VA + 0x1000u)
#define SYNTH_STACK_TOP (INJECT_VA + (INJECT_PAGES - 2u) * 0x1000u - 16u)
#define SENTINEL_VA (INJECT_VA + (INJECT_PAGES - 1u) * 0x1000u)

#define L1_DATA_VA 0x07f00044u
#define L1_CODE_VA 0x07f00048u
#include "stage-layout.h"
#define L1_STAGE_VA HPCLANG_STAGE_L1_SLOT
#define L1_INJECT_VA 0x07f00c00u
#define SWI_VECTOR_VA 0x08000008u

#define COARSE_FLAGS 0x11u
#define SMALL_FLAGS 0x0ffeu

#define INJECT_US 7000000LL
#define BEFORE_CAPTURE_SETTLE_US 2000000LL
#define POLL_US 20000LL
#define DEADLINE_US 300000000LL
#define KEY_HOLD_US 50000LL
#define RETURN_ON_US 200000LL
#define RETURN_SETTLE_FIRST_US 300000LL
#define RETURN_SETTLE_INTERVAL_US 50000LL
#define RETURN_SETTLE_TIMEOUT_US 2000000LL
#define RETURN_SETTLE_STABLE 4u

struct cpu_snapshot
{
    uint32_t regs[16];
    uint32_t cpsr;
    uint32_t spsr;
    uint32_t banked_spsr[6];
    uint32_t banked_r13[6];
    uint32_t banked_r14[6];
    uint32_t usr_regs[5];
    uint32_t fiq_regs[5];
};

enum gallery_phase
{
    GALLERY_WAIT_INITIAL,
    GALLERY_WAIT_BODY,
    GALLERY_WAIT_SAMPLE_RETURN,
    GALLERY_WAIT_NEXT,
    GALLERY_WAIT_PASS,
};

struct state
{
    hdw_t *hdw;
    struct hpclang_timed_sequence return_keys;
    struct hpclang_timed_sequence boot;
    struct hpclang_ready_window before_ready;
    uint8_t *stage0;
    size_t stage0_size;
    uint32_t result_addr;
    const char *before_pgm;
    const char *pass_pgm;
    const char *return_pgm;
    const char *gallery_dir;
    const char *json;
    int lcd_height;

    struct cpu_snapshot stock;
    uint32_t old_data;
    uint32_t old_code;
    uint32_t old_stage;
    uint32_t old_inject;
    uint32_t old_swi;

    enum gallery_phase gallery_phase;
    uint32_t gallery_count;
    uint32_t gallery_index;
    uint32_t gallery_frames;
    int gallery_started;
    int key_busy;
    int gallery_key;

    struct hpclang_lcd_quiescence return_quiescence;
    int64_t return_settle_deadline;
    uint32_t result;
    int saw_pass;
    int returned;
    int return_settled;
    int failed;
    int64_t deadline;
};

struct key_action
{
    int key;
    int down;
};
static const struct key_action f_down = {HP4950_KEY_F, 1};

static const struct key_action on_down = {HP4950_KEY_ON, 1};
static const struct key_action on_up = {HP4950_KEY_ON, 0};

static int later(timer_callback_t fn, void *arg, int64_t us)
{
    hdw_timer_t *t = timer_new(HDW_TIMER_REALTIME, fn, arg);
    if (t == NULL)
        return 0;
    timer_mod(t, timer_get_clock() + us);
    return 1;
}

static void key_cb(void *arg)
{
    const struct key_action *a = arg;
    if (a->down)
        press_key(a->key);
    else
        release_key(a->key);
}

static int64_t timed_key_cb(void *arg)
{
    key_cb(arg);
    return 0;
}

static int tap(struct state *s, const struct key_action *down, const struct key_action *up,
               int64_t us)
{
    return hpclang_event_add(&s->return_keys.events, us, timed_key_cb, (void *)down) &&
           hpclang_event_add(&s->return_keys.events, us + KEY_HOLD_US, timed_key_cb, (void *)up) &&
           hpclang_sequence_start(&s->return_keys);
}

static int gallery_tap(struct state *s, int key)
{
    if (s->key_busy)
        return 0;
    press_key(key);
    s->gallery_key = key;
    s->key_busy = 1;
    return 1;
}

static int guest_read32(hdw_t *h, uint32_t va, uint32_t *v)
{
    uint8_t b[4];
    if (cpu_memory_rw_debug(h->cpu, va, b, 4, 0) != 0)
        return 0;
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 1;
}

static int guest_write32(hdw_t *h, uint32_t va, uint32_t v)
{
    uint8_t b[4] = {
        (uint8_t)v,
        (uint8_t)(v >> 8),
        (uint8_t)(v >> 16),
        (uint8_t)(v >> 24),
    };
    return cpu_memory_rw_debug(h->cpu, va, b, 4, 1) == 0;
}

static unsigned capture_lcd(hdw_t *h, const char *path, int height)
{
    FILE *f = fopen(path, "wb");
    unsigned nonzero = 0;
    if (f == NULL)
        return 0;

    fprintf(f, "P5\n%d %d\n255\n", LCD_W, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < LCD_W; ++x) {
            int level = s3c2410_get_pixel_color(h->s3c2410_lcd, x, y);
            if (level < 0)
                level = 0;
            if (level > 15)
                level = 15;
            if (level)
                ++nonzero;
            uint8_t pixel = (uint8_t)(255 - 17 * level);
            fwrite(&pixel, 1, 1, f);
        }
    }
    fclose(f);
    return nonzero;
}

static int capture_gallery(struct state *s, uint32_t index, uint32_t page)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%04u-%s.pgm", s->gallery_dir, index,
             page == 0u ? "sample" : "text");
    if (!capture_lcd(s->hdw, path, s->lcd_height))
        return 0;
    ++s->gallery_frames;
    return 1;
}

static void snapshot_cpu(struct cpu_snapshot *s, CPUARMState *c)
{
    memcpy(s->regs, c->regs, sizeof(s->regs));
    s->cpsr = cpsr_read(c);
    s->spsr = c->spsr;
    memcpy(s->banked_spsr, c->banked_spsr, sizeof(s->banked_spsr));
    memcpy(s->banked_r13, c->banked_r13, sizeof(s->banked_r13));
    memcpy(s->banked_r14, c->banked_r14, sizeof(s->banked_r14));
    memcpy(s->usr_regs, c->usr_regs, sizeof(s->usr_regs));
    memcpy(s->fiq_regs, c->fiq_regs, sizeof(s->fiq_regs));
}

static void restore_cpu(const struct cpu_snapshot *s, CPUARMState *c)
{
    cpsr_write(c, s->cpsr, 0xffffffffu);
    memcpy(c->banked_spsr, s->banked_spsr, sizeof(s->banked_spsr));
    memcpy(c->banked_r13, s->banked_r13, sizeof(s->banked_r13));
    memcpy(c->banked_r14, s->banked_r14, sizeof(s->banked_r14));
    memcpy(c->usr_regs, s->usr_regs, sizeof(s->usr_regs));
    memcpy(c->fiq_regs, s->fiq_regs, sizeof(s->fiq_regs));
    memcpy(c->regs, s->regs, sizeof(s->regs));
    c->spsr = s->spsr;
}

static uint8_t *load_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if (n <= 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = malloc((size_t)n);
    if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return buf;
}

static int map_test_ram(struct state *s)
{
    if (phys_ram_size < (int)TEST_RAM_SIZE)
        return 0;
    uint32_t off = (uint32_t)phys_ram_size - TEST_RAM_SIZE;
    if (off & 0xfffu)
        return 0;
    void *p = mmap(phys_ram_base + off, TEST_RAM_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED)
        return 0;
    memset(p, 0, TEST_RAM_SIZE);
    cpu_register_physical_memory(TEST_PHYS, TEST_RAM_SIZE, (ram_addr_t)off | IO_MEM_RAM);
    tb_flush(s->hdw->cpu);
    tlb_flush(s->hdw->cpu, 1);
    return 1;
}

static int build_launcher_ram(struct state *s)
{
    const uint32_t stage0_phys = INJECT_PHYS + 0x1000u;
    const uint32_t sentinel_phys = INJECT_PHYS + (INJECT_PAGES - 1u) * 0x1000u;
    const size_t max_stage0 = (INJECT_PAGES - 3u) * 0x1000u;
    if (s->stage0_size > max_stage0)
        return 0;

    for (uint32_t i = 0; i < 256u; ++i) {
        uint32_t d = i < STAGE_PAGES ? (STAGE_PHYS + i * 0x1000u) | SMALL_FLAGS : 0;
        stl_phys(STAGE_PHYS + i * 4u, d);
    }
    for (uint32_t i = 0; i < 256u; ++i) {
        uint32_t d = i < INJECT_PAGES ? (INJECT_PHYS + i * 0x1000u) | SMALL_FLAGS : 0;
        stl_phys(INJECT_L2_PHYS + i * 4u, d);
    }

    stl_phys(INJECT_PHYS + 0x914u, STAGE_PAGES);
    stl_phys(INJECT_PHYS + 0x918u, 0);
    stl_phys(INJECT_PHYS + 0x920u, STAGE_PHYS);
    cpu_physical_memory_write(stage0_phys, s->stage0, (int)s->stage0_size);
    stl_phys(sentinel_phys, 0xeafffffeu);
    tb_flush(s->hdw->cpu);
    return 1;
}

static int snapshot_stock(struct state *s)
{
    if (!guest_read32(s->hdw, L1_DATA_VA, &s->old_data) ||
        !guest_read32(s->hdw, L1_CODE_VA, &s->old_code) ||
        !guest_read32(s->hdw, L1_STAGE_VA, &s->old_stage) ||
        !guest_read32(s->hdw, L1_INJECT_VA, &s->old_inject) ||
        !guest_read32(s->hdw, SWI_VECTOR_VA, &s->old_swi))
        return 0;
    if (s->old_inject != 0u)
        return 0;
    snapshot_cpu(&s->stock, s->hdw->cpu);
    return 1;
}

static void write_json(struct state *s, const char *status)
{
    FILE *f = fopen(s->json, "w");
    if (f == NULL)
        return;
    fprintf(f,
            "{\n"
            "  \"status\":\"%s\",\n"
            "  \"result\":\"0x%08x\",\n"
            "  \"gallery_count\":%u,\n"
            "  \"gallery_frames\":%u,\n"
            "  \"saw_pass\":%s,\n"
            "  \"returned\":%s,\n"
            "  \"return_settled\":%s,\n"
            "  \"return_settle_observations\":%u,\n"
            "  \"pc\":\"0x%08x\"\n"
            "}\n",
            status, s->result, s->gallery_count, s->gallery_frames, s->saw_pass ? "true" : "false",
            s->returned ? "true" : "false", s->return_settled ? "true" : "false",
            s->return_quiescence.observations, s->hdw->cpu->regs[15]);
    fclose(f);
}

static void fail(struct state *s, const char *why)
{
    s->failed = 1;
    const char *failure_pgm = getenv("HPCLANG_X50NG_FAILURE_PGM");
    if (failure_pgm)
        capture_lcd(s->hdw, failure_pgm, s->lcd_height);
    fprintf(stderr,
            "Font Gallery E2E FAIL: %s (pc=0x%08x lr=0x%08x "
            "dfsr=0x%08x dfar=0x%08x)\n",
            why, s->hdw->cpu->regs[15], s->hdw->cpu->regs[14], s->hdw->cpu->cp15.c5_data,
            s->hdw->cpu->cp15.c6_data);
    write_json(s, why);
    hdw_stop(s->hdw);
}

static int restore_stock(struct state *s)
{
    uint32_t data, code, stage, swi;
    if (!guest_read32(s->hdw, L1_DATA_VA, &data) || !guest_read32(s->hdw, L1_CODE_VA, &code) ||
        !guest_read32(s->hdw, L1_STAGE_VA, &stage) || !guest_read32(s->hdw, SWI_VECTOR_VA, &swi))
        return 0;
    if (data != s->old_data || code != s->old_code || stage != s->old_stage || swi != s->old_swi)
        return 0;
    if (!guest_write32(s->hdw, L1_INJECT_VA, s->old_inject))
        return 0;

    tlb_flush(s->hdw->cpu, 1);
    tb_flush(s->hdw->cpu);
    restore_cpu(&s->stock, s->hdw->cpu);
    hdw_set_idle(s->hdw, HDW_ARM_RUN);
    tlb_flush(s->hdw->cpu, 1);
    tb_flush(s->hdw->cpu);
    s->returned = 1;
    return 1;
}

static void settle_return(void *arg)
{
    struct state *s = arg;

    if (timer_get_clock() >= s->return_settle_deadline) {
        fail(s, "return-not-settled");
        return;
    }

    if (is_display_on() && hpclang_lcd_quiescence_observe(&s->return_quiescence, s->hdw, LCD_W,
                                                          s->lcd_height, RETURN_SETTLE_STABLE)) {
        if (!capture_lcd(s->hdw, s->return_pgm, s->lcd_height) ||
            s->hdw->cpu->regs[15] == SENTINEL_VA) {
            fail(s, "stock-resume");
            return;
        }
        s->return_settled = 1;
        write_json(s, "pass");
        hdw_stop(s->hdw);
        return;
    }

    if (!later(settle_return, s, RETURN_SETTLE_INTERVAL_US))
        fail(s, "return-settle-timer");
}

static int drive_gallery(struct state *s, uint32_t value)
{
    uint32_t page = (value >> 23) & 1u;
    uint32_t count = (value >> 12) & 0x3ffu;
    uint32_t index = value & 0xfffu;
    int next_same_family = (value & GALLERY_NEXT_FACE) != 0u;

    if (count == 0u || index >= count)
        return 0;
    if (s->key_busy)
        return 1;

    if (!s->gallery_started) {
        if (index != 0u || page != 0u)
            return 1;
        s->gallery_started = 1;
        s->gallery_count = count;
        s->gallery_index = 0u;
        s->gallery_phase = GALLERY_WAIT_BODY;
        if (!capture_gallery(s, index, page) || !gallery_tap(s, HP4950_KEY_ENTER))
            return 0;
        return 1;
    }

    if (count != s->gallery_count)
        return 0;

    switch (s->gallery_phase) {
    case GALLERY_WAIT_BODY:
        if (index == s->gallery_index && page == 1u) {
            if (!capture_gallery(s, index, page) || !gallery_tap(s, HP4950_KEY_ENTER))
                return 0;
            s->gallery_phase = GALLERY_WAIT_SAMPLE_RETURN;
        }
        break;

    case GALLERY_WAIT_SAMPLE_RETURN:
        if (index == s->gallery_index && page == 0u) {
            if (s->gallery_index + 1u < s->gallery_count) {
                int key = next_same_family ? HP4950_KEY_DOWN : HP4950_KEY_RIGHT;
                if (!gallery_tap(s, key))
                    return 0;
                s->gallery_phase = GALLERY_WAIT_NEXT;
            } else {
                if (!gallery_tap(s, HP4950_KEY_ON))
                    return 0;
                s->gallery_phase = GALLERY_WAIT_PASS;
            }
        }
        break;

    case GALLERY_WAIT_NEXT:
        if (index == s->gallery_index + 1u && page == 0u) {
            s->gallery_index = index;
            if (!capture_gallery(s, index, page) || !gallery_tap(s, HP4950_KEY_ENTER))
                return 0;
            s->gallery_phase = GALLERY_WAIT_BODY;
        }
        break;

    case GALLERY_WAIT_PASS:
    case GALLERY_WAIT_INITIAL:
        break;
    }
    return 1;
}

static void monitor(void *arg)
{
    struct state *s = arg;
    uint32_t v = 0;

    if (timer_get_clock() >= s->deadline) {
        fail(s, "timeout");
        return;
    }

    if (!s->saw_pass && s->hdw->cpu->regs[15] == 0x00000010u) {
        fail(s, "data-abort");
        return;
    }

    if (guest_read32(s->hdw, s->result_addr, &v)) {
        if (!s->saw_pass)
            s->result = v;
        if (v == GALLERY_KEY_ACK && !s->saw_pass) {
            if (s->key_busy) {
                release_key(s->gallery_key);
                s->key_busy = 0;
            }
        } else if ((v & 0xff000000u) == GALLERY_PREFIX && !s->saw_pass) {
            if (!drive_gallery(s, v)) {
                fail(s, "gallery-drive");
                return;
            }
        } else if (!s->saw_pass && v == PASS_WORD) {
            s->result = v;
            s->saw_pass = 1;
            if (!capture_lcd(s->hdw, s->pass_pgm, s->lcd_height)) {
                fail(s, "pass-capture");
                return;
            }
            hpclang_lcd_quiescence_seed(&s->return_quiescence, s->hdw, LCD_W, s->lcd_height);
            fprintf(stderr, "Font Gallery PASS after %u faces / %u captures\n", s->gallery_count,
                    s->gallery_frames);
        } else if ((v & 0xffff0000u) == FAIL_PREFIX) {
            s->result = v;
            fail(s, "runtime-fail");
            return;
        }
    }

    if (s->saw_pass && s->hdw->cpu->regs[15] == SENTINEL_VA) {
        if (!restore_stock(s)) {
            fail(s, "restore");
            return;
        }
        s->return_settle_deadline = timer_get_clock() + RETURN_SETTLE_TIMEOUT_US;
        if (!tap(s, &on_down, &on_up, RETURN_ON_US) ||
            !later(settle_return, s, RETURN_SETTLE_FIRST_US)) {
            fail(s, "return-timer");
        }
        return;
    }

    if (!later(monitor, s, POLL_US))
        fail(s, "monitor-timer");
}

static int64_t inject(void *arg)
{
    struct state *s = arg;
    CPUARMState *cpu = s->hdw->cpu;

    int ready = hpclang_ready_observe(&s->before_ready, timer_get_clock(), BEFORE_CAPTURE_SETTLE_US,
                                      capture_lcd(s->hdw, s->before_pgm, s->lcd_height) != 0);
    if (ready == 0)
        return POLL_US;
    if (ready < 0) {
        fail(s, "before-capture");
        return -1;
    }
    if (!map_test_ram(s) || !build_launcher_ram(s) || !snapshot_stock(s)) {
        fail(s, "prepare");
        return -1;
    }
    if (!guest_write32(s->hdw, L1_INJECT_VA, INJECT_L2_PHYS | COARSE_FLAGS)) {
        fail(s, "inject-l1");
        return -1;
    }
    tlb_flush(cpu, 1);
    tb_flush(cpu);

    if (cpu_get_phys_page_debug(cpu, STAGE0_VA) != (target_phys_addr_t)(INJECT_PHYS + 0x1000u)) {
        fail(s, "inject-map");
        return -1;
    }

    uint32_t launch_cpsr =
        (s->stock.cpsr & (CPSR_NZCV | CPSR_Q | CPSR_GE)) | ARM_CPU_MODE_SYS | CPSR_I | CPSR_F;
    cpsr_write(cpu, launch_cpsr, 0xffffffffu);
    cpu->regs[13] = SYNTH_STACK_TOP;
    cpu->regs[1] = INJECT_VA;
    cpu->regs[14] = SENTINEL_VA;
    cpu->regs[15] = STAGE0_VA;

    s->deadline = timer_get_clock() + DEADLINE_US;
    tb_flush(cpu);
    tlb_flush(cpu, 1);
    hdw_set_idle(s->hdw, HDW_ARM_RUN);

    if (!later(monitor, s, POLL_US))
        fail(s, "monitor-start");
    return 0;
}

static void boot_input_failure(void *context, const char *reason)
{
    fail(context, reason);
}

int main(int argc, char **argv)
{
    const char *stage0_path = getenv("HPCLANG_X50NG_STAGE0");
    const char *result_text = getenv("HPCLANG_X50NG_RESULT_ADDR");
    const char *lcd_height_text = getenv("HPCLANG_X50NG_LCD_HEIGHT");
    struct state s;
    char *end = NULL;

    if (stage0_path == NULL || result_text == NULL) {
        fprintf(stderr, "missing Gallery stage0/result configuration\n");
        return EXIT_FAILURE;
    }

    memset(&s, 0, sizeof(s));
    unsigned long result = strtoul(result_text, &end, 0);
    if (end == result_text || *end != '\0' || result > 0xfffffffful) {
        fprintf(stderr, "invalid result address\n");
        return EXIT_FAILURE;
    }
    s.result_addr = (uint32_t)result;
    s.lcd_height = lcd_height_text ? atoi(lcd_height_text) : 80;
    if (s.lcd_height != 64 && s.lcd_height != 80)
        return EXIT_FAILURE;

    s.stage0 = load_file(stage0_path, &s.stage0_size);
    if (s.stage0 == NULL)
        return EXIT_FAILURE;

    s.before_pgm = getenv("HPCLANG_X50NG_BEFORE_PGM");
    s.pass_pgm = getenv("HPCLANG_X50NG_PASS_PGM");
    s.return_pgm = getenv("HPCLANG_X50NG_RETURN_PGM");
    s.gallery_dir = getenv("HPCLANG_X50NG_GALLERY_DIR");
    s.json = getenv("HPCLANG_X50NG_E2E_JSON");
    if (!s.before_pgm)
        s.before_pgm = "before.pgm";
    if (!s.pass_pgm)
        s.pass_pgm = "pass.pgm";
    if (!s.return_pgm)
        s.return_pgm = "return.pgm";
    if (!s.gallery_dir)
        s.gallery_dir = ".";
    if (!s.json)
        s.json = "e2e.json";

    config_t config = hpclang_x50ng_headless_config(argc, argv);
    s.hdw = emulator_init(&config);

    if (!hpclang_direct_boot(&s.boot.events, s.hdw, s.lcd_height, f_down.key, boot_input_failure,
                             &s) ||
        !hpclang_event_add(&s.boot.events, INJECT_US, inject, &s) ||
        !hpclang_sequence_start(&s.boot)) {
        fprintf(stderr, "cannot schedule boot/Gallery actions\n");
        return EXIT_FAILURE;
    }

    main_loop(s.hdw);
    exit_modules(s.hdw);
    free(s.stage0);
    return (s.failed || !s.saw_pass || !s.returned || !s.return_settled) ? EXIT_FAILURE
                                                                         : EXIT_SUCCESS;
}
