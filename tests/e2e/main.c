#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/keyboard.h>
#include <hpclang/storage.h>
#include <hpclang/target.h>
#include <hpclang/time.h>

int hpclang_e2e_cpp_test(void);
extern const struct hpclang_font hpclang_e2e_ui_font;

static const char hpclang_e2e_font_label[] = HPCLANG_UI_TEXT("FONT OK");
static const char hpclang_e2e_pass_label[] = HPCLANG_UI_TEXT("RUNTIME PASS");
static const char hpclang_e2e_fail_label[] = HPCLANG_UI_TEXT("RUNTIME FAIL");
static const char hpclang_e2e_code_label[] = HPCLANG_UI_TEXT("CODE");
static const char hpclang_e2e_hex_digits[] = HPCLANG_UI_TEXT("0123456789ABCDEF");

volatile uint32_t hpclang_e2e_result;
volatile uint32_t hpclang_initialized_probe = 0x13579bdfu;
volatile uint32_t hpclang_bss_probe;

static _Thread_local uint32_t hpclang_tls_initialized = 0x2468ace0u;
static _Thread_local uint32_t hpclang_tls_zero;

static volatile uint32_t hpclang_ctor_probe;

#ifdef HPCLANG_E2E_KEYBOARD
static uint64_t hpclang_keyboard_passed_bits;
static uint8_t hpclang_keyboard_failed_scan = 0xffu;
static unsigned hpclang_keyboard_passed_count;
#endif

static volatile uint32_t hpclang_u32_a = 1000003u;
static volatile uint32_t hpclang_u32_b = 97u;
static volatile uint64_t hpclang_u64_a = UINT64_C(0x123456789abcdef0);
static volatile uint64_t hpclang_u64_b = UINT64_C(1000003);
static volatile double hpclang_double_a = 1.25;
static volatile double hpclang_double_b = 3.5;

__attribute__((constructor))
static void hpclang_runtime_ctor(void)
{
    hpclang_ctor_probe = 0xc0def00du;
}

static void text(
    struct hpclang_surface *surface,
    int x,
    int y,
    const char *s)
{
    hpclang_surface_draw_text(
        surface, hpclang_font_mini(), x, y, s, 255u);
}

static void hex2(
    struct hpclang_surface *surface,
    int x,
    int y,
    uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    const struct hpclang_font *font = hpclang_font_mini();

    hpclang_surface_draw_char(
        surface, font, x, y,
        (uint8_t)digits[(value >> 4) & 15u], 255u);
    hpclang_surface_draw_char(
        surface, font, x + (int)hpclang_font_advance(font), y,
        (uint8_t)digits[value & 15u], 255u);
}

static void result_text(
    struct hpclang_surface *surface,
    int x,
    int y,
    const char *s)
{
    hpclang_surface_draw_text(
        surface, &hpclang_e2e_ui_font, x, y, s, 255u);
}

static void result_hex2(
    struct hpclang_surface *surface,
    int x,
    int y,
    uint32_t value)
{
    const struct hpclang_font *font = &hpclang_e2e_ui_font;
    hpclang_surface_draw_char(
        surface, font, x, y,
        (uint8_t)hpclang_e2e_hex_digits[(value >> 4) & 15u], 255u);
    hpclang_surface_draw_char(
        surface, font, x + (int)hpclang_font_advance(font), y,
        (uint8_t)hpclang_e2e_hex_digits[value & 15u], 255u);
}

#ifdef HPCLANG_E2E_KEYBOARD
static int keyboard_scan_passed(uint8_t scan)
{
    return
        (hpclang_keyboard_passed_bits & (UINT64_C(1) << scan)) != 0u;
}

static void keyboard_rect(
    struct hpclang_surface *surface,
    int x,
    int y,
    int w,
    int h,
    int fill)
{
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            int corner =
                (xx == 0 || xx == w - 1) &&
                (yy == 0 || yy == h - 1);
            if (corner)
                continue;
            if (fill || yy == 0 || yy == h - 1 ||
                xx == 0 || xx == w - 1)
                hpclang_surface_set_pixel(surface, x + xx, y + yy, 255u);
        }
    }
}

static void keyboard_cross(
    struct hpclang_surface *surface,
    int x,
    int y,
    int w,
    int h)
{
    int cx = x + w / 2;
    int cy = y + h / 2;

    for (int i = -1; i <= 1; ++i) {
        hpclang_surface_set_pixel(surface, cx + i, cy + i, 255u);
        hpclang_surface_set_pixel(surface, cx + i, cy - i, 255u);
    }
}

static void keyboard_cell(
    struct hpclang_surface *surface,
    uint8_t scan,
    int x,
    int y,
    int w,
    int h)
{
    int passed = keyboard_scan_passed(scan);
    keyboard_rect(surface, x, y, w, h, passed);
    if (!passed && hpclang_keyboard_failed_scan == scan)
        keyboard_cross(surface, x, y, w, h);
}

static void keyboard_cursor_cell(
    struct hpclang_surface *surface,
    uint8_t scan,
    int cx,
    int cy)
{
    int passed = keyboard_scan_passed(scan);

    for (int yy = -3; yy <= 3; ++yy) {
        int vertical = yy < 0 ? -yy : yy;
        int half_width = vertical == 3 ? 1 : vertical == 2 ? 2 : 3;

        for (int xx = -half_width; xx <= half_width; ++xx) {
            int boundary =
                vertical == 3 || xx == -half_width || xx == half_width;
            if (passed || boundary)
                hpclang_surface_set_pixel(
                    surface, cx + xx, cy + yy, 255u);
        }
    }

    if (!passed && hpclang_keyboard_failed_scan == scan) {
        for (int i = -1; i <= 1; ++i) {
            hpclang_surface_set_pixel(surface, cx + i, cy + i, 255u);
            hpclang_surface_set_pixel(surface, cx + i, cy - i, 255u);
        }
    }
}

/*
 * These scan positions are laid out by physical key position, not by the
 * S3C2410 electrical matrix.  The five automated calculators share this
 * chassis geometry: six menu keys, two rows of three control keys plus the
 * four-key cursor cluster, then seven rows of five main keys.
 */
static const uint8_t keyboard_soft_scans[6] = {
    35u, 36u, 37u, 38u, 39u, 40u,
};

static const uint8_t keyboard_control_scans[2][3] = {
    {41u, 46u, 47u},
    {48u, 49u, 50u},
};

static const uint8_t keyboard_main_scans[7][5] = {
    {28u, 21u, 14u,  7u, 0u},
    {29u, 22u, 15u,  8u, 1u},
    {30u, 23u, 16u,  9u, 2u},
    {57u, 24u, 17u, 10u, 3u},
    {58u, 25u, 18u, 11u, 4u},
    {59u, 26u, 19u, 12u, 5u},
    {56u, 27u, 20u, 13u, 6u},
};

static void draw_keyboard_result(struct hpclang_surface *surface, int pass)
{
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(
        surface, &hpclang_e2e_ui_font, 2, 1,
        hpclang_e2e_font_label, 255u);

    if (pass) {
        text(surface, 99, 1, "PASS 51");
    } else if (hpclang_keyboard_failed_scan != 0xffu) {
        text(surface, 99, 1, "FAIL");
        hex2(surface, 116, 1, hpclang_keyboard_failed_scan);
    } else {
        char progress[8];
        (void)snprintf(
            progress, sizeof(progress), "KEY %02u",
            hpclang_keyboard_passed_count);
        text(surface, 105, 1, progress);
    }

    for (int col = 0; col < 6; ++col)
        keyboard_cell(
            surface, keyboard_soft_scans[col],
            4 + col * 21, 7, 17, 4);

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col)
            keyboard_cell(
                surface, keyboard_control_scans[row][col],
                4 + col * 26, 13 + row * 8, 20, 5);
    }

    keyboard_cursor_cell(surface, 42u, 105, 14);
    keyboard_cursor_cell(surface, 43u,  96, 20);
    keyboard_cursor_cell(surface, 45u, 114, 20);
    keyboard_cursor_cell(surface, 44u, 105, 26);

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col)
            keyboard_cell(
                surface, keyboard_main_scans[row][col],
                4 + col * 26, 30 + row * 5, 19, 4);
    }
}
#endif

static int display_height(uint32_t model)
{
    return model == HPCLANG_MODEL_HP49GPLUS ||
           model == HPCLANG_MODEL_HP50G
        ? 80
        : 64;
}

#ifdef HPCLANG_E2E_STORAGE
static uint32_t stock_stream_storage_test(uint32_t model)
{
    if (model != HPCLANG_MODEL_HP49GPLUS &&
        model != HPCLANG_MODEL_HP50G) {
        errno = 0;
        FILE *unavailable = fopen("HPCLANG.TST", "rb");
        if (unavailable != NULL) {
            fclose(unavailable);
            return 0x31u;
        }
        if (errno != ENOSYS)
            return 0x32u;

        struct stat unavailable_stat;
        errno = 0;
        if (stat("HPCLANG.TST", &unavailable_stat) == 0 ||
            errno != ENOSYS)
            return 0x53u;

        struct hpclang_stream_path_info unavailable_path;
        errno = 0;
        if (hpclang_stream_path_query(
                "HPCLANG.TST", &unavailable_path) == 0 ||
            errno != ENOSYS)
            return 0x54u;


        struct hpclang_stream_storage_info unavailable_storage;
        errno = 0;
        if (hpclang_stream_storage_query(&unavailable_storage) == 0 ||
            errno != ENOSYS)
            return 0x55u;

        uint64_t unavailable_free = 0u;
        errno = 0;
        if (hpclang_stream_storage_free_bytes(&unavailable_free) == 0 ||
            errno != ENOSYS)
            return 0x56u;

        errno = 0;
        if (rename("HPCLANG.A", "HPCLANG.B") == 0 ||
            errno != ENOSYS)
            return 0x57u;

        return 0u;
    }

    static const char input_expected[] = "HPCLANG-STORAGE\n";
    static const char output_expected[] = "stream-write-seek";
    char buffer[32];

    /*
     * Test missing-file semantics before any write/delete can leave unrelated
     * stock-OS error state behind. This is also the public regression for the
     * stock KOS -200 fopen sentinel.
     */
    errno = 0;
    FILE *missing = fopen("HPCLANG.MIS", "rb");
    if (missing != NULL) {
        fclose(missing);
        return 0x40u;
    }
    if (errno != ENOENT)
        return 0x41u;

    errno = 0;
    missing = fopen("HPCLANG.MIS", "r+b");
    if (missing != NULL) {
        fclose(missing);
        return 0x42u;
    }
    if (errno != ENOENT)
        return 0x43u;

    struct hpclang_stream_path_info path_info;
    if (hpclang_stream_path_query("HPCLANG.TST", &path_info) != 0)
        return 0x58u;
    if (path_info.version != HPCLANG_STREAM_PATH_INFO_VERSION ||
        path_info.size < sizeof(path_info) ||
        path_info.type != HPCLANG_STREAM_PATH_TYPE_FILE ||
        path_info.byte_size != sizeof(input_expected) - 1u)
        return 0x59u;

    struct stat input_info;
    if (stat("HPCLANG.TST", &input_info) != 0)
        return 0x5au;
    if (!S_ISREG(input_info.st_mode) ||
        input_info.st_size != (off_t)(sizeof(input_expected) - 1u))
        return 0x5bu;

    if ((path_info.time_validity &
         HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID) == 0u ||
        path_info.last_modified.year != 2024u ||
        path_info.last_modified.month != 1u ||
        path_info.last_modified.day != 2u ||
        path_info.last_modified.hour != 3u ||
        path_info.last_modified.minute != 4u ||
        path_info.last_modified.second != 6u)
        return 0x69u;

    /*
     * "/" is synthetic volume-root metadata because KOS GetSpecialFileInfo
     * only addresses real directory entries. Exercise that contract separately
     * from the real named-directory witness below.
     */
    struct hpclang_stream_path_info root_info;
    if (hpclang_stream_path_query("/", &root_info) != 0 ||
        root_info.type != HPCLANG_STREAM_PATH_TYPE_DIRECTORY)
        return 0x5cu;

    struct hpclang_stream_path_info directory_info;
    if (hpclang_stream_path_query("ZZHPDIR", &directory_info) != 0 ||
        directory_info.type != HPCLANG_STREAM_PATH_TYPE_DIRECTORY)
        return 0x68u;

    /*
     * x50ng's host-directory SD fixture currently exposes the ordinary FAT
     * archive bit on this pre-existing file. KOS FATTR mutation is not part of
     * the public contract because this backend does not preserve requested
     * read-only/hidden mutations even when Stock reports success.
     */
    if ((path_info.attributes &
         HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE) == 0u)
        return 0x67u;

    struct hpclang_stream_storage_info storage_info;
    if (hpclang_stream_storage_query(&storage_info) != 0)
        return 0x5du;
    if (storage_info.version != HPCLANG_STREAM_STORAGE_INFO_VERSION ||
        storage_info.size < sizeof(storage_info) ||
        (storage_info.flags & HPCLANG_STREAM_STORAGE_MEDIA_PRESENT) == 0u ||
        (storage_info.flags &
         HPCLANG_STREAM_STORAGE_FREE_BYTES_VALID) == 0u ||
        storage_info.free_bytes == 0u)
        return 0x5eu;
    if ((storage_info.flags &
         HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID) != 0u &&
        storage_info.capacity_bytes < storage_info.free_bytes)
        return 0x5fu;

    uint64_t free_bytes = 0u;
    if (hpclang_stream_storage_free_bytes(&free_bytes) != 0 ||
        free_bytes != storage_info.free_bytes)
        return 0x60u;

    FILE *input = fopen("HPCLANG.TST", "rb");
    if (input == NULL)
        return 0x33u;

    memset(buffer, 0, sizeof(buffer));
    size_t got = fread(
        buffer, 1u, sizeof(input_expected) - 1u, input);
    int input_close = fclose(input);
    if (got != sizeof(input_expected) - 1u ||
        input_close != 0 ||
        memcmp(buffer, input_expected, sizeof(input_expected) - 1u) != 0)
        return 0x34u;

    FILE *output = fopen("HPCLANG.OUT", "w+b");
    if (output == NULL)
        return 0x35u;

    size_t written = fwrite(
        output_expected, 1u, sizeof(output_expected) - 1u, output);
    if (written != sizeof(output_expected) - 1u ||
        fflush(output) != 0) {
        fclose(output);
        return 0x36u;
    }

    if (fseek(output, 7L, SEEK_SET) != 0 ||
        ftell(output) != 7L) {
        fclose(output);
        return 0x37u;
    }

    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, 5u, output);
    if (got != 5u ||
        memcmp(buffer, "write", 5u) != 0 ||
        ftell(output) != 12L) {
        fclose(output);
        return 0x38u;
    }

    if (fclose(output) != 0)
        return 0x39u;

    output = fopen("HPCLANG.OUT", "rb");
    if (output == NULL)
        return 0x3au;

    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(buffer), output);
    int output_close = fclose(output);

    if (output_close != 0 ||
        got != sizeof(output_expected) - 1u ||
        memcmp(buffer, output_expected, sizeof(output_expected) - 1u) != 0)
        return 0x3bu;

    if (remove("HPCLANG.OUT") != 0)
        return 0x3cu;

    errno = 0;
    int missing_fd = open("HPCLANG.OUT", O_RDONLY);
    if (missing_fd >= 0) {
        close(missing_fd);
        return 0x3du;
    }
    if (errno != ENOENT)
        return 0x3eu;

    errno = 0;
    output = fopen("HPCLANG.OUT", "rb");
    if (output != NULL) {
        fclose(output);
        return 0x3fu;
    }
    if (errno != ENOENT)
        return 0x44u;

    /*
     * "w" must create a genuinely missing file. This specifically exercises
     * O_CREAT|O_TRUNC when KOS delete reports the path as absent.
     */
    static const char create_expected[] = "new-file";
    output = fopen("HPCLANG.NEW", "w+b");
    if (output == NULL)
        return 0x45u;
    written = fwrite(
        create_expected, 1u, sizeof(create_expected) - 1u, output);
    if (written != sizeof(create_expected) - 1u ||
        fclose(output) != 0)
        return 0x46u;

    output = fopen("HPCLANG.NEW", "rb");
    if (output == NULL)
        return 0x47u;
    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(buffer), output);
    output_close = fclose(output);
    if (output_close != 0 ||
        got != sizeof(create_expected) - 1u ||
        memcmp(buffer, create_expected, sizeof(create_expected) - 1u) != 0)
        return 0x48u;

    /*
     * Stock 2.15 KOS FRENAME is measured broken on HP49g+ and HP50g.
     * The public standard name therefore fails explicitly rather than doing
     * a non-atomic copy+delete that would pretend to be POSIX rename.
     */
    errno = 0;
    if (rename("HPCLANG.NEW", "HPCLANG.REN") == 0)
        return 0x61u;
    if (errno != ENOTSUP)
        return 0x62u;

    output = fopen("HPCLANG.NEW", "rb");
    if (output == NULL)
        return 0x63u;
    if (fclose(output) != 0)
        return 0x64u;

    errno = 0;
    output = fopen("HPCLANG.REN", "rb");
    if (output != NULL) {
        fclose(output);
        return 0x65u;
    }
    if (errno != ENOENT)
        return 0x66u;

    if (remove("HPCLANG.NEW") != 0)
        return 0x49u;

    /*
     * HPGCC 2.0 documents a stock KOS bug where FILE_APPEND does not create a
     * missing file. Standard fopen("a") must still create, then every later
     * append must preserve existing contents.
     */
    output = fopen("HPCLANG.APP", "ab");
    if (output == NULL)
        return 0x4au;
    if (fwrite("one", 1u, 3u, output) != 3u ||
        fclose(output) != 0)
        return 0x4bu;

    output = fopen("HPCLANG.APP", "ab");
    if (output == NULL)
        return 0x4cu;
    if (fwrite("-two", 1u, 4u, output) != 4u ||
        fclose(output) != 0)
        return 0x4du;

    output = fopen("HPCLANG.APP", "rb");
    if (output == NULL)
        return 0x4eu;
    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(buffer), output);
    output_close = fclose(output);
    if (output_close != 0 ||
        got != 7u ||
        memcmp(buffer, "one-two", 7u) != 0)
        return 0x4fu;

    if (remove("HPCLANG.APP") != 0)
        return 0x50u;

    errno = 0;
    if (remove("HPCLANG.APP") == 0)
        return 0x51u;
    if (errno != ENOENT)
        return 0x52u;

    return 0u;
}
#endif

#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
static int enter_pressed(void)
{
    /*
     * The interactive HP49g+/50g validation package uses ENTER, which is
     * matrix column 0 / row 6 on that family. Reuse the public raw scanner
     * instead of carrying a second GPIO implementation in the test payload.
     */
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    return hpclang_keyboard_matrix_is_down(&state, 0u, 6u);
}
#endif

static void hold_result(void)
{
#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
    /*
     * Keep the interactive HP50g result visible until ENTER. Automated stock
     * E2E stops the emulator as soon as it observes PASS, so it does not inject
     * a keyboard interrupt into the launcher return path.
     */
    while (enter_pressed())
        ;
    while (!enter_pressed())
        ;
    while (enter_pressed())
        ;
#else
    /*
     * Existing HP39/40 and canonical gates intentionally remain automatic:
     * they validate return/state restoration independently of keyboard GPIO.
     */
    for (volatile uint32_t n = 12000000u; n != 0; --n)
        __asm__ volatile("nop");
#endif
}

static int finish(uint32_t failure, uint32_t model)
{
    (void)model;
    struct hpclang_surface *surface = hpclang_display_surface();

    hpclang_surface_clear(surface, 0u);
    if (failure == 0) {
#ifdef HPCLANG_E2E_KEYBOARD
        draw_keyboard_result(surface, 1);
#else
        result_text(surface, 30, 28, hpclang_e2e_pass_label);
#endif
#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
        text(surface, 39, 40, "ENTER TO EXIT");
#endif
        hpclang_e2e_result = 0x50415353u;
        hold_result();
        return 0;
    }

#ifdef HPCLANG_E2E_KEYBOARD
    if (failure == 0x10u) {
        draw_keyboard_result(surface, 0);
    } else
#endif
    {
        result_text(surface, 24, 24, hpclang_e2e_fail_label);
        result_text(surface, 48, 36, hpclang_e2e_code_label);
        result_hex2(surface, 72, 36, failure);
    }
#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
    text(surface, 39, 48, "ENTER TO EXIT");
#endif
    hpclang_e2e_result = 0x46410000u | failure;
    hold_result();
    return (int)failure;
}

static int supported_model(uint32_t model)
{
    return model >= HPCLANG_MODEL_HP39GPLUS &&
           model <= HPCLANG_MODEL_HP50G;
}

static uint32_t expected_family(uint32_t model)
{
    if (model >= HPCLANG_MODEL_HP39GPLUS && model <= HPCLANG_MODEL_HP40GS)
        return HPCLANG_FAMILY_HP39;
    if (model == HPCLANG_MODEL_HP48GII)
        return HPCLANG_FAMILY_HP48;
    if (model == HPCLANG_MODEL_HP49GPLUS || model == HPCLANG_MODEL_HP50G)
        return HPCLANG_FAMILY_HP4950;
    return HPCLANG_FAMILY_UNKNOWN;
}

#ifdef HPCLANG_E2E_KEYBOARD
struct expected_key_event {
    uint8_t scan;
    uint16_t key;
    uint32_t down_modifiers;
};

#define E(scan, key) { scan##u, key, 0u }
#define EM(scan, key, mods) { scan##u, key, mods }

static const struct expected_key_event hp39_keyboard[] = {
    E(0, HPCLANG_KEY_HP39_DEL),
    E(1, HPCLANG_KEY_HP39_LOG),
    E(2, HPCLANG_KEY_DIVIDE),
    E(3, HPCLANG_KEY_MULTIPLY),
    E(4, HPCLANG_KEY_MINUS),
    E(5, HPCLANG_KEY_PLUS),
    E(6, HPCLANG_KEY_ENTER),
    E(7, HPCLANG_KEY_HP39_X_THETA_T),
    E(8, HPCLANG_KEY_HP39_LN),
    E(9, HPCLANG_KEY_HP39_RIGHT_PAREN),
    E(10, HPCLANG_KEY_9),
    E(11, HPCLANG_KEY_6),
    E(12, HPCLANG_KEY_3),
    E(13, HPCLANG_KEY_HP39_NEGATE),
    E(14, HPCLANG_KEY_HP39_DERIVATIVE),
    E(15, HPCLANG_KEY_HP39_TAN),
    E(16, HPCLANG_KEY_HP39_LEFT_PAREN),
    E(17, HPCLANG_KEY_8),
    E(18, HPCLANG_KEY_5),
    E(19, HPCLANG_KEY_2),
    E(20, HPCLANG_KEY_PERIOD),
    E(21, HPCLANG_KEY_HP39_MATH),
    E(22, HPCLANG_KEY_HP39_COS),
    E(23, HPCLANG_KEY_HP39_POWER),
    E(24, HPCLANG_KEY_7),
    E(25, HPCLANG_KEY_4),
    E(26, HPCLANG_KEY_1),
    E(27, HPCLANG_KEY_0),
    E(28, HPCLANG_KEY_HP39_VARS),
    E(29, HPCLANG_KEY_HP39_SIN),
    E(30, HPCLANG_KEY_HP39_SQUARE),
    E(35, HPCLANG_KEY_F1),
    E(36, HPCLANG_KEY_F2),
    E(37, HPCLANG_KEY_F3),
    E(38, HPCLANG_KEY_F4),
    E(39, HPCLANG_KEY_F5),
    E(40, HPCLANG_KEY_F6),
    E(41, HPCLANG_KEY_HP39_SYMB),
    E(42, HPCLANG_KEY_UP),
    E(43, HPCLANG_KEY_LEFT),
    E(44, HPCLANG_KEY_DOWN),
    E(45, HPCLANG_KEY_RIGHT),
    E(46, HPCLANG_KEY_HP39_PLOT),
    E(47, HPCLANG_KEY_HP39_NUM),
    E(48, HPCLANG_KEY_HP39_HOME),
    E(49, HPCLANG_KEY_HP39_APLET),
    E(50, HPCLANG_KEY_HP39_VIEWS),
    EM(56, HPCLANG_KEY_ON, HPCLANG_KEYBOARD_MOD_ON),
    E(57, HPCLANG_KEY_HP39_COMMA),
    EM(58, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA),
    EM(59, HPCLANG_KEY_HP39_SHIFT, HPCLANG_KEYBOARD_MOD_SHIFT),
};

static const struct expected_key_event hp48_keyboard[] = {
    E(0, HPCLANG_KEY_HP48_BACKSPACE),
    E(1, HPCLANG_KEY_HP48_TAN),
    E(2, HPCLANG_KEY_DIVIDE),
    E(3, HPCLANG_KEY_MULTIPLY),
    E(4, HPCLANG_KEY_MINUS),
    E(5, HPCLANG_KEY_PLUS),
    E(6, HPCLANG_KEY_ENTER),
    E(7, HPCLANG_KEY_HP48_SYMB),
    E(8, HPCLANG_KEY_HP48_COS),
    E(9, HPCLANG_KEY_HP48_RECIPROCAL),
    E(10, HPCLANG_KEY_9),
    E(11, HPCLANG_KEY_6),
    E(12, HPCLANG_KEY_3),
    E(13, HPCLANG_KEY_HP48_SPACE),
    E(14, HPCLANG_KEY_HP48_QUOTE),
    E(15, HPCLANG_KEY_HP48_SIN),
    E(16, HPCLANG_KEY_HP48_X),
    E(17, HPCLANG_KEY_8),
    E(18, HPCLANG_KEY_5),
    E(19, HPCLANG_KEY_2),
    E(20, HPCLANG_KEY_PERIOD),
    E(21, HPCLANG_KEY_HP48_EVAL),
    E(22, HPCLANG_KEY_HP48_SQRT),
    E(23, HPCLANG_KEY_HP48_NEGATE),
    E(24, HPCLANG_KEY_7),
    E(25, HPCLANG_KEY_4),
    E(26, HPCLANG_KEY_1),
    E(27, HPCLANG_KEY_0),
    E(28, HPCLANG_KEY_HP48_HIST),
    E(29, HPCLANG_KEY_HP48_POWER),
    E(30, HPCLANG_KEY_HP48_EEX),
    E(35, HPCLANG_KEY_F1),
    E(36, HPCLANG_KEY_F2),
    E(37, HPCLANG_KEY_F3),
    E(38, HPCLANG_KEY_F4),
    E(39, HPCLANG_KEY_F5),
    E(40, HPCLANG_KEY_F6),
    E(41, HPCLANG_KEY_HP48_APPS),
    E(42, HPCLANG_KEY_UP),
    E(43, HPCLANG_KEY_LEFT),
    E(44, HPCLANG_KEY_DOWN),
    E(45, HPCLANG_KEY_RIGHT),
    E(46, HPCLANG_KEY_HP48_MODE),
    E(47, HPCLANG_KEY_HP48_TOOL),
    E(48, HPCLANG_KEY_HP48_VAR),
    E(49, HPCLANG_KEY_HP48_STO),
    E(50, HPCLANG_KEY_HP48_NXT),
    EM(56, HPCLANG_KEY_ON, HPCLANG_KEYBOARD_MOD_ON),
    EM(57, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA),
    EM(58, HPCLANG_KEY_LEFT_SHIFT,
       HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_LEFT_SHIFT),
    EM(59, HPCLANG_KEY_RIGHT_SHIFT,
       HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT),
};

#define HP4950_SHARED(scan, key) E(scan, key)
#define HP4950_SHARED_MOD(scan, key, mods) EM(scan, key, mods)

static const struct expected_key_event hp49_keyboard[] = {
    HP4950_SHARED(0, HPCLANG_KEY_HP4950_BACKSPACE),
    HP4950_SHARED(1, HPCLANG_KEY_HP4950_TAN),
    E(2, HPCLANG_KEY_DIVIDE),
    E(3, HPCLANG_KEY_MULTIPLY),
    E(4, HPCLANG_KEY_MINUS),
    E(5, HPCLANG_KEY_PLUS),
    E(6, HPCLANG_KEY_ENTER),
    HP4950_SHARED(7, HPCLANG_KEY_HP4950_SYMB),
    HP4950_SHARED(8, HPCLANG_KEY_HP4950_COS),
    HP4950_SHARED(9, HPCLANG_KEY_HP4950_RECIPROCAL),
    E(10, HPCLANG_KEY_9),
    E(11, HPCLANG_KEY_6),
    E(12, HPCLANG_KEY_3),
    HP4950_SHARED(13, HPCLANG_KEY_HP4950_SPACE),
    E(14, HPCLANG_KEY_HP49_EQW),
    HP4950_SHARED(15, HPCLANG_KEY_HP4950_SIN),
    HP4950_SHARED(16, HPCLANG_KEY_HP4950_X),
    E(17, HPCLANG_KEY_8),
    E(18, HPCLANG_KEY_5),
    E(19, HPCLANG_KEY_2),
    E(20, HPCLANG_KEY_PERIOD),
    E(21, HPCLANG_KEY_HP49_CAT),
    HP4950_SHARED(22, HPCLANG_KEY_HP4950_SQRT),
    HP4950_SHARED(23, HPCLANG_KEY_HP4950_NEGATE),
    E(24, HPCLANG_KEY_7),
    E(25, HPCLANG_KEY_4),
    E(26, HPCLANG_KEY_1),
    E(27, HPCLANG_KEY_0),
    HP4950_SHARED(28, HPCLANG_KEY_HP4950_HIST),
    HP4950_SHARED(29, HPCLANG_KEY_HP4950_POWER),
    HP4950_SHARED(30, HPCLANG_KEY_HP4950_EEX),
    E(35, HPCLANG_KEY_F1),
    E(36, HPCLANG_KEY_F2),
    E(37, HPCLANG_KEY_F3),
    E(38, HPCLANG_KEY_F4),
    E(39, HPCLANG_KEY_F5),
    E(40, HPCLANG_KEY_F6),
    HP4950_SHARED(41, HPCLANG_KEY_HP4950_APPS),
    E(42, HPCLANG_KEY_UP),
    E(43, HPCLANG_KEY_LEFT),
    E(44, HPCLANG_KEY_DOWN),
    E(45, HPCLANG_KEY_RIGHT),
    HP4950_SHARED(46, HPCLANG_KEY_HP4950_MODE),
    HP4950_SHARED(47, HPCLANG_KEY_HP4950_TOOL),
    HP4950_SHARED(48, HPCLANG_KEY_HP4950_VAR),
    HP4950_SHARED(49, HPCLANG_KEY_HP4950_STO),
    HP4950_SHARED(50, HPCLANG_KEY_HP4950_NXT),
    EM(56, HPCLANG_KEY_ON, HPCLANG_KEYBOARD_MOD_ON),
    HP4950_SHARED_MOD(57, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA),
    HP4950_SHARED_MOD(
        58, HPCLANG_KEY_LEFT_SHIFT,
        HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_LEFT_SHIFT),
    HP4950_SHARED_MOD(
        59, HPCLANG_KEY_RIGHT_SHIFT,
        HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT),
};

static const struct expected_key_event hp50_keyboard[] = {
    HP4950_SHARED(0, HPCLANG_KEY_HP4950_BACKSPACE),
    HP4950_SHARED(1, HPCLANG_KEY_HP4950_TAN),
    E(2, HPCLANG_KEY_DIVIDE),
    E(3, HPCLANG_KEY_MULTIPLY),
    E(4, HPCLANG_KEY_MINUS),
    E(5, HPCLANG_KEY_PLUS),
    E(6, HPCLANG_KEY_ENTER),
    HP4950_SHARED(7, HPCLANG_KEY_HP4950_SYMB),
    HP4950_SHARED(8, HPCLANG_KEY_HP4950_COS),
    HP4950_SHARED(9, HPCLANG_KEY_HP4950_RECIPROCAL),
    E(10, HPCLANG_KEY_9),
    E(11, HPCLANG_KEY_6),
    E(12, HPCLANG_KEY_3),
    HP4950_SHARED(13, HPCLANG_KEY_HP4950_SPACE),
    E(14, HPCLANG_KEY_HP50_QUOTE),
    HP4950_SHARED(15, HPCLANG_KEY_HP4950_SIN),
    HP4950_SHARED(16, HPCLANG_KEY_HP4950_X),
    E(17, HPCLANG_KEY_8),
    E(18, HPCLANG_KEY_5),
    E(19, HPCLANG_KEY_2),
    E(20, HPCLANG_KEY_PERIOD),
    E(21, HPCLANG_KEY_HP50_EVAL),
    HP4950_SHARED(22, HPCLANG_KEY_HP4950_SQRT),
    HP4950_SHARED(23, HPCLANG_KEY_HP4950_NEGATE),
    E(24, HPCLANG_KEY_7),
    E(25, HPCLANG_KEY_4),
    E(26, HPCLANG_KEY_1),
    E(27, HPCLANG_KEY_0),
    HP4950_SHARED(28, HPCLANG_KEY_HP4950_HIST),
    HP4950_SHARED(29, HPCLANG_KEY_HP4950_POWER),
    HP4950_SHARED(30, HPCLANG_KEY_HP4950_EEX),
    E(35, HPCLANG_KEY_F1),
    E(36, HPCLANG_KEY_F2),
    E(37, HPCLANG_KEY_F3),
    E(38, HPCLANG_KEY_F4),
    E(39, HPCLANG_KEY_F5),
    E(40, HPCLANG_KEY_F6),
    HP4950_SHARED(41, HPCLANG_KEY_HP4950_APPS),
    E(42, HPCLANG_KEY_UP),
    E(43, HPCLANG_KEY_LEFT),
    E(44, HPCLANG_KEY_DOWN),
    E(45, HPCLANG_KEY_RIGHT),
    HP4950_SHARED(46, HPCLANG_KEY_HP4950_MODE),
    HP4950_SHARED(47, HPCLANG_KEY_HP4950_TOOL),
    HP4950_SHARED(48, HPCLANG_KEY_HP4950_VAR),
    HP4950_SHARED(49, HPCLANG_KEY_HP4950_STO),
    HP4950_SHARED(50, HPCLANG_KEY_HP4950_NXT),
    EM(56, HPCLANG_KEY_ON, HPCLANG_KEYBOARD_MOD_ON),
    HP4950_SHARED_MOD(57, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA),
    HP4950_SHARED_MOD(
        58, HPCLANG_KEY_LEFT_SHIFT,
        HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_LEFT_SHIFT),
    HP4950_SHARED_MOD(
        59, HPCLANG_KEY_RIGHT_SHIFT,
        HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT),
};

#undef HP4950_SHARED_MOD
#undef HP4950_SHARED
#undef EM
#undef E

static const struct expected_key_event *expected_keyboard(
    uint32_t model,
    size_t *count)
{
    *count = 51u;
    if (model == HPCLANG_MODEL_HP39GS || model == HPCLANG_MODEL_HP40GS)
        return hp39_keyboard;
    if (model == HPCLANG_MODEL_HP48GII)
        return hp48_keyboard;
    if (model == HPCLANG_MODEL_HP49GPLUS)
        return hp49_keyboard;
    if (model == HPCLANG_MODEL_HP50G)
        return hp50_keyboard;
    *count = 0u;
    return NULL;
}

static int keyboard_transition(uint32_t model)
{
    hpclang_keyboard_passed_bits = 0u;
    hpclang_keyboard_failed_scan = 0xffu;
    hpclang_keyboard_passed_count = 0u;

    struct hpclang_keyboard_state raw;
    hpclang_keyboard_read(&raw);
    if (hpclang_keyboard_any_is_down(&raw))
        return 0;

    hpclang_keyboard_events_set_timing(5000u, 500000u, 0u);
    hpclang_keyboard_events_reset();

    size_t count = 0u;
    const struct expected_key_event *expected = expected_keyboard(model, &count);
    if (expected == NULL || count != 51u)
        return 0;

    struct hpclang_surface *surface = hpclang_display_surface();
    draw_keyboard_result(surface, 0);

    size_t index = 0u;
    int awaiting_up = 0;
    /*
     * x50ng injects E2E keys from its realtime clock while this timeout uses
     * the calculator's stock Timer0. Their rates can differ substantially in
     * emulation, so bound inactivity between accepted events instead of the
     * total 51-key host-realtime sequence.
     */
    uint64_t deadline = hpclang_time_monotonic_us() + UINT64_C(12000000);

    while (index < count && hpclang_time_monotonic_us() < deadline) {
        struct hpclang_keyboard_event event;
        if (!hpclang_keyboard_event_poll(&event))
            continue;

        const struct expected_key_event *want = &expected[index];
        if (!awaiting_up) {
            if (event.type != HPCLANG_KEYBOARD_EVENT_DOWN ||
                event.scan_code != want->scan ||
                event.key != want->key ||
                event.modifiers != want->down_modifiers) {
                hpclang_keyboard_failed_scan = want->scan;
                return 0;
            }
            awaiting_up = 1;
            deadline = hpclang_time_monotonic_us() + UINT64_C(12000000);
            continue;
        }

        if (event.type != HPCLANG_KEYBOARD_EVENT_UP ||
            event.scan_code != want->scan ||
            event.key != want->key ||
            event.modifiers != 0u) {
            hpclang_keyboard_failed_scan = want->scan;
            return 0;
        }

        hpclang_keyboard_passed_bits |= UINT64_C(1) << want->scan;
        ++hpclang_keyboard_passed_count;
        draw_keyboard_result(surface, 0);
        awaiting_up = 0;
        ++index;
        deadline = hpclang_time_monotonic_us() + UINT64_C(12000000);
    }

    if (index != count || awaiting_up) {
        if (index < count)
            hpclang_keyboard_failed_scan = expected[index].scan;
        return 0;
    }

    return hpclang_keyboard_passed_count == 51u;
}
#endif

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    if (!supported_model(boot->model) || boot->rw_bytes < 0x4000u)
        return finish(0x01, boot->model);

    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL ||
        device->version != HPCLANG_DEVICE_INFO_VERSION ||
        device->size < sizeof(*device) ||
        device->model != boot->model ||
        device->family != expected_family(boot->model) ||
        device->display_width != 131u ||
        device->display_height != (uint32_t)display_height(boot->model))
        return finish(0x0e, boot->model);

    struct hpclang_surface *surface = hpclang_display_surface();
    if (hpclang_surface_width(surface) != device->display_width ||
        hpclang_surface_height(surface) != device->display_height ||
        hpclang_surface_levels(surface) != 2u)
        return finish(0x11, boot->model);

    uint64_t time_before = hpclang_time_monotonic_us();
    hpclang_delay_us(3000u);
    uint64_t time_after = hpclang_time_monotonic_us();
    if (time_after < time_before ||
        time_after - time_before < 3000u ||
        time_after - time_before > 200000u)
        return finish(0x0f, boot->model);

    if (hpclang_e2e_cpp_test() != 0)
        return finish(0x12, boot->model);

#ifdef HPCLANG_E2E_STORAGE
    uint32_t storage_failure = stock_stream_storage_test(boot->model);
    if (storage_failure != 0u)
        return finish(storage_failure, boot->model);
#endif

#ifdef HPCLANG_E2E_KEYBOARD
    if (!keyboard_transition(boot->model))
        return finish(0x10, boot->model);
#endif

    if (hpclang_initialized_probe != 0x13579bdfu)
        return finish(0x02, boot->model);

    if (hpclang_bss_probe != 0u)
        return finish(0x03, boot->model);

    if (hpclang_tls_initialized != 0x2468ace0u || hpclang_tls_zero != 0u)
        return finish(0x04, boot->model);
    hpclang_tls_initialized ^= 0xffffffffu;
    hpclang_tls_zero = 0x11223344u;
    if (hpclang_tls_initialized != 0xdb97531fu ||
        hpclang_tls_zero != 0x11223344u)
        return finish(0x05, boot->model);

    if (hpclang_ctor_probe != 0xc0def00du)
        return finish(0x06, boot->model);
    uint32_t q32 = hpclang_u32_a / hpclang_u32_b;
    if (q32 != 10309u)
        return finish(0x07, boot->model);
    uint64_t q64 = hpclang_u64_a / hpclang_u64_b;
    if (q64 != UINT64_C(1311764532170))
        return finish(0x08, boot->model);
    volatile double mul = hpclang_double_a * hpclang_double_b;
    if (!(mul > 4.37 && mul < 4.38))
        return finish(0x09, boot->model);

    volatile double fp = mul + 0.5;
    if (!(fp > 4.87))
        return finish(0x09, boot->model);
    if (!(fp < 4.88))
        return finish(0x09, boot->model);
    char src[] = "hpclang";
    char copy[16];
    memset(copy, 0, sizeof(copy));
    memcpy(copy, src, sizeof(src));
    if (strlen(copy) != 7u || strcmp(copy, "hpclang") != 0)
        return finish(0x0a, boot->model);
    char *heap = malloc(24);
    if (heap == NULL)
        return finish(0x0b, boot->model);
    memset(heap, 0, 24);
    memcpy(heap, copy, sizeof(src));
    if (strcmp(heap, "hpclang") != 0)
        return finish(0x0c, boot->model);
    char formatted[64];
    int n = snprintf(
        formatted,
        sizeof(formatted),
        "%s:%lu:%llu",
        copy,
        (unsigned long)q32,
        (unsigned long long)q64);
    if (n <= 0 ||
        strcmp(formatted, "hpclang:10309:1311764532170") != 0)
        return finish(0x0d, boot->model);
    free(heap);
    return finish(0, boot->model);
}
