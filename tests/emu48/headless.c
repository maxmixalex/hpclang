#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef BOOL (CALLBACK *EmuCreateFn)(const void *);
typedef BOOL (CALLBACK *EmuDestroyFn)(void);
typedef HACCEL (CALLBACK *EmuAcceleratorTableFn)(HWND *);
typedef BYTE (CALLBACK *EmuCalculatorTypeFn)(void);
typedef void (CALLBACK *EmuSimulateKeyFn)(BOOL, UINT, UINT);
typedef void (CALLBACK *EmuPressOnFn)(BOOL);
typedef DWORD (CALLBACK *EmuGetRegisterFn)(UINT);
typedef void (CALLBACK *EmuGetMemFn)(DWORD, BYTE *);
typedef BOOL (CALLBACK *EmuRunFn)(void);
typedef BOOL (CALLBACK *EmuStopFn)(void);

struct emu48_api {
    HMODULE module;
    EmuCreateFn create;
    EmuDestroyFn destroy;
    EmuAcceleratorTableFn accelerator_table;
    EmuCalculatorTypeFn calculator_type;
    EmuSimulateKeyFn simulate_key;
    EmuPressOnFn press_on;
    EmuGetRegisterFn get_register;
    EmuGetMemFn get_mem;
    EmuRunFn run;
    EmuStopFn stop;
};

static FARPROC required_stdcall(
    HMODULE module, const char *name, unsigned argument_bytes)
{
    char decorated[128];
    FARPROC proc = GetProcAddress(module, name);

    if (proc != NULL)
        return proc;

    snprintf(decorated, sizeof(decorated), "_%s@%u", name, argument_bytes);
    proc = GetProcAddress(module, decorated);
    if (proc != NULL)
        return proc;

    snprintf(decorated, sizeof(decorated), "%s@%u", name, argument_bytes);
    proc = GetProcAddress(module, decorated);
    if (proc != NULL)
        return proc;

    fprintf(stderr, "missing required Emu48 export: %s (stdcall @%u)\n",
            name, argument_bytes);
    ExitProcess(3);
}

static struct emu48_api load_api(const char *dll_path)
{
    struct emu48_api api;
    memset(&api, 0, sizeof(api));
    api.module = LoadLibraryA(dll_path);
    if (api.module == NULL) {
        fprintf(stderr, "LoadLibraryA failed for %s: error %lu\n",
                dll_path, GetLastError());
        ExitProcess(2);
    }
#define LOAD(name, bytes, field, type) \
    api.field = (type)required_stdcall(api.module, name, bytes)
    LOAD("EmuCreate", 4, create, EmuCreateFn);
    LOAD("EmuDestroy", 0, destroy, EmuDestroyFn);
    LOAD("EmuAcceleratorTable", 4, accelerator_table, EmuAcceleratorTableFn);
    LOAD("EmuCalculatorType", 0, calculator_type, EmuCalculatorTypeFn);
    LOAD("EmuSimulateKey", 12, simulate_key, EmuSimulateKeyFn);
    LOAD("EmuPressOn", 4, press_on, EmuPressOnFn);
    LOAD("EmuGetRegister", 4, get_register, EmuGetRegisterFn);
    LOAD("EmuGetMem", 8, get_mem, EmuGetMemFn);
    LOAD("EmuRun", 0, run, EmuRunFn);
    LOAD("EmuStop", 0, stop, EmuStopFn);
#undef LOAD
    return api;
}

static void unload_api(struct emu48_api *api)
{
    if (api->module != NULL)
        FreeLibrary(api->module);
    memset(api, 0, sizeof(*api));
}

static int write_bmp24(
    const char *path, int width, int height, const uint8_t *rgb)
{
    const uint32_t stride = (uint32_t)((width * 3 + 3) & ~3);
    const uint32_t pixel_bytes = stride * (uint32_t)height;
    const uint32_t file_bytes = 54u + pixel_bytes;
    uint8_t *row = (uint8_t *)calloc(1, stride);
    FILE *f = NULL;

    if (row == NULL)
        return 0;
    f = fopen(path, "wb");
    if (f == NULL) {
        free(row);
        return 0;
    }

    {
        uint8_t fh[14] = {
            'B','M',
            (uint8_t)file_bytes, (uint8_t)(file_bytes >> 8),
            (uint8_t)(file_bytes >> 16), (uint8_t)(file_bytes >> 24),
            0,0,0,0, 54,0,0,0
        };
        uint8_t ih[40] = {
            40,0,0,0,
            (uint8_t)width, (uint8_t)(width >> 8),
            (uint8_t)(width >> 16), (uint8_t)(width >> 24),
            (uint8_t)height, (uint8_t)(height >> 8),
            (uint8_t)(height >> 16), (uint8_t)(height >> 24),
            1,0,24,0
        };
        fwrite(fh, 1, sizeof(fh), f);
        fwrite(ih, 1, sizeof(ih), f);
    }

    for (int y = height - 1; y >= 0; --y) {
        const uint8_t *src = rgb + (size_t)y * (size_t)width * 3u;
        memset(row, 0, stride);
        for (int x = 0; x < width; ++x) {
            row[x * 3 + 0] = src[x * 3 + 2];
            row[x * 3 + 1] = src[x * 3 + 1];
            row[x * 3 + 2] = src[x * 3 + 0];
        }
        fwrite(row, 1, stride, f);
    }

    fclose(f);
    free(row);
    return 1;
}

static int capture_client(HWND hwnd, const char *bmp_path)
{
    RECT rect;
    HDC window_dc = NULL;
    HDC memory_dc = NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    BITMAPINFO bmi;
    uint8_t *pixels = NULL;
    int width, height;
    int ok = 0;

    if (!GetClientRect(hwnd, &rect))
        goto done;
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0)
        goto done;

    window_dc = GetDC(hwnd);
    if (window_dc == NULL)
        goto done;
    memory_dc = CreateCompatibleDC(window_dc);
    if (memory_dc == NULL)
        goto done;
    bitmap = CreateCompatibleBitmap(window_dc, width, height);
    if (bitmap == NULL)
        goto done;
    old_bitmap = SelectObject(memory_dc, bitmap);

    if (!PrintWindow(hwnd, memory_dc, PW_CLIENTONLY) &&
        !BitBlt(memory_dc, 0, 0, width, height,
                window_dc, 0, 0, SRCCOPY))
        goto done;

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    pixels = (uint8_t *)malloc((size_t)width * (size_t)height * 3u);
    if (pixels == NULL)
        goto done;
    if (GetDIBits(memory_dc, bitmap, 0, (UINT)height,
                  pixels, &bmi, DIB_RGB_COLORS) == 0)
        goto done;

    for (size_t i = 0; i < (size_t)width * (size_t)height; ++i) {
        uint8_t b = pixels[i * 3 + 0];
        pixels[i * 3 + 0] = pixels[i * 3 + 2];
        pixels[i * 3 + 2] = b;
    }

    ok = write_bmp24(bmp_path, width, height, pixels);

done:
    free(pixels);
    if (old_bitmap != NULL && memory_dc != NULL)
        SelectObject(memory_dc, old_bitmap);
    if (bitmap != NULL)
        DeleteObject(bitmap);
    if (memory_dc != NULL)
        DeleteDC(memory_dc);
    if (window_dc != NULL)
        ReleaseDC(hwnd, window_dc);
    return ok;
}

static int selftest(const char *bmp_path)
{
    enum { W = 131, H = 64 };
    uint8_t *pixels = (uint8_t *)malloc(W * H * 3u);
    if (pixels == NULL)
        return 2;

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            uint8_t v = (((x / 8) ^ (y / 8)) & 1) ? 32u : 224u;
            size_t p = ((size_t)y * W + x) * 3u;
            pixels[p + 0] = v;
            pixels[p + 1] = v;
            pixels[p + 2] = v;
        }
    }
    for (int x = 0; x < W; ++x) {
        memset(pixels + (size_t)x * 3u, 0, 3);
        memset(pixels + ((size_t)(H - 1) * W + x) * 3u, 0, 3);
    }
    for (int y = 0; y < H; ++y) {
        memset(pixels + (size_t)y * W * 3u, 0, 3);
        memset(pixels + ((size_t)y * W + W - 1) * 3u, 0, 3);
    }

    if (!write_bmp24(bmp_path, W, H, pixels)) {
        free(pixels);
        return 3;
    }
    free(pixels);
    return 0;
}

static int probe(const char *dll_path)
{
    struct emu48_api api = load_api(dll_path);
    printf("{\n");
    printf("  \"dll\": \"%s\",\n", dll_path);
    printf("  \"status\": \"ok\",\n");
    printf("  \"api\": \"create/destroy/key/register/memory/run-stop\"\n");
    printf("}\n");
    unload_api(&api);
    return 0;
}

static int capture_state(
    const char *dll_path, const char *state_path, const char *bmp_path)
{
    struct emu48_api api;
    HWND hwnd = NULL;
    static const wchar_t neutral_state_name[] = L"s";
    int result = 1;

    /*
     * EmuCreate is exported with TCHAR rather than A/W suffixes. The one-letter
     * name "s" works for both ANSI and Unicode builds when passed as L"s".
     */
    if (!CopyFileA(state_path, "s", FALSE)) {
        fprintf(stderr, "failed to copy state fixture to ./s: error %lu\n",
                GetLastError());
        return 2;
    }

    api = load_api(dll_path);
    if (api.create(neutral_state_name)) {
        fprintf(stderr, "EmuCreate failed for state fixture\n");
        result = 3;
        goto done;
    }

    (void)api.accelerator_table(&hwnd);
    if (hwnd == NULL) {
        fprintf(stderr, "Emu48 did not return a window handle\n");
        result = 4;
        goto destroy;
    }

    SetWindowPos(hwnd, NULL, -30000, -30000, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    Sleep(250);

    if (!capture_client(hwnd, bmp_path)) {
        fprintf(stderr, "failed to capture Emu48 client bitmap\n");
        result = 5;
        goto destroy;
    }

    printf("{\"calculator_type\":%u,\"screenshot\":\"%s\"}\n",
           (unsigned)api.calculator_type(), bmp_path);
    result = 0;

destroy:
    api.destroy();
    Sleep(100);
done:
    unload_api(&api);
    DeleteFileA("s");
    return result;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s probe <Emu48.dll>\n"
        "  %s selftest <output.bmp>\n"
        "  %s capture-state <Emu48.dll> <state> <output.bmp>\n",
        argv0, argv0, argv0);
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "probe") == 0)
        return probe(argv[2]);
    if (argc == 3 && strcmp(argv[1], "selftest") == 0)
        return selftest(argv[2]);
    if (argc == 5 && strcmp(argv[1], "capture-state") == 0)
        return capture_state(argv[2], argv[3], argv[4]);
    usage(argv[0]);
    return 64;
}
