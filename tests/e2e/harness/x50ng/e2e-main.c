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
#include "headless-ui-init.h"
#include "lcd-quiescence.h"
#include "s3c2410/s3c2410_lcd.h"
#include "qemu/cpu-common.h"
#include "qemu/exec-all.h"

#define LCD_W 131

#define PASS_WORD 0x50415353u
#define FAIL_PREFIX 0x46410000u
#define PHASE_KEYBOARD_READY 0x4b425244u
#define PHASE_KEYBOARD_ACK 0x4b410000u

#define TEST_RAM_SIZE 0x00100000u
#define TEST_PHYS 0x70000000u

#define STAGE_PAGES 96u
#define STAGE_PHYS TEST_PHYS
#define STAGE_SIZE (STAGE_PAGES * 0x1000u)

#define INJECT_L2_PHYS (TEST_PHYS + STAGE_SIZE)
#define INJECT_PHYS (INJECT_L2_PHYS + 0x1000u)
#define INJECT_PAGES 64u
#define INJECT_VA 0x30000000u
#define STAGE0_VA (INJECT_VA + 0x1000u)
#define SYNTH_STACK_TOP (INJECT_VA + 0x3f000u - 16u)
#define SENTINEL_VA (INJECT_VA + 0x3f000u)

#define L1_DATA_VA 0x07f00044u
#define L1_CODE_VA 0x07f00048u
#define L1_STAGE_VA 0x07f00240u
#define L1_INJECT_VA 0x07f00c00u
#define SWI_VECTOR_VA 0x08000008u

#define POWER_MPLLCON_VA 0x07200004u
#define POWER_CLKSLOW_VA 0x07200010u
#define POWER_CLKDIVN_VA 0x07200014u
#define TIMER_TCFG0_VA   0x07600000u
#define TIMER_TCFG1_VA   0x07600004u
#define TIMER_TCON_VA    0x07600008u
#define TIMER_TCNTB0_VA  0x0760000cu

#define COARSE_FLAGS 0x11u
#define SMALL_FLAGS 0x0ffeu

#define BOOT_NO_US 2000000LL
#define BOOT_OK_US 4000000LL
#define INJECT_US 7000000LL
#define KEY_HOLD_US 50000LL
#define KEYBOARD_FIRST_DELAY_US 20000LL
#define POLL_US 20000LL
#define DEADLINE_US 16000000LL
#define RETURN_ON_US 200000LL
#define RETURN_POLL_US 50000LL
#define RETURN_DEADLINE_US 2000000LL
#define RETURN_STABLE_OBSERVATIONS 4u

struct key_action {
    int key;
    int down;
};

struct cpu_snapshot {
    uint32_t regs[16];
    uint32_t cpsr;
    uint32_t spsr;
    uint32_t banked_spsr[6];
    uint32_t banked_r13[6];
    uint32_t banked_r14[6];
    uint32_t usr_regs[5];
    uint32_t fiq_regs[5];
};

/*
 * Stable LCD controller configuration that a stock-hosted HPC1 application
 * must leave untouched. Interrupt pending/source registers are deliberately
 * excluded because they are dynamic status rather than configuration.
 */
struct lcd_snapshot {
    uint32_t lcdcon1;
    uint32_t lcdcon2;
    uint32_t lcdcon3;
    uint32_t lcdcon4;
    uint32_t lcdcon5;
    uint32_t lcdsaddr1;
    uint32_t lcdsaddr2;
    uint32_t lcdsaddr3;
    uint32_t redlut;
    uint32_t greenlut;
    uint32_t bluelut;
    uint32_t dithmode;
    uint32_t tpal;
    uint32_t lcdintmsk;
    uint32_t lpcsel;
};

struct e2e {
    hdw_t *hdw;
    uint8_t *stage0;
    size_t stage0_size;
    uint32_t result_addr;

    const char *before_pgm;
    const char *pass_pgm;
    const char *return_pgm;
    const char *json;

    struct cpu_snapshot stock;
    struct lcd_snapshot stock_lcd;
    struct hpclang_lcd_quiescence return_quiescence;
    uint32_t lcd_state_diff;
    uint32_t old_data;
    uint32_t old_code;
    uint32_t old_stage;
    uint32_t old_inject;
    uint32_t old_swi;

    uint32_t stock_mpllcon;
    uint32_t stock_clkslow;
    uint32_t stock_clkdivn;
    uint32_t stock_tcfg0;
    uint32_t stock_tcfg1;
    uint32_t stock_tcon;
    uint32_t stock_tcntb0;

    uint32_t result;
    int saw_pass;
    int payload_keyboard_scheduled;
    size_t payload_keyboard_sent;
    size_t payload_keyboard_captured;
    int returned;
    int failed;
    int lcd_height;
    int64_t deadline;
    int64_t return_deadline;
};

static const struct key_action f_down = { HP4950_KEY_F, 1 };
static const struct key_action f_up = { HP4950_KEY_F, 0 };
static const struct key_action on_down = { HP4950_KEY_ON, 1 };
static const struct key_action on_up = { HP4950_KEY_ON, 0 };

static const int keyboard_codes[] = {
    HP4950_KEY_BACKSPACE,
    HP4950_KEY_U,
    HP4950_KEY_Z,
    HP4950_KEY_MULTIPLY,
    HP4950_KEY_MINUS,
    HP4950_KEY_PLUS,
    HP4950_KEY_ENTER,
    HP4950_KEY_P,
    HP4950_KEY_T,
    HP4950_KEY_Y,
    HP4950_KEY_9,
    HP4950_KEY_6,
    HP4950_KEY_3,
    HP4950_KEY_SPACE,
    HP4950_KEY_O,
    HP4950_KEY_S,
    HP4950_KEY_X,
    HP4950_KEY_8,
    HP4950_KEY_5,
    HP4950_KEY_2,
    HP4950_KEY_PERIOD,
    HP4950_KEY_N,
    HP4950_KEY_R,
    HP4950_KEY_W,
    HP4950_KEY_7,
    HP4950_KEY_4,
    HP4950_KEY_1,
    HP4950_KEY_0,
    HP4950_KEY_M,
    HP4950_KEY_Q,
    HP4950_KEY_V,
    HP4950_KEY_A,
    HP4950_KEY_B,
    HP4950_KEY_C,
    HP4950_KEY_D,
    HP4950_KEY_E,
    HP4950_KEY_F,
    HP4950_KEY_G,
    HP4950_KEY_UP,
    HP4950_KEY_LEFT,
    HP4950_KEY_DOWN,
    HP4950_KEY_RIGHT,
    HP4950_KEY_H,
    HP4950_KEY_I,
    HP4950_KEY_J,
    HP4950_KEY_K,
    HP4950_KEY_L,
    HP4950_KEY_ON,
    HP4950_KEY_ALPHA,
    HP4950_KEY_LEFTSHIFT,
    HP4950_KEY_RIGHTSHIFT,
};

static struct key_action keyboard_down[
    sizeof(keyboard_codes) / sizeof(keyboard_codes[0])];
static struct key_action keyboard_up[
    sizeof(keyboard_codes) / sizeof(keyboard_codes[0])];

static int guest_read32(hdw_t *h, uint32_t va, uint32_t *v)
{
    uint8_t b[4];
    if (cpu_memory_rw_debug(h->cpu, va, b, 4, 0) != 0)
        return 0;
    *v = (uint32_t)b[0] |
         ((uint32_t)b[1] << 8) |
         ((uint32_t)b[2] << 16) |
         ((uint32_t)b[3] << 24);
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
            uint8_t pixel;
            if (level < 0) level = 0;
            if (level > 15) level = 15;
            if (level) ++nonzero;
            pixel = (uint8_t)(255 - 17 * level);
            fwrite(&pixel, 1, 1, f);
        }
    }
    fclose(f);
    return nonzero;
}

static int capture_keyboard_progress(struct e2e *s, size_t index)
{
    const char *dir = getenv("HPCLANG_X50NG_KEYBOARD_DIR");
    char path[512];

    if (dir == NULL || *dir == '\0')
        return 1;
    snprintf(path, sizeof(path), "%s/%02zu.pgm", dir, index);
    if (!capture_lcd(s->hdw, path, s->lcd_height)) {
        fprintf(stderr,
                "blank keyboard progress capture %zu\n", index);
        return 0;
    }
    return 1;
}

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
    if (a->down) press_key(a->key);
    else release_key(a->key);
}

static int tap(
    const struct key_action *down,
    const struct key_action *up,
    int64_t us)
{
    return later(key_cb, (void *)down, us) &&
           later(key_cb, (void *)up, us + KEY_HOLD_US);
}

static int schedule_payload_keyboard_key(
    struct e2e *s,
    size_t index)
{
    size_t count = sizeof(keyboard_codes) / sizeof(keyboard_codes[0]);
    if (index >= count)
        return 0;

    keyboard_down[index].key = keyboard_codes[index];
    keyboard_down[index].down = 1;
    keyboard_up[index].key = keyboard_codes[index];
    keyboard_up[index].down = 0;

    if (!later(
            key_cb, &keyboard_down[index],
            KEYBOARD_FIRST_DELAY_US) ||
        !later(
            key_cb, &keyboard_up[index],
            KEYBOARD_FIRST_DELAY_US + KEY_HOLD_US))
        return 0;

    s->payload_keyboard_sent = index + 1u;
    return 1;
}

static int schedule_payload_keyboard(struct e2e *s)
{
    size_t count = sizeof(keyboard_codes) / sizeof(keyboard_codes[0]);

    if (s->payload_keyboard_scheduled)
        return 1;
    if (count == 0u || !schedule_payload_keyboard_key(s, 0u))
        return 0;

    s->payload_keyboard_scheduled = 1;
    fprintf(stderr,
            "HPC1 keyboard phase reached; started %zu-key "
            "acknowledged sequence\n",
            count);
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

    if (f == NULL) return NULL;
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

static int map_test_ram(struct e2e *s)
{
    uint32_t off;
    void *p;

    if (phys_ram_size < (int)TEST_RAM_SIZE)
        return 0;
    off = (uint32_t)phys_ram_size - TEST_RAM_SIZE;
    if (off & 0xfffu)
        return 0;

    p = mmap(
        phys_ram_base + off,
        TEST_RAM_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
        -1,
        0);
    if (p == MAP_FAILED)
        return 0;

    memset(p, 0, TEST_RAM_SIZE);
    cpu_register_physical_memory(
        TEST_PHYS, TEST_RAM_SIZE, (ram_addr_t)off | IO_MEM_RAM);
    tb_flush(s->hdw->cpu);
    tlb_flush(s->hdw->cpu, 1);
    return 1;
}

static int build_launcher_ram(struct e2e *s)
{
    const uint32_t stage0_phys = INJECT_PHYS + 0x1000u;
    const uint32_t sentinel_phys =
        INJECT_PHYS + (INJECT_PAGES - 1u) * 0x1000u;
    /* Reserve page 62 for the synthetic outer-launcher stack and page 63
     * for the return sentinel. */
    const size_t max_stage0 = (INJECT_PAGES - 3u) * 0x1000u;

    if (s->stage0_size > max_stage0)
        return 0;

    for (uint32_t i = 0; i < 256u; ++i) {
        uint32_t d = i < STAGE_PAGES
            ? (STAGE_PHYS + i * 0x1000u) | SMALL_FLAGS
            : 0;
        stl_phys(STAGE_PHYS + i * 4u, d);
    }

    for (uint32_t i = 0; i < 256u; ++i) {
        uint32_t d = i < INJECT_PAGES
            ? (INJECT_PHYS + i * 0x1000u) | SMALL_FLAGS
            : 0;
        stl_phys(INJECT_L2_PHYS + i * 4u, d);
    }

    stl_phys(INJECT_PHYS + 0x914u, STAGE_PAGES);
    stl_phys(INJECT_PHYS + 0x918u, 0);
    stl_phys(INJECT_PHYS + 0x920u, STAGE_PHYS);

    cpu_physical_memory_write(
        stage0_phys, s->stage0, (int)s->stage0_size);
    stl_phys(sentinel_phys, 0xeafffffeu);
    tb_flush(s->hdw->cpu);
    return 1;
}

static uint32_t lcd_framebuffer_phys(struct e2e *s)
{
    uint32_t v = ((s3c2410_lcd_t *)s->hdw->s3c2410_lcd)->lcdsaddr1;
    return ((v << 1) & 0x7fc00000u) |
           ((v << 1) & 0x003ffffeu);
}

static void snapshot_lcd(
    const s3c2410_lcd_t *lcd,
    struct lcd_snapshot *snapshot)
{
    snapshot->lcdcon1 = lcd->lcdcon1;
    snapshot->lcdcon2 = lcd->lcdcon2;
    snapshot->lcdcon3 = lcd->lcdcon3;
    snapshot->lcdcon4 = lcd->lcdcon4;
    snapshot->lcdcon5 = lcd->lcdcon5;
    snapshot->lcdsaddr1 = lcd->lcdsaddr1;
    snapshot->lcdsaddr2 = lcd->lcdsaddr2;
    snapshot->lcdsaddr3 = lcd->lcdsaddr3;
    snapshot->redlut = lcd->redlut;
    snapshot->greenlut = lcd->greenlut;
    snapshot->bluelut = lcd->bluelut;
    snapshot->dithmode = lcd->dithmode;
    snapshot->tpal = lcd->tpal;
    snapshot->lcdintmsk = lcd->lcdintmsk;
    snapshot->lpcsel = lcd->lpcsel;
}

static uint32_t lcd_diff_mask(
    const struct lcd_snapshot *expected,
    const struct lcd_snapshot *actual)
{
    uint32_t mask = 0u;

#define LCD_DIFF(bit, field) \
    do { \
        if (expected->field != actual->field) \
            mask |= UINT32_C(1) << (bit); \
    } while (0)

    LCD_DIFF(0, lcdcon1);
    LCD_DIFF(1, lcdcon2);
    LCD_DIFF(2, lcdcon3);
    LCD_DIFF(3, lcdcon4);
    LCD_DIFF(4, lcdcon5);
    LCD_DIFF(5, lcdsaddr1);
    LCD_DIFF(6, lcdsaddr2);
    LCD_DIFF(7, lcdsaddr3);
    LCD_DIFF(8, redlut);
    LCD_DIFF(9, greenlut);
    LCD_DIFF(10, bluelut);
    LCD_DIFF(11, dithmode);
    LCD_DIFF(12, tpal);
    LCD_DIFF(13, lcdintmsk);
    LCD_DIFF(14, lpcsel);

#undef LCD_DIFF

    return mask;
}

static uint32_t check_lcd_state(struct e2e *s)
{
    struct lcd_snapshot current;
    snapshot_lcd(
        (const s3c2410_lcd_t *)s->hdw->s3c2410_lcd,
        &current);

    uint32_t diff = lcd_diff_mask(&s->stock_lcd, &current);
    if (diff != 0u) {
        fprintf(stderr,
                "LCD configuration changed before stock restore: 0x%08x\n",
                diff);
    }
    return diff;
}

static int snapshot_stock(struct e2e *s)
{
    if (!guest_read32(s->hdw, L1_DATA_VA, &s->old_data) ||
        !guest_read32(s->hdw, L1_CODE_VA, &s->old_code) ||
        !guest_read32(s->hdw, L1_STAGE_VA, &s->old_stage) ||
        !guest_read32(s->hdw, L1_INJECT_VA, &s->old_inject) ||
        !guest_read32(s->hdw, SWI_VECTOR_VA, &s->old_swi))
        return 0;

    /*
     * Read-only clock/timer evidence for M7 time-service design. These values
     * do not affect PASS and are never written by the host frontend.
     */
    (void)guest_read32(s->hdw, POWER_MPLLCON_VA, &s->stock_mpllcon);
    (void)guest_read32(s->hdw, POWER_CLKSLOW_VA, &s->stock_clkslow);
    (void)guest_read32(s->hdw, POWER_CLKDIVN_VA, &s->stock_clkdivn);
    (void)guest_read32(s->hdw, TIMER_TCFG0_VA, &s->stock_tcfg0);
    (void)guest_read32(s->hdw, TIMER_TCFG1_VA, &s->stock_tcfg1);
    (void)guest_read32(s->hdw, TIMER_TCON_VA, &s->stock_tcon);
    (void)guest_read32(s->hdw, TIMER_TCNTB0_VA, &s->stock_tcntb0);

    /*
     * HP's stock 49/50 layout stores the LCD framebuffer in the same 4 KiB
     * S3C2410 SRAM that backs the low part of the L1 table. Never choose a
     * synthetic descriptor whose four bytes overlap the live framebuffer.
     */
    uint32_t l1_phys = s->hdw->cpu->cp15.c2_base0 & 0xffffc000u;
    uint32_t inject_desc_phys = l1_phys + 0x0c00u;
    uint32_t fb_phys = lcd_framebuffer_phys(s);
    uint32_t fb_bytes = 160u * (uint32_t)s->lcd_height / 8u;

    if (s->old_inject != 0u) {
        fprintf(stderr, "synthetic L1 slot 0x300 is not empty: %08x\n",
                s->old_inject);
        return 0;
    }

    if (inject_desc_phys + 4u > fb_phys &&
        inject_desc_phys < fb_phys + fb_bytes) {
        fprintf(stderr,
                "synthetic L1 descriptor overlaps LCD framebuffer: "
                "desc=%08x fb=%08x..%08x\n",
                inject_desc_phys, fb_phys, fb_phys + fb_bytes);
        return 0;
    }

    snapshot_cpu(&s->stock, s->hdw->cpu);
    snapshot_lcd(
        (const s3c2410_lcd_t *)s->hdw->s3c2410_lcd,
        &s->stock_lcd);
    s->lcd_state_diff = 0u;
    return 1;
}

static int return_is_settled(const struct e2e *s)
{
    return s->return_quiescence.changed_from_baseline &&
           s->return_quiescence.stable_observations >=
               RETURN_STABLE_OBSERVATIONS;
}

static void json(struct e2e *s, const char *status)
{
    FILE *f = fopen(s->json, "w");
    uint32_t data = 0xffffffffu;
    uint32_t code = 0xffffffffu;
    uint32_t stage = 0xffffffffu;
    uint32_t inject = 0xffffffffu;
    uint32_t swi = 0xffffffffu;

    if (f == NULL) return;
    (void)guest_read32(s->hdw, L1_DATA_VA, &data);
    (void)guest_read32(s->hdw, L1_CODE_VA, &code);
    (void)guest_read32(s->hdw, L1_STAGE_VA, &stage);
    (void)guest_read32(s->hdw, L1_INJECT_VA, &inject);
    (void)guest_read32(s->hdw, SWI_VECTOR_VA, &swi);

    uint32_t ttbr0 = s->hdw->cpu->cp15.c2_base0;
    uint32_t l1_phys_base = ttbr0 & 0xffffc000u;
    uint32_t inject_phys_desc = ldl_phys(l1_phys_base + 0x0c00u);
    uint32_t lcdsaddr1 = ((s3c2410_lcd_t *)s->hdw->s3c2410_lcd)->lcdsaddr1;
    uint32_t fb_phys = lcd_framebuffer_phys(s);
    uint32_t inject_desc_addr =
        (s->hdw->cpu->cp15.c2_base0 & 0xffffc000u) + 0x0c00u;

    fprintf(
        f,
        "{\n"
        "  \"status\":\"%s\",\n"
        "  \"result\":\"0x%08x\",\n"
        "  \"saw_pass\":%s,\n"
        "  \"payload_keyboard_scheduled\":%s,\n"
        "  \"returned\":%s,\n"
        "  \"return_settled\":%s,\n"
        "  \"return_observations\":%u,\n"
        "  \"pc\":\"0x%08x\",\n"
        "  \"saved_pc\":\"0x%08x\",\n"
        "  \"l1_data\":\"0x%08x\",\n"
        "  \"l1_code\":\"0x%08x\",\n"
        "  \"l1_stage\":\"0x%08x\",\n"
        "  \"l1_inject\":\"0x%08x\",\n"
        "  \"swi\":\"0x%08x\",\n"
        "  \"saved_swi\":\"0x%08x\",\n"
        "  \"ttbr0\":\"0x%08x\",\n"
        "  \"inject_phys_desc\":\"0x%08x\",\n"
        "  \"cpsr\":\"0x%08x\",\n"
        "  \"sp\":\"0x%08x\",\n"
        "  \"lr\":\"0x%08x\",\n"
        "  \"ifsr\":\"0x%08x\",\n"
        "  \"ifar\":\"0x%08x\",\n"
        "  \"lcdsaddr1\":\"0x%08x\",\n"
        "  \"framebuffer_phys\":\"0x%08x\",\n"
        "  \"inject_desc_phys\":\"0x%08x\",\n"
        "  \"stock_mpllcon\":\"0x%08x\",\n"
        "  \"stock_clkslow\":\"0x%08x\",\n"
        "  \"stock_clkdivn\":\"0x%08x\",\n"
        "  \"stock_tcfg0\":\"0x%08x\",\n"
        "  \"stock_tcfg1\":\"0x%08x\",\n"
        "  \"stock_tcon\":\"0x%08x\",\n"
        "  \"stock_tcntb0\":\"0x%08x\",\n"
        "  \"lcd_state_diff\":\"0x%08x\"\n"
        "}\n",
        status,
        s->result,
        s->saw_pass ? "true" : "false",
        s->payload_keyboard_scheduled ? "true" : "false",
        s->returned ? "true" : "false",
        return_is_settled(s) ? "true" : "false",
        s->return_quiescence.observations,
        s->hdw->cpu->regs[15],
        s->stock.regs[15],
        data, code, stage, inject, swi, s->old_swi,
        ttbr0,
        inject_phys_desc,
        cpsr_read(s->hdw->cpu),
        s->hdw->cpu->regs[13],
        s->hdw->cpu->regs[14],
        s->hdw->cpu->cp15.c5_insn,
        s->hdw->cpu->cp15.c6_insn,
        lcdsaddr1,
        fb_phys,
        inject_desc_addr,
        s->stock_mpllcon,
        s->stock_clkslow,
        s->stock_clkdivn,
        s->stock_tcfg0,
        s->stock_tcfg1,
        s->stock_tcon,
        s->stock_tcntb0,
        s->lcd_state_diff);
    fclose(f);
}

static void fail(struct e2e *s, const char *why)
{
    s->failed = 1;
    fprintf(stderr, "HPC1 E2E FAIL: %s\n", why);
    json(s, why);
    hdw_stop(s->hdw);
}

static int restore_stock(struct e2e *s)
{
    uint32_t data, code, stage, swi;

    if (!guest_read32(s->hdw, L1_DATA_VA, &data) ||
        !guest_read32(s->hdw, L1_CODE_VA, &code) ||
        !guest_read32(s->hdw, L1_STAGE_VA, &stage) ||
        !guest_read32(s->hdw, SWI_VECTOR_VA, &swi))
        return 0;

    if (data != s->old_data ||
        code != s->old_code ||
        stage != s->old_stage ||
        swi != s->old_swi)
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

static void finish_return(void *arg)
{
    struct e2e *s = arg;

    if (!is_display_on() || s->hdw->cpu->regs[15] == SENTINEL_VA) {
        fail(s, "stock-resume");
        return;
    }

    if (hpclang_lcd_quiescence_observe(
            &s->return_quiescence,
            s->hdw,
            LCD_W,
            s->lcd_height,
            RETURN_STABLE_OBSERVATIONS)) {
        unsigned pixels = capture_lcd(
            s->hdw, s->return_pgm, s->lcd_height);
        if (pixels == 0u) {
            fail(s, "stock-resume");
            return;
        }
        json(s, "pass");
        hdw_stop(s->hdw);
        return;
    }

    if (timer_get_clock() >= s->return_deadline) {
        fail(s, "return-unsettled");
        return;
    }
    if (!later(finish_return, s, RETURN_POLL_US))
        fail(s, "return-timer");
}

static void monitor(void *arg)
{
    struct e2e *s = arg;
    uint32_t v = 0;

    if (timer_get_clock() >= s->deadline) {
        fail(s, "timeout");
        return;
    }

    if (guest_read32(s->hdw, s->result_addr, &v)) {
        size_t keyboard_count =
            sizeof(keyboard_codes) / sizeof(keyboard_codes[0]);

        if (!s->payload_keyboard_scheduled && v == PHASE_KEYBOARD_READY) {
            if (!capture_keyboard_progress(s, 0u)) {
                fail(s, "keyboard-capture");
                return;
            }
            if (!schedule_payload_keyboard(s)) {
                fail(s, "keyboard-schedule");
                return;
            }
        } else if (s->payload_keyboard_scheduled &&
                   (v & 0xffff0000u) == PHASE_KEYBOARD_ACK) {
            size_t ack = (size_t)(v & 0xffffu);
            if (ack == 0u ||
                ack > keyboard_count ||
                ack > s->payload_keyboard_sent) {
                fail(s, "keyboard-ack");
                return;
            }
            if (ack > s->payload_keyboard_captured) {
                if (!capture_keyboard_progress(s, ack)) {
                    fail(s, "keyboard-capture");
                    return;
                }
                s->payload_keyboard_captured = ack;
            }
            if (ack == s->payload_keyboard_sent &&
                ack < keyboard_count) {
                if (!schedule_payload_keyboard_key(s, ack)) {
                    fail(s, "keyboard-schedule");
                    return;
                }
            }
        } else if (!s->saw_pass && v == PASS_WORD) {
            s->result = v;
            s->saw_pass = 1;
            /*
             * The final UP can be followed by PASS before the monitor sees
             * KACK(51). PASS is authoritative evidence that the payload
             * accepted all 51 DOWN->UP pairs.
             */
            if (s->payload_keyboard_scheduled &&
                s->payload_keyboard_captured < keyboard_count) {
                if (!capture_keyboard_progress(s, keyboard_count)) {
                    fail(s, "keyboard-capture");
                    return;
                }
                s->payload_keyboard_captured = keyboard_count;
            }
            if (!capture_lcd(s->hdw, s->pass_pgm, s->lcd_height)) {
                fail(s, "pass-capture");
                return;
            }
            fprintf(stderr, "HPC1 machine-readable PASS observed\n");
        } else if ((v & 0xffff0000u) == FAIL_PREFIX) {
            s->result = v;
            (void)capture_lcd(s->hdw, s->pass_pgm, s->lcd_height);
            fail(s, "runtime-fail");
            return;
        }
    }

    if (s->saw_pass && s->hdw->cpu->regs[15] == SENTINEL_VA) {
        s->lcd_state_diff = check_lcd_state(s);
        if (s->lcd_state_diff != 0u) {
            fail(s, "lcd-state");
            return;
        }

        hpclang_lcd_quiescence_seed(
            &s->return_quiescence, s->hdw, LCD_W, s->lcd_height);
        if (!restore_stock(s)) {
            fail(s, "restore");
            return;
        }
        s->return_deadline = timer_get_clock() + RETURN_DEADLINE_US;
        if (!tap(&on_down, &on_up, RETURN_ON_US) ||
            !later(
                finish_return,
                s,
                RETURN_ON_US + KEY_HOLD_US + RETURN_POLL_US))
            fail(s, "return-timer");
        return;
    }

    if (!later(monitor, s, POLL_US))
        fail(s, "monitor-timer");
}

static void inject(void *arg)
{
    struct e2e *s = arg;
    CPUARMState *cpu = s->hdw->cpu;

    if (!map_test_ram(s) ||
        !build_launcher_ram(s) ||
        !snapshot_stock(s)) {
        fail(s, "prepare");
        return;
    }

    if (!capture_lcd(s->hdw, s->before_pgm, s->lcd_height)) {
        fail(s, "before-capture");
        return;
    }
    if (!guest_write32(
            s->hdw,
            L1_INJECT_VA,
            INJECT_L2_PHYS | COARSE_FLAGS)) {
        fail(s, "inject-l1");
        return;
    }
    tlb_flush(cpu, 1);
    tb_flush(cpu);

    if (cpu_get_phys_page_debug(cpu, STAGE0_VA) !=
        (target_phys_addr_t)(INJECT_PHYS + 0x1000u)) {
        fail(s, "inject-map");
        return;
    }

    /*
     * Do not jump into stage0 from an arbitrary instant of the stock ARM
     * emulator loop. The real Saturn/RPL launcher enters ARM code at a safe
     * call boundary. Model that boundary explicitly with a private System-mode
     * stack and IRQ/FIQ masked until stage0 returns to our sentinel.
     *
     * The host restores the exact stock CPU snapshot afterwards.
     */
    uint32_t launch_cpsr =
        (s->stock.cpsr & (CPSR_NZCV | CPSR_Q | CPSR_GE)) |
        ARM_CPU_MODE_SYS | CPSR_I | CPSR_F;
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
}

int main(int argc, char **argv)
{
    const char *stage0_path = getenv("HPCLANG_X50NG_STAGE0");
    const char *result_text = getenv("HPCLANG_X50NG_RESULT_ADDR");
    const char *lcd_height_text = getenv("HPCLANG_X50NG_LCD_HEIGHT");
    char *end = NULL;
    unsigned long result;
    struct e2e s;

    if (stage0_path == NULL || result_text == NULL) {
        fprintf(stderr, "missing E2E stage0/result configuration\n");
        return EXIT_FAILURE;
    }

    memset(&s, 0, sizeof(s));
    result = strtoul(result_text, &end, 0);
    if (end == result_text || *end != '\0' || result > 0xfffffffful) {
        fprintf(stderr, "invalid result address\n");
        return EXIT_FAILURE;
    }
    s.result_addr = (uint32_t)result;

    s.lcd_height = lcd_height_text ? atoi(lcd_height_text) : 80;
    if (s.lcd_height != 64 && s.lcd_height != 80) {
        fprintf(stderr, "invalid LCD height\n");
        return EXIT_FAILURE;
    }

    s.stage0 = load_file(stage0_path, &s.stage0_size);
    if (s.stage0 == NULL) {
        fprintf(stderr, "cannot read stage0\n");
        return EXIT_FAILURE;
    }

    s.before_pgm = getenv("HPCLANG_X50NG_BEFORE_PGM");
    s.pass_pgm = getenv("HPCLANG_X50NG_PASS_PGM");
    s.return_pgm = getenv("HPCLANG_X50NG_RETURN_PGM");
    s.json = getenv("HPCLANG_X50NG_E2E_JSON");
    if (!s.before_pgm) s.before_pgm = "before.pgm";
    if (!s.pass_pgm) s.pass_pgm = "pass.pgm";
    if (!s.return_pgm) s.return_pgm = "return.pgm";
    if (!s.json) s.json = "e2e.json";

    config_t config = hpclang_x50ng_headless_config(argc, argv);
    s.hdw = emulator_init(&config);

    if (!tap(&f_down, &f_up, BOOT_NO_US) ||
        !tap(&f_down, &f_up, BOOT_OK_US) ||
        !later(inject, &s, INJECT_US)) {
        fprintf(stderr, "cannot schedule boot/E2E actions\n");
        return EXIT_FAILURE;
    }

    main_loop(s.hdw);
    exit_modules(s.hdw);
    free(s.stage0);

    return (s.failed || !s.saw_pass || !s.returned ||
            !return_is_settled(&s))
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
