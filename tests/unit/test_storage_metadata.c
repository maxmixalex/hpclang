#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/device.h>
#include <hpclang/target.h>
#include "storage-namespace.h"
#include "ff.h"
#include "diskio.h"

#define SECTORS 2048u
#define ROOT_SECTOR 7u
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s (errno=%d)\n", __LINE__, #x, errno);                      \
            return EXIT_FAILURE;                                                                   \
        }                                                                                          \
    } while (0)
static BYTE disk[SECTORS * 512u];
static int protected_media;
static struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};

const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
int hpclang_storage_stock_media_query(int *present, int *read_only)
{
    *present = 1;
    *read_only = protected_media;
    return 0;
}
int hpclang_hp4950_fatfs_controller_acquire(int read_only)
{
    (void)read_only;
    return 0;
}
void hpclang_hp4950_fatfs_controller_release(void)
{
}
void hpclang_storage_after_stock_reinit(void)
{
}
uint32_t hpclang_kos_swi3(uint32_t index, uint32_t a, uint32_t b, uint32_t c)
{
    (void)index;
    (void)a;
    (void)b;
    (void)c;
    return 0;
}
DSTATUS disk_status(BYTE drive)
{
    return drive ? STA_NOINIT : (protected_media ? STA_PROTECT : 0);
}
DSTATUS disk_initialize(BYTE drive)
{
    return disk_status(drive);
}
DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive || sector >= SECTORS || count > SECTORS - sector)
        return RES_PARERR;
    memcpy(buffer, disk + sector * 512u, count * 512u);
    return RES_OK;
}
DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
    if (protected_media)
        return RES_WRPRT;
    if (drive || sector >= SECTORS || count > SECTORS - sector)
        return RES_PARERR;
    memcpy(disk + sector * 512u, buffer, count * 512u);
    return RES_OK;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
    (void)buffer;
    return !drive && command == CTRL_SYNC ? RES_OK : RES_PARERR;
}
DWORD get_fattime(void)
{
    return ((DWORD)(2026 - 1980) << 25) | (10u << 21) | (7u << 16) | (12u << 11);
}
static void put16(BYTE *p, unsigned value)
{
    p[0] = (BYTE)value;
    p[1] = (BYTE)(value >> 8);
}
static unsigned date(unsigned year, unsigned month, unsigned day)
{
    return ((year - 1980u) << 9) | (month << 5) | day;
}

int main(void)
{
    // Independent FAT12 fixture with three deliberately distinct timestamps.
    memcpy(disk, "\xeb\x3c\x90MSDOS5.0", 11);
    put16(disk + 11, 512);
    disk[13] = 1;
    put16(disk + 14, 1);
    disk[16] = 1;
    put16(disk + 17, 64);
    put16(disk + 19, SECTORS);
    disk[21] = 0xf8;
    put16(disk + 22, 6);
    disk[38] = 0x29;
    memcpy(disk + 54, "FAT12   ", 8);
    put16(disk + 510, 0xaa55);
    memcpy(disk + 512, "\xf8\xff\xff", 3);
    BYTE *entry = disk + ROOT_SECTOR * 512u;
    memcpy(entry, "STAMP   TXT", 11);
    entry[11] = AM_ARC;
    put16(entry + 14, (12u << 11) | (34u << 5) | 28u);
    put16(entry + 16, date(2020, 2, 29));
    put16(entry + 18, date(2021, 4, 5));
    put16(entry + 22, (3u << 11) | (4u << 5) | 3u);
    put16(entry + 24, date(2024, 1, 2));
    struct hpclang_stream_path_info info;
    CHECK(hpclang_namespace_path_query("STAMP.TXT", &info) == 0);
    CHECK(info.time_validity == 7u && info.created.year == 2020 && info.created.day == 29);
    CHECK(info.created.hour == 12 && info.created.minute == 34 && info.created.second == 56);
    CHECK(info.last_accessed.year == 2021 && info.last_accessed.month == 4 &&
          info.last_accessed.day == 5);
    CHECK(info.last_accessed.hour == 0 && info.last_modified.year == 2024);
    struct hpclang_stream_directory *dir = hpclang_stream_directory_open("/");
    struct hpclang_stream_directory_entry item;
    CHECK(dir && hpclang_stream_directory_read(dir, &item) == 1);
    CHECK(item.time_validity == info.time_validity &&
          !memcmp(&item.created, &info.created, sizeof(info.created)));
    CHECK(hpclang_stream_directory_close(dir) == 0);

    const uint32_t ro = HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY;
    const uint32_t hidden = HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN;
    CHECK(hpclang_namespace_path_set_attributes("STAMP.TXT", ro | hidden, ro | hidden) == 0);
    hpclang_storage_shutdown();
    CHECK(hpclang_namespace_path_query("STAMP.TXT", &info) == 0);
    CHECK(info.attributes == (ro | hidden | HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE));
    CHECK(hpclang_namespace_open("STAMP.TXT", O_WRONLY) < 0 && errno == EACCES);
    CHECK(hpclang_namespace_path_set_attributes("STAMP.TXT", 0, ro) == 0);
    CHECK(hpclang_namespace_path_set_attributes("STAMP.TXT", 0, 0x100u) < 0 && errno == EINVAL);
    CHECK(hpclang_namespace_path_set_attributes("STAMP.TXT", ro, hidden) < 0 && errno == EINVAL);
    CHECK(hpclang_namespace_path_set_attributes("MISSING.TXT", 0, ro) < 0 && errno == ENOENT);
    CHECK(hpclang_namespace_path_query("STAMP.TXT", &info) == 0 && !(info.attributes & ro));
    int fd = hpclang_namespace_open("NEW.TXT", O_CREAT | O_RDWR | O_EXCL);
    CHECK(fd >= 0 && hpclang_namespace_write(fd, "abc", 3) == 3);
    CHECK(hpclang_namespace_path_set_attributes("NEW.TXT", hidden, hidden) == 0);
    CHECK(hpclang_namespace_close(fd) == 0);
    hpclang_storage_shutdown();
    CHECK(hpclang_namespace_path_query("NEW.TXT", &info) == 0 && info.byte_size == 3);
    CHECK((info.attributes & hidden) && (info.time_validity & HPCLANG_STREAM_PATH_CREATED_VALID));
    CHECK(info.created.year == 2026 && info.created.month == 10 && info.created.day == 7);
    CHECK(!(info.time_validity & HPCLANG_STREAM_PATH_LAST_ACCESSED_VALID));
    hpclang_storage_shutdown();
    // Invalid calendar fields must not be exported as valid timestamps.
    put16(entry + 16, date(2021, 2, 29));
    put16(entry + 18, 0);
    put16(entry + 22, 0xffff);
    CHECK(hpclang_namespace_path_query("STAMP.TXT", &info) == 0 && info.time_validity == 0);
    CHECK(info.created.year == 0 && info.last_accessed.year == 0 && info.last_modified.year == 0);
    hpclang_storage_shutdown();
    protected_media = 1;
    CHECK(hpclang_namespace_path_set_attributes("STAMP.TXT", 0, hidden) < 0 && errno == EROFS);
    hpclang_storage_shutdown();
    device.family = HPCLANG_FAMILY_HP39;
    CHECK(hpclang_namespace_path_set_attributes("STAMP.TXT", 0, hidden) < 0 && errno == ENOSYS);
    return EXIT_SUCCESS;
}
