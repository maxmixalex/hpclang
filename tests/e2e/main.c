#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fsystem.h>
#include <hpstdio.h>
#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/keyboard.h>
#include <hpclang/object.h>
#include <hpclang/object-store.h>
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

__attribute__((constructor)) static void hpclang_runtime_ctor(void)
{
    hpclang_ctor_probe = 0xc0def00du;
}

static void text(struct hpclang_surface *surface, int x, int y, const char *s)
{
    hpclang_surface_draw_text(surface, hpclang_font_mini(), x, y, s, 255u);
}

static void hex2(struct hpclang_surface *surface, int x, int y, uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    const struct hpclang_font *font = hpclang_font_mini();

    hpclang_surface_draw_char(surface, font, x, y, (uint8_t)digits[(value >> 4) & 15u], 255u);
    hpclang_surface_draw_char(surface, font, x + (int)hpclang_font_advance(font), y,
                              (uint8_t)digits[value & 15u], 255u);
}

static void result_text(struct hpclang_surface *surface, int x, int y, const char *s)
{
    hpclang_surface_draw_text(surface, &hpclang_e2e_ui_font, x, y, s, 255u);
}

static void result_hex2(struct hpclang_surface *surface, int x, int y, uint32_t value)
{
    const struct hpclang_font *font = &hpclang_e2e_ui_font;
    hpclang_surface_draw_char(surface, font, x, y,
                              (uint8_t)hpclang_e2e_hex_digits[(value >> 4) & 15u], 255u);
    hpclang_surface_draw_char(surface, font, x + (int)hpclang_font_advance(font), y,
                              (uint8_t)hpclang_e2e_hex_digits[value & 15u], 255u);
}

#ifdef HPCLANG_E2E_KEYBOARD
static int keyboard_scan_passed(uint8_t scan)
{
    return (hpclang_keyboard_passed_bits & (UINT64_C(1) << scan)) != 0u;
}

static void keyboard_rect(struct hpclang_surface *surface, int x, int y, int w, int h, int fill)
{
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            int corner = (xx == 0 || xx == w - 1) && (yy == 0 || yy == h - 1);
            if (corner)
                continue;
            if (fill || yy == 0 || yy == h - 1 || xx == 0 || xx == w - 1)
                hpclang_surface_set_pixel(surface, x + xx, y + yy, 255u);
        }
    }
}

static void keyboard_cross(struct hpclang_surface *surface, int x, int y, int w, int h)
{
    int cx = x + w / 2;
    int cy = y + h / 2;

    for (int i = -1; i <= 1; ++i) {
        hpclang_surface_set_pixel(surface, cx + i, cy + i, 255u);
        hpclang_surface_set_pixel(surface, cx + i, cy - i, 255u);
    }
}

static void keyboard_cell(struct hpclang_surface *surface, uint8_t scan, int x, int y, int w, int h)
{
    int passed = keyboard_scan_passed(scan);
    keyboard_rect(surface, x, y, w, h, passed);
    if (!passed && hpclang_keyboard_failed_scan == scan)
        keyboard_cross(surface, x, y, w, h);
}

static void keyboard_cursor_cell(struct hpclang_surface *surface, uint8_t scan, int cx, int cy)
{
    int passed = keyboard_scan_passed(scan);

    for (int yy = -3; yy <= 3; ++yy) {
        int vertical = yy < 0 ? -yy : yy;
        int half_width = vertical == 3 ? 1 : vertical == 2 ? 2 : 3;

        for (int xx = -half_width; xx <= half_width; ++xx) {
            int boundary = vertical == 3 || xx == -half_width || xx == half_width;
            if (passed || boundary)
                hpclang_surface_set_pixel(surface, cx + xx, cy + yy, 255u);
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
    {28u, 21u, 14u, 7u, 0u},  {29u, 22u, 15u, 8u, 1u},  {30u, 23u, 16u, 9u, 2u},
    {57u, 24u, 17u, 10u, 3u}, {58u, 25u, 18u, 11u, 4u}, {59u, 26u, 19u, 12u, 5u},
    {56u, 27u, 20u, 13u, 6u},
};

static void draw_keyboard_result(struct hpclang_surface *surface, int pass)
{
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(surface, &hpclang_e2e_ui_font, 2, 1, hpclang_e2e_font_label, 255u);

    if (pass) {
        text(surface, 99, 1, "PASS 51");
    } else if (hpclang_keyboard_failed_scan != 0xffu) {
        text(surface, 99, 1, "FAIL");
        hex2(surface, 116, 1, hpclang_keyboard_failed_scan);
    } else {
        char progress[8];
        (void)snprintf(progress, sizeof(progress), "KEY %02u", hpclang_keyboard_passed_count);
        text(surface, 105, 1, progress);
    }

    for (int col = 0; col < 6; ++col)
        keyboard_cell(surface, keyboard_soft_scans[col], 4 + col * 21, 7, 17, 4);

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col)
            keyboard_cell(surface, keyboard_control_scans[row][col], 4 + col * 26, 13 + row * 8, 20,
                          5);
    }

    keyboard_cursor_cell(surface, 42u, 105, 14);
    keyboard_cursor_cell(surface, 43u, 96, 20);
    keyboard_cursor_cell(surface, 45u, 114, 20);
    keyboard_cursor_cell(surface, 44u, 105, 26);

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col)
            keyboard_cell(surface, keyboard_main_scans[row][col], 4 + col * 26, 30 + row * 5, 19,
                          4);
    }
}
#endif

static int display_height(uint32_t model)
{
    return model == HPCLANG_MODEL_HP49GPLUS || model == HPCLANG_MODEL_HP50G ? 80 : 64;
}

#ifdef HPCLANG_E2E_STORAGE
static uint32_t storage_metadata_test(void)
{
    const char *path = "META/flags.txt";
    FS_FILE *file = NULL;
    if (FSMkdir("META") != FS_OK ||
        FSCreate("META/flags.txt", FSATTR_RDONLY | FSATTR_HIDDEN | FSATTR_SYSTEM, &file) != FS_OK)
        return 0xb0u;
    if (FSWrite("metadata", 8, file) != 8 || FSClose(file) != FS_OK)
        return 0xb1u;
    struct hpclang_stream_path_info before, after;
    if (hpclang_stream_path_query(path, &before) != 0 || before.byte_size != 8u ||
        before.attributes !=
            (HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY | HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN |
             HPCLANG_STREAM_PATH_ATTRIBUTE_SYSTEM | HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE) ||
        !(before.time_validity & HPCLANG_STREAM_PATH_CREATED_VALID) ||
        (before.time_validity & HPCLANG_STREAM_PATH_LAST_ACCESSED_VALID))
        return 0xb2u;
    errno = 0;
    int fd = open(path, O_WRONLY);
    if (fd >= 0 || errno != EACCES)
        return 0xb3u;
    if (FSOpen("META/flags.txt", FSMODE_READ, &file) != FS_OK)
        return 0xb4u;
    struct tm created, accessed;
    FSGetCreatTime(file, &created);
    FSGetAccessDate(file, &accessed);
    if (created.tm_year != (int)before.created.year - 1900 ||
        created.tm_mon != (int)before.created.month - 1 || created.tm_mday != before.created.day ||
        created.tm_hour != before.created.hour || created.tm_min != before.created.minute ||
        created.tm_sec != before.created.second || accessed.tm_year != 0 || accessed.tm_mon != 0 ||
        accessed.tm_mday != 0 || FSClose(file) != FS_OK)
        return 0xb5u;
    FS_FILE *dir = NULL;
    FS_FILE entry;
    if (FSOpenDir("META", &dir) != FS_OK || FSGetNextEntry(&entry, dir) != FS_OK ||
        FSClose(dir) != FS_OK)
        return 0xb6u;
    // Entry owns its mutation path even after its iterator has been closed.
    if (FSChAttr(&entry, FSATTR_HIDDEN) != FS_OK || FSAttr(&entry) != FSATTR_HIDDEN)
        return 0xb7u;
    FSGetCreatTime(&entry, &created);
    if (created.tm_year != (int)before.created.year - 1900)
        return 0xb8u;
    FSReleaseEntry(&entry);
    if (hpclang_stream_path_query(path, &after) != 0 ||
        after.attributes != HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN ||
        memcmp(&before.created, &after.created, sizeof(before.created)) != 0)
        return 0xb9u;
    errno = 0;
    if (hpclang_stream_path_set_attributes(path, 0u, 0x100u) != -1 || errno != EINVAL)
        return 0xbau;
    if (FSOpen("META/flags.txt", FSMODE_MODIFY | FSMODE_NOCREATE, &file) != FS_OK ||
        FSWrite("M", 1, file) != 1 || FSClose(file) != FS_OK)
        return 0xbbu;
    if (hpclang_stream_path_set_attributes(path, 0u, HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN) != 0 ||
        hpclang_stream_path_query(path, &after) != 0 ||
        after.attributes != HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE ||
        FSDelete("META/flags.txt") != FS_OK || FSRmdir("META") != FS_OK)
        return 0xbcu;
    return 0u;
}

static uint32_t stock_stream_storage_test(uint32_t model)
{
    if (model != HPCLANG_MODEL_HP49GPLUS && model != HPCLANG_MODEL_HP50G) {
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
        if (stat("HPCLANG.TST", &unavailable_stat) == 0 || errno != ENOSYS)
            return 0x53u;

        struct hpclang_stream_path_info unavailable_path;
        errno = 0;
        if (hpclang_stream_path_query("HPCLANG.TST", &unavailable_path) == 0 || errno != ENOSYS)
            return 0x54u;

        errno = 0;
        if (hpclang_stream_path_set_attributes("HPCLANG.TST", 0u, 1u) != -1 || errno != ENOSYS)
            return 0xbdu;

        struct hpclang_stream_storage_info unavailable_storage;
        errno = 0;
        if (hpclang_stream_storage_query(&unavailable_storage) == 0 || errno != ENOSYS)
            return 0x55u;

        uint64_t unavailable_free = 0u;
        errno = 0;
        if (hpclang_stream_storage_free_bytes(&unavailable_free) == 0 || errno != ENOSYS)
            return 0x56u;

        errno = 0;
        if (rename("HPCLANG.A", "HPCLANG.B") == 0 || errno != ENOSYS)
            return 0x57u;

        errno = 0;
        if (mkdir("M7DIR", 0777) == 0 || errno != ENOSYS)
            return 0x70u;

        errno = 0;
        struct hpclang_stream_directory *unavailable_directory = hpclang_stream_directory_open("/");
        if (unavailable_directory != NULL || errno != ENOSYS) {
            if (unavailable_directory != NULL)
                (void)hpclang_stream_directory_close(unavailable_directory);
            return 0x71u;
        }

        return 0u;
    }

    static const char input_expected[] = "HPCLANG-STORAGE\n";
    static const char output_expected[] = "stream-write-seek";
    char buffer[32];

    /*
     * The first byte-stream operation acquires the process-wide owned FatFs
     * namespace. Missing paths must immediately expose normal POSIX ENOENT
     * semantics without routing ordinary file I/O through Stock KOS.
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
        path_info.size < sizeof(path_info) || path_info.type != HPCLANG_STREAM_PATH_TYPE_FILE ||
        path_info.byte_size != sizeof(input_expected) - 1u)
        return 0x59u;

    struct stat input_info;
    if (stat("HPCLANG.TST", &input_info) != 0)
        return 0x5au;
    if (!S_ISREG(input_info.st_mode) || input_info.st_size != (off_t)(sizeof(input_expected) - 1u))
        return 0x5bu;

    if ((path_info.time_validity & HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID) == 0u ||
        path_info.last_modified.year != 2024u || path_info.last_modified.month != 1u ||
        path_info.last_modified.day != 2u || path_info.last_modified.hour != 3u ||
        path_info.last_modified.minute != 4u || path_info.last_modified.second != 6u)
        return 0x69u;

    /*
     * The FAT volume root is a directory even though it has no ordinary
     * directory entry of its own. Exercise it separately from a named
     * directory witness.
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
     * x50ng's host-directory SD fixture exposes the ordinary FAT archive bit
     * on this pre-existing file. Preserve that stable FAT metadata contract.
     */
    if ((path_info.attributes & HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE) == 0u)
        return 0x67u;

    struct hpclang_stream_storage_info storage_info;
    if (hpclang_stream_storage_query(&storage_info) != 0)
        return 0x5du;
    if (storage_info.version != HPCLANG_STREAM_STORAGE_INFO_VERSION ||
        storage_info.size < sizeof(storage_info) ||
        (storage_info.flags & HPCLANG_STREAM_STORAGE_MEDIA_PRESENT) == 0u ||
        (storage_info.flags & HPCLANG_STREAM_STORAGE_FREE_BYTES_VALID) == 0u ||
        (storage_info.flags & HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID) == 0u ||
        storage_info.free_bytes == 0u || storage_info.capacity_bytes == 0u)
        return 0x5eu;
    if (storage_info.capacity_bytes < storage_info.free_bytes)
        return 0x5fu;

    uint64_t free_bytes = 0u;
    if (hpclang_stream_storage_free_bytes(&free_bytes) != 0 ||
        free_bytes != storage_info.free_bytes)
        return 0x60u;

    FILE *input = fopen("HPCLANG.TST", "rb");
    if (input == NULL)
        return 0x33u;

    memset(buffer, 0, sizeof(buffer));
    size_t got = fread(buffer, 1u, sizeof(input_expected) - 1u, input);
    int input_close = fclose(input);
    if (got != sizeof(input_expected) - 1u || input_close != 0 ||
        memcmp(buffer, input_expected, sizeof(input_expected) - 1u) != 0)
        return 0x34u;

    FILE *output = fopen("HPCLANG.OUT", "w+b");
    if (output == NULL)
        return 0x35u;

    size_t written = fwrite(output_expected, 1u, sizeof(output_expected) - 1u, output);
    if (written != sizeof(output_expected) - 1u || fflush(output) != 0) {
        fclose(output);
        return 0x36u;
    }

    if (fseek(output, 7L, SEEK_SET) != 0 || ftell(output) != 7L) {
        fclose(output);
        return 0x37u;
    }

    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, 5u, output);
    if (got != 5u || memcmp(buffer, "write", 5u) != 0 || ftell(output) != 12L) {
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

    if (output_close != 0 || got != sizeof(output_expected) - 1u ||
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
     * "w" must create a genuinely missing file and preserve standard
     * O_CREAT|O_TRUNC semantics through the FatFs adapter.
     */
    static const char create_expected[] = "new-file";
    output = fopen("HPCLANG.NEW", "w+b");
    if (output == NULL)
        return 0x45u;
    written = fwrite(create_expected, 1u, sizeof(create_expected) - 1u, output);
    if (written != sizeof(create_expected) - 1u || fclose(output) != 0)
        return 0x46u;

    output = fopen("HPCLANG.NEW", "rb");
    if (output == NULL)
        return 0x47u;
    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(buffer), output);
    output_close = fclose(output);
    if (output_close != 0 || got != sizeof(create_expected) - 1u ||
        memcmp(buffer, create_expected, sizeof(create_expected) - 1u) != 0)
        return 0x48u;

    /*
     * Namespace mutation, stdio and directory traversal all share the same
     * already-owned FatFs namespace for the rest of the native process.
     */
    if (rename("HPCLANG.NEW", "HPCLANG.REN") != 0)
        return 0x61u;

    output = fopen("HPCLANG.REN", "rb");
    if (output == NULL)
        return 0x62u;
    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(buffer), output);
    output_close = fclose(output);
    if (output_close != 0 || got != sizeof(create_expected) - 1u ||
        memcmp(buffer, create_expected, sizeof(create_expected) - 1u) != 0)
        return 0x63u;

    if (mkdir("M7DIR", 0777) != 0)
        return 0x64u;
    if (rename("HPCLANG.REN", "M7DIR/MiXeD Long Name.txt") != 0)
        return 0x65u;

    struct hpclang_stream_directory *directory = hpclang_stream_directory_open("M7DIR");
    if (directory == NULL)
        return 0x66u;

    int found_long_name = 0;
    for (;;) {
        struct hpclang_stream_directory_entry entry;
        int next = hpclang_stream_directory_read(directory, &entry);
        if (next < 0) {
            (void)hpclang_stream_directory_close(directory);
            return 0x72u;
        }
        if (next == 0)
            break;
        if (strcmp(entry.name, "MiXeD Long Name.txt") == 0) {
            if (entry.version != HPCLANG_STREAM_DIRECTORY_ENTRY_VERSION ||
                entry.size < sizeof(entry) || entry.type != HPCLANG_STREAM_PATH_TYPE_FILE ||
                entry.byte_size != sizeof(create_expected) - 1u) {
                (void)hpclang_stream_directory_close(directory);
                return 0x73u;
            }
            found_long_name = 1;
        }
    }
    if (!found_long_name) {
        (void)hpclang_stream_directory_close(directory);
        return 0x74u;
    }

    /*
     * The same process-wide FAT namespace must serve normal stdio while an
     * iterator is live; directory traversal must not make fopen() busy.
     */
    output = fopen("M7DIR/mixed long name.TXT", "rb");
    if (output == NULL) {
        (void)hpclang_stream_directory_close(directory);
        return 0x7eu;
    }
    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(buffer), output);
    output_close = fclose(output);
    if (output_close != 0 || got != sizeof(create_expected) - 1u ||
        memcmp(buffer, create_expected, sizeof(create_expected) - 1u) != 0) {
        (void)hpclang_stream_directory_close(directory);
        return 0x7fu;
    }

    /*
     * A second iterator has independent FatFs DIR state. Case-insensitive FAT
     * lookup must not alter the case preserved by enumeration above.
     */
    struct hpclang_stream_directory *root_directory = hpclang_stream_directory_open("/");
    if (root_directory == NULL) {
        (void)hpclang_stream_directory_close(directory);
        return 0x75u;
    }
    struct hpclang_stream_directory_entry root_entry;
    if (hpclang_stream_directory_read(root_directory, &root_entry) < 0 ||
        hpclang_stream_directory_close(root_directory) != 0) {
        (void)hpclang_stream_directory_close(directory);
        return 0x76u;
    }

    struct hpclang_stream_path_info direct_path_info;
    if (hpclang_stream_path_query("M7DIR/mixed long name.TXT", &direct_path_info) != 0 ||
        direct_path_info.type != HPCLANG_STREAM_PATH_TYPE_FILE ||
        direct_path_info.byte_size != sizeof(create_expected) - 1u) {
        (void)hpclang_stream_directory_close(directory);
        return 0x77u;
    }

    struct hpclang_stream_storage_info direct_storage_info;
    if (hpclang_stream_storage_query(&direct_storage_info) != 0 ||
        (direct_storage_info.flags & HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID) == 0u ||
        direct_storage_info.capacity_bytes == 0u ||
        direct_storage_info.capacity_bytes < direct_storage_info.free_bytes) {
        (void)hpclang_stream_directory_close(directory);
        return 0x78u;
    }

    if (hpclang_stream_directory_close(directory) != 0)
        return 0x79u;

    if (rename("M7DIR/MiXeD Long Name.txt", "HPCLANG.NEW") != 0)
        return 0x7au;
    if (rmdir("M7DIR") != 0)
        return 0x7bu;

    output = fopen("HPCLANG.NEW", "rb");
    if (output == NULL)
        return 0x7cu;
    if (fclose(output) != 0)
        return 0x7du;

    if (remove("HPCLANG.NEW") != 0)
        return 0x49u;

    /*
     * Standard fopen("a") must create a missing file and every later append
     * must preserve existing contents. This retains the application behavior
     * HPGCC users expected without inheriting the historical KOS append bug.
     */
    output = fopen("HPCLANG.APP", "ab");
    if (output == NULL)
        return 0x4au;
    if (fwrite("one", 1u, 3u, output) != 3u || fclose(output) != 0)
        return 0x4bu;

    output = fopen("HPCLANG.APP", "ab");
    if (output == NULL)
        return 0x4cu;
    if (fwrite("-two", 1u, 4u, output) != 4u || fclose(output) != 0)
        return 0x4du;

    output = fopen("HPCLANG.APP", "rb");
    if (output == NULL)
        return 0x4eu;
    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(buffer), output);
    output_close = fclose(output);
    if (output_close != 0 || got != 7u || memcmp(buffer, "one-two", 7u) != 0)
        return 0x4fu;

    if (remove("HPCLANG.APP") != 0)
        return 0x50u;

    errno = 0;
    if (remove("HPCLANG.APP") == 0)
        return 0x51u;
    if (errno != ENOENT)
        return 0x52u;

    /*
     * POSIX ftruncate must zero-fill an extended range and must not move the
     * logical file offset. FatFs does not provide the zero-fill guarantee by
     * itself, so keep this as a direct regression for the syscall shim.
     */
    int trunc_fd = open("HPCLANG.TRN", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (trunc_fd < 0 || write(trunc_fd, "abc", 3u) != 3 || ftruncate(trunc_fd, 8) != 0 ||
        lseek(trunc_fd, 0, SEEK_CUR) != 3)
        return 0x93u;

    if (lseek(trunc_fd, 0, SEEK_SET) != 0)
        return 0x94u;
    memset(buffer, 0xff, sizeof(buffer));
    if (read(trunc_fd, buffer, 8u) != 8 || memcmp(buffer, "abc", 3u) != 0 || buffer[3] != 0 ||
        buffer[4] != 0 || buffer[5] != 0 || buffer[6] != 0 || buffer[7] != 0)
        return 0x95u;

    if (ftruncate(trunc_fd, 2) != 0 || lseek(trunc_fd, 0, SEEK_CUR) != 8 ||
        lseek(trunc_fd, 0, SEEK_SET) != 0) {
        (void)close(trunc_fd);
        return 0x96u;
    }
    memset(buffer, 0, sizeof(buffer));
    if (read(trunc_fd, buffer, sizeof(buffer)) != 2 || memcmp(buffer, "ab", 2u) != 0 ||
        close(trunc_fd) != 0 || remove("HPCLANG.TRN") != 0)
        return 0x97u;

    /*
     * Representative HPGCC fsystem source-adapter flow. This intentionally
     * uses the historical names/FS_FILE fields while all actual I/O remains
     * on the same hpclang-owned FatFs namespace.
     */
    FS_FILE *legacy_file = NULL;
    if (FSOpen(":3:HPCLANG.TST", FSMODE_READ, &legacy_file) != FS_OK || legacy_file == NULL)
        return 0x80u;

    memset(buffer, 0, sizeof(buffer));
    int legacy_read = FSRead(buffer, (int)(sizeof(input_expected) - 1u), legacy_file);
    if (legacy_read != (int)(sizeof(input_expected) - 1u) ||
        memcmp(buffer, input_expected, sizeof(input_expected) - 1u) != 0 ||
        FSTell(legacy_file) != (int)(sizeof(input_expected) - 1u) ||
        FSFileLength(legacy_file) != (int)(sizeof(input_expected) - 1u))
        return 0x81u;

    struct tm legacy_time;
    FSGetWriteTime(legacy_file, &legacy_time);
    if (legacy_time.tm_year != 124 || legacy_time.tm_mon != 0 || legacy_time.tm_mday != 2 ||
        legacy_time.tm_hour != 3 || legacy_time.tm_min != 4 || legacy_time.tm_sec != 6 ||
        FSClose(legacy_file) != FS_OK)
        return 0x82u;

    legacy_file = NULL;
    if (FSCreate("HPCLANG.CMP", FSATTR_ARCHIVE, &legacy_file) != FS_OK || legacy_file == NULL ||
        FSWrite("legacy", 6, legacy_file) != 6 || FSSeek(legacy_file, 2, SEEK_SET) != FS_OK ||
        FSWrite("X", 1, legacy_file) != 1 || FSFileLength(legacy_file) != 3 ||
        FSClose(legacy_file) != FS_OK)
        return 0x83u;

    legacy_file = NULL;
    if (FSOpen("HPCLANG.CMP", FSMODE_READ, &legacy_file) != FS_OK || legacy_file == NULL)
        return 0x84u;
    memset(buffer, 0, sizeof(buffer));
    if (FSRead(buffer, 6, legacy_file) != 3 || memcmp(buffer, "leX", 3u) != 0 ||
        !FSEof(legacy_file) || FSClose(legacy_file) != FS_OK)
        return 0x85u;

    if (FSMkdir("CMPDIR") != FS_OK || FSChdir("CMPDIR") != FS_OK)
        return 0x86u;

    char *legacy_cwd = FSGetcwd(0);
    if (legacy_cwd == NULL || strcmp(legacy_cwd, "\\CMPDIR") != 0) {
        free(legacy_cwd);
        return 0x87u;
    }
    free(legacy_cwd);

    if (FSRename("/HPCLANG.CMP", "Legacy Long Name.txt") != FS_OK)
        return 0x88u;

    FS_FILE *legacy_dir = NULL;
    if (FSOpenDir(".", &legacy_dir) != FS_OK || legacy_dir == NULL)
        return 0x89u;

    int legacy_found = 0;
    for (;;) {
        FS_FILE legacy_entry;
        int legacy_next = FSGetNextEntry(&legacy_entry, legacy_dir);
        if (legacy_next == FS_EOF)
            break;
        if (legacy_next != FS_OK) {
            (void)FSClose(legacy_dir);
            return 0x8au;
        }

        if (legacy_entry.Name != NULL && strcmp(legacy_entry.Name, "Legacy Long Name.txt") == 0) {
            if (legacy_entry.FileSize != 3u || (FSAttr(&legacy_entry) & FSATTR_DIR) != 0) {
                FSReleaseEntry(&legacy_entry);
                (void)FSClose(legacy_dir);
                return 0x8bu;
            }
            legacy_found = 1;
        }
        FSReleaseEntry(&legacy_entry);
    }

    if (!legacy_found || FSClose(legacy_dir) != FS_OK)
        return 0x8cu;
    if (FSChdir("/") != FS_OK)
        return 0x8du;
    if (!FSVolumeMounted(0) || FSVolumeInserted(0) != FS_OK || FSGetCurrentVolume() != 0 ||
        FSSetCurrentVolume(1) != FS_BADVOLUME)
        return 0x8eu;

    int legacy_volume_size = FSGetVolumeSize(0);
    int legacy_volume_free = FSGetVolumeFree(0);
    if (legacy_volume_size <= 0 || legacy_volume_free < 0 ||
        legacy_volume_free > legacy_volume_size)
        return 0x92u;

    if (!FSNameCompare("Name.TXT", "name.txt", FSCASE_INSENS) ||
        FSNameCompare("Name.TXT", "name.txt", FSCASE_SENS) ||
        !FSNameCompare("Name.TXT;;", "Name.TXT", FSCASE_SENSHP))
        return 0x8fu;

    int hp_name = FSGetNameType(":3:dir/file.txt");
    if (hp_name < 0 ||
        (hp_name & (FSNAME_HASVOL | FSNAME_VOLHP | FSNAME_HASPATH)) !=
            (FSNAME_HASVOL | FSNAME_VOLHP | FSNAME_HASPATH) ||
        (hp_name >> 16) != 1 || FSGetNameType("C:\\dir\\\\file") != FSNAME_INVALID)
        return 0x90u;

    if (FSRename("CMPDIR/Legacy Long Name.txt", "HPCLANG.CMP") != FS_OK ||
        FSRmdir("CMPDIR") != FS_OK || fdelete("HPCLANG.CMP") != 0)
        return 0x91u;

    /*
     * Direct namespace ownership remains coherent through ordinary stream
     * work. The runtime releases it and invokes Stock ResetSD only after
     * destructors, immediately before returning to the calculator OS.
     */
    FILE *reset_input = fopen("HPCLANG.TST", "rb");
    if (reset_input == NULL)
        return 0x6du;
    memset(buffer, 0, sizeof(buffer));
    got = fread(buffer, 1u, sizeof(input_expected) - 1u, reset_input);
    input_close = fclose(reset_input);
    if (got != sizeof(input_expected) - 1u || input_close != 0 ||
        memcmp(buffer, input_expected, sizeof(input_expected) - 1u) != 0)
        return 0x6eu;

    return storage_metadata_test();
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

static void hold_failure_result(void)
{
    /*
     * FAIL is terminal for automated E2E. Keep the temporary runtime mapping
     * alive until the harness observes FAIL_PREFIX and stops the emulator.
     * A finite nop delay can be shorter than the Stock harness poll interval,
     * causing the failure word to disappear during launcher teardown and
     * degrading a precise failure into an uninformative timeout.
     */
    for (;;)
        __asm__ volatile("nop");
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
    hold_failure_result();
    return (int)failure;
}

static int supported_model(uint32_t model)
{
    return model >= HPCLANG_MODEL_HP39GPLUS && model <= HPCLANG_MODEL_HP50G;
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
struct expected_key_event
{
    uint8_t scan;
    uint16_t key;
    uint32_t down_modifiers;
};

#define E(scan, key) {scan##u, key, 0u}
#define EM(scan, key, mods) {scan##u, key, mods}

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
    EM(58, HPCLANG_KEY_LEFT_SHIFT, HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_LEFT_SHIFT),
    EM(59, HPCLANG_KEY_RIGHT_SHIFT, HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT),
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
    HP4950_SHARED_MOD(58, HPCLANG_KEY_LEFT_SHIFT,
                      HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_LEFT_SHIFT),
    HP4950_SHARED_MOD(59, HPCLANG_KEY_RIGHT_SHIFT,
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
    HP4950_SHARED_MOD(58, HPCLANG_KEY_LEFT_SHIFT,
                      HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_LEFT_SHIFT),
    HP4950_SHARED_MOD(59, HPCLANG_KEY_RIGHT_SHIFT,
                      HPCLANG_KEYBOARD_MOD_SHIFT | HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT),
};

#undef HP4950_SHARED_MOD
#undef HP4950_SHARED
#undef EM
#undef E

static const struct expected_key_event *expected_keyboard(uint32_t model, size_t *count)
{
    *count = 51u;
    if (model == HPCLANG_MODEL_HP39GPLUS || model == HPCLANG_MODEL_HP39GS ||
        model == HPCLANG_MODEL_HP40GS)
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
            if (event.type != HPCLANG_KEYBOARD_EVENT_DOWN || event.scan_code != want->scan ||
                event.key != want->key || event.modifiers != want->down_modifiers) {
                hpclang_keyboard_failed_scan = want->scan;
                return 0;
            }
            awaiting_up = 1;
            deadline = hpclang_time_monotonic_us() + UINT64_C(12000000);
            continue;
        }

        if (event.type != HPCLANG_KEYBOARD_EVENT_UP || event.scan_code != want->scan ||
            event.key != want->key || event.modifiers != 0u) {
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

static uint32_t object_read_only_test(uint32_t model)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    int rpl_family = model == HPCLANG_MODEL_HP48GII || model == HPCLANG_MODEL_HP49GPLUS ||
                     model == HPCLANG_MODEL_HP50G;
    int live_stock_view = rpl_family && boot != NULL && boot->saturn_state != 0u;

    uint32_t depth = UINT32_MAX;
    errno = 0;
    int result = hpclang_stack_depth(&depth);

    if (!live_stock_view) {
        if (result != -1 || depth != 0u || errno != ENOSYS)
            return 0xa0u;
        return 0u;
    }

    if (result != 0)
        return 0xa1u;

    uint32_t before = depth;
    if (before != 0u) {
        struct hpclang_object *object = NULL;
        errno = 0;
        result = hpclang_stack_peek(1u, &object);
        if (result == 0) {
            if (object == NULL)
                return 0xa2u;
            (void)hpclang_object_kind(object);
            hpclang_object_release(object);
        } else {
            /*
             * The first M7.3 slice is deliberately conservative: a Stock
             * object whose exact native boundary is not frozen yet reports
             * ENOTSUP rather than being copied heuristically.
             */
            if (object != NULL || errno != ENOTSUP)
                return 0xa3u;
        }
    }

    uint32_t after = UINT32_MAX;
    if (hpclang_stack_depth(&after) != 0 || after != before)
        return 0xa4u;

    if (before != UINT32_MAX) {
        struct hpclang_object *object = NULL;
        errno = 0;
        if (hpclang_stack_peek(before + 1u, &object) != -1 || object != NULL || errno != EINVAL)
            return 0xa5u;
    }

#ifdef HPCLANG_E2E_KEYBOARD
    /*
     * Build the mutation witness entirely inside HPC1. File Manager and VAR
     * recall can place a pre-existing X-register value above the recalled
     * launcher, so UI-prepared stack arguments are intentionally avoided.
     */
    struct hpclang_object *char_a = NULL;
    struct hpclang_object *char_b = NULL;
    if (hpclang_object_create_character('A', &char_a) != 0 ||
        hpclang_object_create_character('B', &char_b) != 0) {
        hpclang_object_release(char_a);
        hpclang_object_release(char_b);
        return 0xa6u;
    }

    uint8_t character = 0u;
    if (hpclang_object_character_value(char_a, &character) != 0 || character != 'A' ||
        hpclang_object_character_value(char_b, &character) != 0 || character != 'B') {
        hpclang_object_release(char_a);
        hpclang_object_release(char_b);
        return 0xa7u;
    }

    if (live_stock_view) {
        if (hpclang_stack_push(char_a) != 0) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xa8u;
        }

        uint32_t mutated_depth = UINT32_MAX;
        if (hpclang_stack_depth(&mutated_depth) != 0 || mutated_depth != before + 1u) {
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xa9u;
        }

        if (hpclang_stack_replace(1u, char_b) != 0) {
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xaau;
        }

        struct hpclang_object *peek = NULL;
        if (hpclang_stack_peek(1u, &peek) != 0 || peek == NULL ||
            hpclang_object_character_value(peek, &character) != 0 || character != 'B') {
            hpclang_object_release(peek);
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xabu;
        }
        hpclang_object_release(peek);

        if (hpclang_stack_push(char_a) != 0) {
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xacu;
        }

        if (hpclang_stack_depth(&mutated_depth) != 0 || mutated_depth != before + 2u) {
            (void)hpclang_stack_drop(2u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xadu;
        }

        struct hpclang_object *popped = NULL;
        if (hpclang_stack_pop(&popped) != 0 || popped == NULL ||
            hpclang_object_character_value(popped, &character) != 0 || character != 'A') {
            hpclang_object_release(popped);
            (void)hpclang_stack_drop(2u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xaeu;
        }
        hpclang_object_release(popped);

        if (hpclang_stack_depth(&mutated_depth) != 0 || mutated_depth != before + 1u ||
            hpclang_stack_drop(1u) != 0 || hpclang_stack_depth(&mutated_depth) != 0 ||
            mutated_depth != before) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xafu;
        }
    } else {
        errno = 0;
        if (hpclang_stack_push(char_a) != -1 || errno != ENOSYS) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb0u;
        }

        errno = 0;
        if (hpclang_stack_replace(1u, char_b) != -1 || errno != ENOSYS) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb1u;
        }

        struct hpclang_object *popped = NULL;
        errno = 0;
        if (hpclang_stack_pop(&popped) != -1 || popped != NULL || errno != ENOSYS) {
            hpclang_object_release(popped);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb2u;
        }

        errno = 0;
        if (hpclang_stack_drop(1u) != -1 || errno != ENOSYS) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb3u;
        }
    }

    hpclang_object_release(char_a);
    hpclang_object_release(char_b);
#endif

    return 0u;
}

#ifdef HPCLANG_E2E_KEYBOARD
static int object_store_find_named(struct hpclang_object_store_iterator *iterator,
                                   const uint8_t *name, uint32_t name_length,
                                   struct hpclang_object_store_entry *found)
{
    struct hpclang_object_store_entry entry = {
        .size = sizeof(struct hpclang_object_store_entry),
        .version = HPCLANG_OBJECT_STORE_ENTRY_VERSION,
    };

    int result = 0;
    while ((result = hpclang_object_store_iterator_read(iterator, &entry)) > 0) {
        if (entry.name_length == name_length && memcmp(entry.name, name, name_length) == 0) {
            if (found != NULL)
                *found = entry;
            return 1;
        }
    }
    return result;
}

#define HPCLANG_E2E_OBJECT_STORE_PHASE UINT32_C(0x4f530000) /* OS + step */

static void object_store_phase(uint32_t step)
{
    hpclang_e2e_result = HPCLANG_E2E_OBJECT_STORE_PHASE | (step & UINT32_C(0xffff));
}

static uint32_t object_store_service_test(uint32_t model)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    int rpl_family = model == HPCLANG_MODEL_HP48GII || model == HPCLANG_MODEL_HP49GPLUS ||
                     model == HPCLANG_MODEL_HP50G;
    int live_stock_view = rpl_family && boot != NULL && boot->saturn_state != 0u;

    if (!live_stock_view) {
        errno = 0;
        struct hpclang_object_store *store = hpclang_object_store_home();
        if (store != NULL || errno != ENOSYS) {
            hpclang_object_store_close(store);
            return 0xc0u;
        }

        errno = 0;
        store = hpclang_object_store_current();
        if (store != NULL || errno != ENOSYS) {
            hpclang_object_store_close(store);
            return 0xc1u;
        }
        return 0u;
    }

    static const uint8_t e2e_witness_name[] = {'Z', 'Z', 'E', '2', 'E'};
    static const uint8_t e2e_witness_value[] = "HPCLANG-E2E";
    /*
     * x50ng's Stock VVFAT setup exposes a stable empty DOCSTR named
     * X50NG-VV.FAT in USEROB before the payload starts. HPCLANG.HP is the
     * FAT transfer/launcher file and is not a HOME variable, so it is not a
     * valid named-object-store witness on HP49g+/50g.
     */
    static const uint8_t hp4950_witness_name[] = {'X', '5', '0', 'N', 'G', '-',
                                                  'V', 'V', '.', 'F', 'A', 'T'};
    const uint8_t *witness_name =
        model == HPCLANG_MODEL_HP48GII ? e2e_witness_name : hp4950_witness_name;
    uint32_t witness_name_length = model == HPCLANG_MODEL_HP48GII
                                       ? (uint32_t)sizeof(e2e_witness_name)
                                       : (uint32_t)sizeof(hp4950_witness_name);
    int witness_has_e2e_value = model == HPCLANG_MODEL_HP48GII;

    static const uint8_t missing_name[] = {'_', '_', 'H', 'P', 'C', 'L', 'A', 'N', 'G', '_',
                                           'E', '2', 'E', '_', 'M', 'I', 'S', 'S', 'I', 'N',
                                           'G', '_', '8', 'A', '1', '7', '_', '_'};

    uint32_t failure = 0u;
    object_store_phase(1u); /* before HOME snapshot */
    struct hpclang_object_store *home = hpclang_object_store_home();
    struct hpclang_object_store *current = NULL;
    struct hpclang_object_store *child = NULL;
    struct hpclang_object_store_iterator *first = NULL;
    struct hpclang_object_store_iterator *second = NULL;
    struct hpclang_object_store_iterator *current_iterator = NULL;
    struct hpclang_object *witness = NULL;

    if (home == NULL) {
        failure = 0xc2u;
        goto done;
    }

    /*
     * Primary HP49/50 USB Stock runs receive the same stable HOME string
     * witness as HP48gII. The preserved SD/VVFAT secondary gate has no such
     * HOME transfer and keeps using X50NG-VV.FAT. Prefer the stronger USB
     * witness whenever it is present without making transport identity part
     * of the runtime API.
     */
    if (model != HPCLANG_MODEL_HP48GII) {
        struct hpclang_object *candidate = NULL;
        errno = 0;
        if (hpclang_object_store_get(home, e2e_witness_name, sizeof(e2e_witness_name),
                                     &candidate) == 0 &&
            candidate != NULL && hpclang_object_kind(candidate) == HPCLANG_OBJECT_STRING) {
            witness_name = e2e_witness_name;
            witness_name_length = (uint32_t)sizeof(e2e_witness_name);
            witness_has_e2e_value = 1;
        }
        hpclang_object_release(candidate);
    }

    object_store_phase(2u); /* HOME ready */
    object_store_phase(3u); /* before current-directory snapshot */
    current = hpclang_object_store_current();
    if (current == NULL) {
        failure = 0xc3u;
        goto done;
    }

    object_store_phase(4u); /* current path ready */
    object_store_phase(5u); /* before first HOME iterator snapshot */
    first = hpclang_object_store_iterator_open(home);
    if (first == NULL) {
        failure = 0xc4u;
        goto done;
    }
    object_store_phase(6u);
    second = hpclang_object_store_iterator_open(home);
    if (second == NULL) {
        failure = 0xc4u;
        goto done;
    }
    object_store_phase(7u);
    current_iterator = hpclang_object_store_iterator_open(current);
    if (current_iterator == NULL) {
        failure = 0xc4u;
        goto done;
    }

    object_store_phase(8u); /* all iterator snapshots ready */
    struct hpclang_object_store_entry entry1;
    struct hpclang_object_store_entry entry2;
    object_store_phase(9u); /* before first independent lookup */
    int found1 = object_store_find_named(first, witness_name, witness_name_length, &entry1);
    object_store_phase(10u);
    int found2 = object_store_find_named(second, witness_name, witness_name_length, &entry2);
    object_store_phase(11u);
    if (found1 != 1 || found2 != 1) {
        failure = 0xc5u;
        goto done;
    }

    if (entry1.kind != HPCLANG_OBJECT_STRING || entry2.kind != HPCLANG_OBJECT_STRING ||
        (entry1.flags & HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY) != 0u ||
        (entry2.flags & HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY) != 0u) {
        failure = 0xc6u;
        goto done;
    }

    object_store_phase(12u); /* before successful named-object snapshot */
    size_t witness_string_size = 0u;
    const size_t expected_witness_size =
        witness_has_e2e_value ? sizeof(e2e_witness_value) - 1u : 0u;
    errno = 0;
    if (hpclang_object_store_get(home, witness_name, witness_name_length, &witness) != 0 ||
        witness == NULL || hpclang_object_kind(witness) != HPCLANG_OBJECT_STRING ||
        hpclang_object_string_size(witness, &witness_string_size) != 0 ||
        witness_string_size != expected_witness_size) {
        failure = 0xc9u;
        goto done;
    }
    if (witness_has_e2e_value) {
        uint8_t witness_copy[sizeof(e2e_witness_value) - 1u];
        if (hpclang_object_string_copy(witness, witness_copy, sizeof(witness_copy)) != 0 ||
            memcmp(witness_copy, e2e_witness_value, sizeof(witness_copy)) != 0) {
            failure = 0xc9u;
            goto done;
        }
    }

    object_store_phase(13u); /* before ENOTDIR child-open check */
    errno = 0;
    child = hpclang_object_store_open_child(home, witness_name, witness_name_length);
    if (child != NULL || errno != ENOTDIR) {
        failure = 0xc7u;
        goto done;
    }

    object_store_phase(14u); /* before missing-object lookup */
    struct hpclang_object *missing = NULL;
    errno = 0;
    if (hpclang_object_store_get(home, missing_name, sizeof(missing_name), &missing) != -1 ||
        missing != NULL || errno != ENOENT) {
        hpclang_object_release(missing);
        failure = 0xc8u;
        goto done;
    }

    object_store_phase(15u); /* read-only object-store witness complete */

done:
    hpclang_object_release(witness);
    hpclang_object_store_iterator_close(current_iterator);
    hpclang_object_store_iterator_close(second);
    hpclang_object_store_iterator_close(first);
    hpclang_object_store_close(child);
    hpclang_object_store_close(current);
    hpclang_object_store_close(home);
    return failure;
}
#endif

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    if (!supported_model(boot->model) || boot->rw_bytes < 0x4000u)
        return finish(0x01, boot->model);

    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL || device->version != HPCLANG_DEVICE_INFO_VERSION ||
        device->size < sizeof(*device) || device->model != boot->model ||
        device->family != expected_family(boot->model) || device->display_width != 131u ||
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
    if (time_after < time_before || time_after - time_before < 3000u ||
        time_after - time_before > 200000u)
        return finish(0x0f, boot->model);

    if (hpclang_e2e_cpp_test() != 0)
        return finish(0x12, boot->model);

    uint32_t object_failure = object_read_only_test(boot->model);
    if (object_failure != 0u)
        return finish(object_failure, boot->model);

#ifdef HPCLANG_E2E_KEYBOARD

    uint32_t object_store_failure = object_store_service_test(boot->model);
    if (object_store_failure != 0u)
        return finish(object_store_failure, boot->model);
#endif

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
    if (hpclang_tls_initialized != 0xdb97531fu || hpclang_tls_zero != 0x11223344u)
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
    int n = snprintf(formatted, sizeof(formatted), "%s:%lu:%llu", copy, (unsigned long)q32,
                     (unsigned long long)q64);
    if (n <= 0 || strcmp(formatted, "hpclang:10309:1311764532170") != 0)
        return finish(0x0d, boot->model);
    free(heap);
    return finish(0, boot->model);
}
