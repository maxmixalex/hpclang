#ifndef HPCLANG_STORAGE_H
#define HPCLANG_STORAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HPCLANG_STREAM_PATH_INFO_VERSION 2u
#define HPCLANG_STREAM_STORAGE_INFO_VERSION 1u
#define HPCLANG_STREAM_DIRECTORY_ENTRY_VERSION 1u
#define HPCLANG_STREAM_NAME_CAPACITY 256u

enum hpclang_stream_path_type {
    HPCLANG_STREAM_PATH_TYPE_UNKNOWN = 0,
    HPCLANG_STREAM_PATH_TYPE_FILE = 1,
    HPCLANG_STREAM_PATH_TYPE_DIRECTORY = 2,
    HPCLANG_STREAM_PATH_TYPE_VOLUME_LABEL = 3
};

enum hpclang_stream_path_attribute {
    HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY = 1u << 0,
    HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN = 1u << 1,
    HPCLANG_STREAM_PATH_ATTRIBUTE_SYSTEM = 1u << 2,
    HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE = 1u << 3
};

enum hpclang_stream_path_time_validity {
    HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID = 1u << 0
};

/*
 * Filesystem civil date/time values. They deliberately carry no timezone or
 * epoch interpretation: FAT stores local civil fields. Last-modified has
 * FAT's two-second resolution.
 */

struct hpclang_stream_civil_datetime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t reserved;
};

/*
 * Portable metadata for one byte-stream filesystem path.
 *
 * The public attributes deliberately expose only stable FAT semantics.
 * Timestamp validity is explicit. The owned FatFs backend publishes the FAT
 * last-modified civil datetime without inventing a timezone/epoch conversion.
 */
struct hpclang_stream_path_info {
    uint32_t size;
    uint32_t version;
    uint64_t byte_size;
    uint32_t type;
    uint32_t attributes;
    uint32_t time_validity;
    uint32_t reserved;
    struct hpclang_stream_civil_datetime last_modified;
    uint32_t reserved2[2];
};

enum hpclang_stream_storage_flag {
    HPCLANG_STREAM_STORAGE_MEDIA_PRESENT = 1u << 0,
    HPCLANG_STREAM_STORAGE_MEDIA_READ_ONLY = 1u << 1,
    HPCLANG_STREAM_STORAGE_FREE_BYTES_VALID = 1u << 2,
    HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID = 1u << 3
};

/*
 * Byte-stream media state.
 *
 * The HP49g+/HP50g SD volume is owned by one process-lifetime FatFs namespace
 * once byte-stream storage is first used. Free space and total capacity come
 * from that backend. Stock KOS is not used for ordinary file reads/writes; it
 * is retained only for the narrow host-coordination boundary around SD
 * ownership (pre-takeover media/protect sampling and post-release ResetSD).
 *
 * Media insertion/write-protect state is sampled once per native launch before
 * takeover. Re-enter the native application to resample hot-plug/protect
 * changes.
 */
struct hpclang_stream_storage_info {
    uint32_t size;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved;
    uint64_t free_bytes;
    uint64_t capacity_bytes;
};

/*
 * These services address the HP49g+/HP50g SD-card FAT volume. HOME/Port 0,
 * Port 1 and Port 2 are calculator/RPL object stores, not FAT byte-stream
 * volumes, and belong to the separate calculator-object service boundary.
 * Other calculator families return -1 with errno=ENOSYS rather than treating
 * calculator-object stores as POSIX/byte-stream filesystems.
 */
int hpclang_stream_path_query(
    const char *path,
    struct hpclang_stream_path_info *info);


/*
 * Owned directory iterator.
 *
 * The implementation is intentionally opaque: unlike Stock KOS enumeration,
 * each iterator owns independent traversal state. Paths are explicit UTF-8
 * paths rooted at the SD volume; no calculator-global current directory is
 * changed.
 */
struct hpclang_stream_directory;

struct hpclang_stream_directory_entry {
    uint32_t size;
    uint32_t version;
    uint64_t byte_size;
    uint32_t type;
    uint32_t attributes;
    uint32_t time_validity;
    uint32_t reserved;
    struct hpclang_stream_civil_datetime last_modified;
    char name[HPCLANG_STREAM_NAME_CAPACITY];
};

struct hpclang_stream_directory *hpclang_stream_directory_open(
    const char *path);

/*
 * Return 1 and fill entry for one item, 0 at end-of-directory, or -1 with
 * errno set on error.
 */
int hpclang_stream_directory_read(
    struct hpclang_stream_directory *directory,
    struct hpclang_stream_directory_entry *entry);

int hpclang_stream_directory_close(
    struct hpclang_stream_directory *directory);


int hpclang_stream_storage_query(
    struct hpclang_stream_storage_info *info);

/* Convenience query requiring present media and a valid free-byte count. */
int hpclang_stream_storage_free_bytes(uint64_t *bytes);

#ifdef __cplusplus
}
#endif

#endif
