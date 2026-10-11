/* Flow-level port of HPGCC 2.0 sources/fsystem/examples/example3.c.
 * Preserve volume enumeration, independent cwd, nested directories, create,
 * append, seek/modify, rename and cleanup. The original example's literal
 * append length reads beyond its string; use sizeof minus the terminator.
 * Interactive pauses become assertions in this unattended application client.
 * Runtime teardown performs the optional original FSShutdown operation. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fsystem.h>

static int volume_client(int volume)
{
    static const char greeting[] = "Hello world!\n";
    static const char suffix[] = "Appended string";
    static const char expected[] = "Hello HP49G+\nAppended string";
    FS_FILE *file = NULL;
    FS_FILE *directory = NULL;
    int failure = 1;
    if (FSSetCurrentVolume(volume) != FS_OK || FSGetCurrentVolume() != volume ||
        FSGetVolumeSize(volume) <= 0 || FSGetVolumeFree(volume) <= 0 ||
        FSMkdir("Test Dir") != FS_OK || FSChdir("Test Dir") != FS_OK || FSMkdir("Subdir") != FS_OK)
        goto done;
    failure = 2;
    if (FSOpen("Test file 1", FSMODE_WRITE, &file) != FS_OK || file->Volume != (unsigned)volume ||
        FSWrite((void *)greeting, sizeof greeting - 1, file) != sizeof greeting - 1)
        goto done;
    int closed = FSClose(file);
    file = NULL;
    if (closed != FS_OK)
        goto done;
    failure = 3;
    if (FSOpen("Test file 1", FSMODE_APPEND, &file) != FS_OK ||
        FSWrite((void *)suffix, sizeof suffix - 1, file) != sizeof suffix - 1)
        goto done;
    closed = FSClose(file);
    file = NULL;
    if (closed != FS_OK)
        goto done;
    failure = 4;
    if (FSOpen("Test file 1", FSMODE_MODIFY | FSMODE_NOCREATE, &file) != FS_OK ||
        FSSeek(file, 6, SEEK_SET) != FS_OK || FSWrite("HP49G+", 6, file) != 6)
        goto done;
    closed = FSClose(file);
    file = NULL;
    if (closed != FS_OK || FSRename("Test file 1", "Subdir/Renamed text.txt") != FS_OK)
        goto done;
    failure = 5;
    if (FSChdir("Subdir") != FS_OK || FSOpen("Renamed text.txt", FSMODE_READ, &file) != FS_OK)
        goto done;
    char contents[sizeof expected] = {0};
    if (FSRead(contents, sizeof contents, file) != sizeof expected - 1 ||
        strcmp(contents, expected))
        goto done;
    closed = FSClose(file);
    file = NULL;
    if (closed != FS_OK)
        goto done;
    failure = 6;
    if (FSOpenDir(".", &directory) != FS_OK)
        goto done;
    unsigned files = 0, bytes = 0;
    FS_FILE entry;
    int status;
    while ((status = FSGetNextEntry(&entry, directory)) == FS_OK) {
        if (entry.Volume != (unsigned)volume || directory->Volume != (unsigned)volume) {
            FSReleaseEntry(&entry);
            goto done;
        }
        if (!(FSAttr(&entry) & FSATTR_DIR)) {
            ++files;
            bytes += entry.FileSize;
        }
        FSReleaseEntry(&entry);
    }
    closed = FSClose(directory);
    directory = NULL;
    if (status != FS_EOF || closed != FS_OK || files != 1 || bytes != sizeof expected - 1)
        goto done;
    failure = 7;
    if (FSDelete("Renamed text.txt") != FS_OK || FSChdir("..") != FS_OK ||
        FSRmdir("Subdir") != FS_OK || FSChdir("..") != FS_OK || FSRmdir("Test Dir") != FS_OK)
        goto done;
    failure = 0;
done:
    if (file)
        (void)FSClose(file);
    if (directory)
        (void)FSClose(directory);
    return failure;
}

unsigned hpclang_historical_files(void)
{
    unsigned visited = 0;
    for (int volume = 0; volume < 4; ++volume) {
        int status = FSVolumeInserted(volume);
        if (status == FS_BADVOLUME)
            continue;
        if (status != FS_OK)
            return 1;
        int failure = volume_client(volume);
        if (failure)
            return 2u + (unsigned)failure;
        ++visited;
    }
    return visited == 2 && FSSetCurrentVolume(0) == FS_OK ? 0 : 10;
}
