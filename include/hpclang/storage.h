#ifndef HPCLANG_STORAGE_H
#define HPCLANG_STORAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HPCLANG_STREAM_PATH_INFO_VERSION 2u
#define HPCLANG_STREAM_STORAGE_INFO_VERSION 1u

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
    HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID = 1u << 0,
    HPCLANG_STREAM_PATH_LAST_ACCESS_DATE_VALID = 1u << 1
};

/*
 * Filesystem civil date/time values. They deliberately carry no timezone or
 * epoch interpretation: FAT/KOS stores local civil fields. last-modified has
 * FAT's two-second resolution; last-access has date-only resolution.
 */
struct hpclang_stream_civil_date {
    uint16_t year;
    uint8_t month;
    uint8_t day;
};

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
 * The public attributes deliberately expose only stable FAT/KOS semantics.
 * Timestamp validity is explicit. The Stock backend publishes the independently
 * verified last-modified civil datetime and last-access civil date; KOS
 * creation-time and undocumented FILE_INFO fields remain runtime-private.
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
    struct hpclang_stream_civil_date last_access_date;
    uint32_t reserved2;
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
 * Validity bits are explicit because the Stock KOS filesystem exposes free
 * bytes and media state but no safe total-capacity query. capacity_bytes is
 * zero unless HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID is set.
 *
 * On the Stock HP49g+/HP50g backend, insertion and write-protect state are
 * sampled lazily and remain fixed for one native launch. Re-enter the native
 * application to resample physical SD hot-plug/protect changes. Free space is
 * queried on each call.
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
 * These services are available on HP49g+/HP50g. Other calculator families
 * return -1 with errno=ENOSYS rather than treating calculator-object stores as
 * POSIX/byte-stream filesystems.
 */
int hpclang_stream_path_query(
    const char *path,
    struct hpclang_stream_path_info *info);


int hpclang_stream_storage_query(
    struct hpclang_stream_storage_info *info);

/* Convenience query requiring present media and a valid free-byte count. */
int hpclang_stream_storage_free_bytes(uint64_t *bytes);

#ifdef __cplusplus
}
#endif

#endif
