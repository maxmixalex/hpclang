/* External observation; original application statements remain in separate TUs. */
#include <fsystem.h>
#include <hpgraphics.h>
#include <hpkeyb49.h>
#include <hpclang/time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int historical_enum_main(void);
int historical_write_main(void);
int __real_keyb_getkey(int wait);
int __real_keyb_getkeyM(int wait);
int __real_FSWrite(char *buffer, int bytes, FS_FILE *file);
int __real_FSClose(FS_FILE *file);
int __real_FSCloseAndDelete(FS_FILE *file);
int __real_FSRename(char *oldname, char *newname);
int __real_FSGetNextEntry(FS_FILE *entry, FS_FILE *directory);

volatile uint32_t hpclang_e2e_result;
static unsigned failure, phase, waits, pages, entries, writes, closes, deletes, renames;
static const char modified[] = "Hello HP49G+\nAppended string";

static void require(int condition, unsigned code)
{
    if (!condition && !failure)
        failure = code;
}
static int mailbox(uint32_t request, uint32_t ack)
{
    uint64_t start = hpclang_time_monotonic_us();
    hpclang_e2e_result = request;
    while (hpclang_e2e_result != ack)
        if (hpclang_time_monotonic_us() - start > 2000000) {
            require(0, 1);
            return -1;
        }
    return 0;
}
static int key(int matrix, int wait)
{
    require(wait == 1 && !keyb_isAnyKeyPressed(), 2);
    unsigned frame = 2300 + waits + pages;
    if (mailbox(UINT32_C(0x46520000) | frame, UINT32_C(0x464b0000) | frame) ||
        mailbox(UINT32_C(0x4b500123), UINT32_C(0x4b510123)))
        return -1;
    uint64_t start = hpclang_time_monotonic_us();
    int result = matrix ? __real_keyb_getkeyM(wait) : __real_keyb_getkey(wait);
    require(result == (matrix ? 41 : 5) && !keyb_isAnyKeyPressed() &&
                hpclang_time_monotonic_us() - start >= 100000,
            3);
    if (matrix)
        ++pages;
    else
        ++waits;
    return result;
}
int __wrap_keyb_getkey(int wait)
{
    return key(0, wait);
}
int __wrap_keyb_getkeyM(int wait)
{
    return key(1, wait);
}

static int contents(char *path, const char *expected)
{
    FS_FILE *file = NULL;
    char buffer[80] = {0};
    if (FSOpen(path, FSMODE_READ, &file) != FS_OK)
        return 0;
    int count = FSRead(buffer, sizeof buffer, file);
    int closed = __real_FSClose(file);
    return count == (int)strlen(expected) && !memcmp(buffer, expected, (size_t)count) &&
           closed == FS_OK;
}
static void output(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    require(file != NULL, 4);
    if (file) {
        require(fwrite(text, 1, strlen(text), file) == strlen(text), 5);
        require(fclose(file) == 0, 6);
    }
}
int __wrap_FSGetNextEntry(FS_FILE *entry, FS_FILE *directory)
{
    int result = __real_FSGetNextEntry(entry, directory);
    if (phase == 1 && result == FS_OK)
        ++entries;
    require(result == FS_OK || result == FS_EOF, 7);
    return result;
}
int __wrap_FSWrite(char *buffer, int bytes, FS_FILE *file)
{
    static const char *const expected[] = {"Hello world!\n", "Appended string", "HP49G+"};
    if (phase == 2) {
        require(writes < 3, 8);
        if (writes < 3)
            require(bytes == (int)strlen(expected[writes]) &&
                        !memcmp(buffer, expected[writes], (size_t)bytes),
                    9);
        ++writes;
    }
    int result = __real_FSWrite(buffer, bytes, file);
    require(result == bytes, 10);
    return result;
}
int __wrap_FSClose(FS_FILE *file)
{
    unsigned mode = file->Mode;
    int result = __real_FSClose(file);
    require(result == FS_OK, 11);
    if (phase == 2 && (mode == FSMODE_WRITE || mode == FSMODE_APPEND)) {
        ++closes;
        require(contents("Test file 1",
                         mode == FSMODE_WRITE ? "Hello world!\n" : "Hello world!\nAppended string"),
                12);
    }
    return result;
}
int __wrap_FSCloseAndDelete(FS_FILE *file)
{
    char buffer[80] = {0};
    require(phase == 2 && FSSeek(file, 0, SEEK_SET) == FS_OK, 13);
    int count = FSRead(buffer, sizeof buffer, file);
    require(count == sizeof modified - 1 && !memcmp(buffer, modified, sizeof modified - 1), 14);
    if (count == sizeof modified - 1)
        output("/CAPTURE.BIN", buffer);
    int result = __real_FSCloseAndDelete(file);
    require(result == FS_OK, 15);
    ++deletes;
    return result;
}
int __wrap_FSRename(char *oldname, char *newname)
{
    require(!strcmp(oldname, "My own text document.txt") &&
                !strcmp(newname, "\\Another document.bin"),
            16);
    int result = __real_FSRename(oldname, newname);
    require(result == FS_OK && contents(newname, "RENAME-SENTINEL\n"), 17);
    ++renames;
    return result;
}
int main(void)
{
    if (mailbox(UINT32_C(0x46520032), UINT32_C(0x464b0032)))
        goto done;
    uint64_t start = hpclang_time_monotonic_us();
    while (keyb_isAnyKeyPressed())
        if (hpclang_time_monotonic_us() - start > 2000000) {
            require(0, 18);
            goto done;
        }
    require(FSVolumeInserted(0) == FS_OK && FSSetCurrentVolume(0) == FS_OK &&
                FSChdir("\\") == FS_OK,
            19);
    for (int volume = 1; volume < 4; ++volume)
        require(FSVolumeInserted(volume) == FS_BADVOLUME, 20);
    /* Disposable single-volume card. Seed enough entries to exercise pagination. */
    for (unsigned n = 0; n < 6; ++n) {
        char path[] = "/SEED0.TXT";
        path[5] = (char)('0' + n);
        output(path, "ENTRY\n");
    }
    require(FSMkdir("Test Dir") == FS_OK, 21);
    output("/Test Dir/My own text document.txt", "RENAME-SENTINEL\n");
    if (failure)
        goto done;
    phase = 1;
    require(historical_enum_main() == 0, 22);
    require(entries >= 7 && pages >= 1 && waits == 2, 23);
    phase = 2;
    require(historical_write_main() == 0, 24);
    phase = 0;
    require(writes == 3 && closes == 2 && deletes == 1 && renames == 1 && waits == 4, 25);
    FS_FILE *file = NULL;
    require(FSOpen("Test file 1", FSMODE_READ, &file) == FS_NOTFOUND && file == NULL, 26);
    require(FSOpenDir("Subdir", &file) == FS_NOTFOUND && file == NULL, 27);
    require(FSOpen("My own text document.txt", FSMODE_READ, &file) == FS_NOTFOUND && file == NULL,
            28);
    require(contents("\\Another document.bin", "RENAME-SENTINEL\n"), 29);
    if (!failure)
        output("/RESULT.TXT", "ORIGINAL-FSYSTEM-PASS\n");
done:
    hpclang_e2e_result = failure ? UINT32_C(0x46418000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile unsigned n = 12000000; n; --n)
        __asm__ volatile("nop");
    hpg_cleanup();
    return 0;
}
