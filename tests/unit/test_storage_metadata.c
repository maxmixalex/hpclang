#include <assert.h>
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

#define VOLUME_SECTORS 2048u
#define SECTORS (VOLUME_SECTORS * 5u)
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
static int inserted_media;
static int initialized_media;
static unsigned media_writes;
static unsigned controller_acquires;
static uint8_t card_identity = 0x37;
static struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};

const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
int hpclang_storage_stock_media_query(int *present, int *read_only)
{
    assert(hpclang_namespace_stock_active());
    *present = inserted_media;
    *read_only = protected_media;
    return 0;
}
int hpclang_hp4950_fatfs_controller_acquire(int read_only)
{
    (void)read_only;
    ++controller_acquires;
    return 0;
}
void hpclang_hp4950_fatfs_controller_release(void)
{
}
int hpclang_hp4950_fatfs_card_identity(uint8_t cid[16])
{
    assert(inserted_media && initialized_media);
    memset(cid, card_identity, 16);
    return 0;
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
    if (!inserted_media)
        initialized_media = 0;
    return drive || !initialized_media ? STA_NOINIT : (protected_media ? STA_PROTECT : 0);
}
DSTATUS disk_initialize(BYTE drive)
{
    if (!drive && inserted_media)
        initialized_media = 1;
    return disk_status(drive);
}
DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
    if (disk_status(drive) & STA_NOINIT)
        return RES_NOTRDY;
    if (drive || sector >= SECTORS || count > SECTORS - sector)
        return RES_PARERR;
    memcpy(buffer, disk + sector * 512u, count * 512u);
    return RES_OK;
}
DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
    if (disk_status(drive) & STA_NOINIT)
        return RES_NOTRDY;
    if (protected_media)
        return RES_WRPRT;
    if (drive || sector >= SECTORS || count > SECTORS - sector)
        return RES_PARERR;
    ++media_writes;
    memcpy(disk + sector * 512u, buffer, count * 512u);
    return RES_OK;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
    if (!drive && command == GET_SECTOR_COUNT) {
        *(LBA_t *)buffer = SECTORS;
        return RES_OK;
    }
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
static void put32(BYTE *p, uint32_t value)
{
    put16(p, value & 0xffffu);
    put16(p + 2, value >> 16);
}

static int partition_checks(void)
{
    // Preserve a real FAT volume, then put copies in nonadjacent MBR slots.
    memmove(disk + VOLUME_SECTORS * 512u, disk, VOLUME_SECTORS * 512u);
    memcpy(disk + VOLUME_SECTORS * 3u * 512u, disk + VOLUME_SECTORS * 512u, VOLUME_SECTORS * 512u);
    memset(disk, 0, 512);
    put16(disk + 510, 0xaa55);
    BYTE *first = disk + 446;
    BYTE *third = first + 32;
    first[4] = third[4] = 1;
    put32(first + 8, VOLUME_SECTORS);
    put32(first + 12, VOLUME_SECTORS);
    put32(third + 8, VOLUME_SECTORS * 3u);
    put32(third + 12, VOLUME_SECTORS);
    unsigned count = 99;
    CHECK(hpclang_stream_volume_count(&count) == 0 && count == 2);
    struct hpclang_stream_volume_info info;
    CHECK(hpclang_stream_volume_query(0, &info) == 0 && info.partition == 1 &&
          info.first_sector == VOLUME_SECTORS);
    CHECK(hpclang_stream_volume_query(1, &info) == 0 && info.partition == 3 &&
          info.first_sector == VOLUME_SECTORS * 3u);
    CHECK(info.storage.capacity_bytes == VOLUME_SECTORS * 512u);
    CHECK(hpclang_stream_volume_query(2, &info) == -1 && errno == ENODEV);
    CHECK(hpclang_namespace_open("2:/NEW.TXT", O_RDONLY) == -1 && errno == ENODEV);
    CHECK(hpclang_stream_directory_open("2:/") == NULL && errno == ENODEV);
    int left = hpclang_namespace_open("0:/NEW.TXT", O_RDWR);
    int right = hpclang_namespace_open("1:/NEW.TXT", O_RDWR);
    CHECK(left >= 0 && right >= 0);
    CHECK(hpclang_namespace_write(left, "one", 3) == 3);
    CHECK(hpclang_namespace_write(right, "two", 3) == 3);
    CHECK(hpclang_namespace_close(left) == 0 && hpclang_namespace_close(right) == 0);
    CHECK(hpclang_namespace_rename("0:/NEW.TXT", "1:/MOVED.TXT") == -1 && errno == EXDEV);
    left = hpclang_namespace_open("NEW.TXT", O_RDONLY);
    right = hpclang_namespace_open("1:/NEW.TXT", O_RDWR);
    char bytes[3];
    CHECK(hpclang_namespace_read(left, bytes, 3) == 3 && !memcmp(bytes, "one", 3));
    CHECK(hpclang_namespace_read(right, bytes, 3) == 3 && !memcmp(bytes, "two", 3));
    CHECK(hpclang_namespace_write(right, "dirty", 5) == 5);
    unsigned writes = media_writes;
    initialized_media = 0; // Card changed between observations while still inserted.
    CHECK(hpclang_stream_volume_count(&count) == 0 && count == 2);
    CHECK(hpclang_namespace_close(left) == -1 && errno == EBADF);
    CHECK(hpclang_namespace_close(right) == -1 && errno == EBADF);
    CHECK(media_writes == writes);
    hpclang_storage_shutdown();
    // Overlapping partitions must not expose two caches over the same sectors.
    put32(third + 8, VOLUME_SECTORS);
    count = 99;
    CHECK(hpclang_stream_volume_count(&count) == -1 && count == 99);
    hpclang_storage_shutdown();
    put32(third + 8, VOLUME_SECTORS * 3u);
    // Exercise the fourth slot and the exact physical-card boundary.
    memcpy(disk + VOLUME_SECTORS * 2u * 512u, disk + VOLUME_SECTORS * 512u, VOLUME_SECTORS * 512u);
    memcpy(disk + VOLUME_SECTORS * 4u * 512u, disk + VOLUME_SECTORS * 512u, VOLUME_SECTORS * 512u);
    for (unsigned slot = 1; slot < 4; slot += 2) {
        BYTE *partition = first + slot * 16;
        partition[4] = 1;
        put32(partition + 8, (slot + 1u) * VOLUME_SECTORS);
        put32(partition + 12, VOLUME_SECTORS);
    }
    CHECK(hpclang_stream_volume_count(&count) == 0 && count == 4);
    CHECK(hpclang_stream_volume_query(3, &info) == 0 && info.partition == 4);
    hpclang_storage_shutdown();
    put32(first + 3u * 16u + 12u, VOLUME_SECTORS + 1u);
    CHECK(hpclang_stream_volume_count(&count) == -1);
    hpclang_storage_shutdown();
    return EXIT_SUCCESS;
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
    put16(disk + 19, VOLUME_SECTORS);
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
    CHECK(hpclang_namespace_stock_active());
    CHECK(hpclang_namespace_path_query("STAMP.TXT", &info) == -1 && errno == ENODEV);
    CHECK(controller_acquires == 0 && hpclang_namespace_stock_active());
    struct hpclang_stream_media_identity original = {.size = 99}, observed;
    CHECK(hpclang_stream_media_identity_query(&original) == -1 && errno == ENODEV);
    CHECK(original.size == 99 && controller_acquires == 0);
    inserted_media = 1;
    CHECK(hpclang_namespace_path_query("STAMP.TXT", &info) == 0);
    CHECK(controller_acquires == 1 && !hpclang_namespace_stock_active());
    CHECK(hpclang_stream_media_identity_query(&original) == 0);
    CHECK(original.size == sizeof original &&
          original.version == HPCLANG_STREAM_MEDIA_IDENTITY_VERSION);
    CHECK(original.cid[0] == 0x37 && original.cid[15] == 0x37);
    CHECK(hpclang_stream_media_identity_query(&observed) == 0 &&
          !memcmp(original.cid, observed.cid, sizeof original.cid));
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
    protected_media = 0;
    // Keep dirty file and directory objects alive across an observed removal.
    fd = hpclang_namespace_open("NEW.TXT", O_RDWR);
    CHECK(fd >= 0);
    CHECK(hpclang_namespace_write(fd, "old", 3) == 3);
    struct hpclang_stream_directory *old_directory = hpclang_stream_directory_open("/");
    CHECK(old_directory != NULL);
    unsigned before_removal = media_writes;
    inserted_media = 0;
    CHECK(hpclang_stream_media_identity_query(&observed) == -1 && errno == ENODEV);
    CHECK(!memcmp(original.cid, observed.cid, sizeof original.cid));
    char old_byte = '?';
    CHECK(hpclang_namespace_read(fd, &old_byte, 1) == -1 && errno == EBADF);
    CHECK(old_byte == '?');
    CHECK(hpclang_namespace_lseek(fd, 0, SEEK_SET) == -1 && errno == EBADF);
    CHECK(hpclang_namespace_write(fd, "bad", 3) == -1 && errno == EBADF);
    CHECK(hpclang_namespace_ftruncate(fd, 0) == -1 && errno == EBADF);
    // Replacement has different contents at the same path/cluster location.
    BYTE *replacement_data = disk + (ROOT_SECTOR + 4u) * 512u;
    CHECK(memcmp(replacement_data, "abc", 3) == 0);
    memcpy(replacement_data, "xyz", 3);
    inserted_media = 1;
    int fresh = hpclang_namespace_open("NEW.TXT", O_RDWR);
    card_identity = 0x58;
    CHECK(hpclang_stream_media_identity_query(&observed) == 0 && observed.cid[0] == 0x58);
    CHECK(memcmp(original.cid, observed.cid, sizeof original.cid) != 0);
    CHECK(fresh >= 0 && fresh != fd);
    char persisted[3];
    CHECK(hpclang_namespace_read(fresh, persisted, sizeof persisted) == 3);
    CHECK(memcmp(persisted, "xyz", 3) == 0);
    CHECK(hpclang_namespace_lseek(fresh, 0, SEEK_SET) == 0);
    CHECK(hpclang_namespace_read(fd, &old_byte, 1) == -1 && errno == EBADF);
    struct hpclang_stream_directory_entry dir_entry;
    CHECK(hpclang_stream_directory_read(old_directory, &dir_entry) == -1 && errno == EBADF);
    CHECK(media_writes == before_removal);
    CHECK(hpclang_namespace_path_set_attributes("NEW.TXT", 0, hidden) == 0);
    CHECK(hpclang_namespace_close(fd) == -1 && errno == EBADF);
    CHECK(hpclang_stream_directory_close(old_directory) == -1 && errno == EBADF);
    protected_media = 1;
    CHECK(hpclang_namespace_write(fresh, "bad", 3) == -1 && errno == EROFS);
    CHECK(hpclang_namespace_ftruncate(fresh, 0) == -1 && errno == EROFS);
    protected_media = 0;
    CHECK(hpclang_namespace_write(fresh, "new", 3) == 3);
    CHECK(hpclang_namespace_close(fresh) == 0);
    hpclang_storage_shutdown();
    CHECK(partition_checks() == EXIT_SUCCESS);
    device.family = HPCLANG_FAMILY_HP39;
    struct hpclang_stream_media_identity identity = {.size = 123};
    CHECK(hpclang_stream_media_identity_query(NULL) == -1 && errno == EFAULT);
    CHECK(hpclang_stream_media_identity_query(&identity) == -1 && errno == ENOSYS);
    CHECK(identity.size == 123);
    CHECK(hpclang_namespace_path_set_attributes("STAMP.TXT", 0, hidden) < 0 && errno == ENOSYS);
    return EXIT_SUCCESS;
}
