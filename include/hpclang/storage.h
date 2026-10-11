#ifndef HPCLANG_STORAGE_H
#define HPCLANG_STORAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HPCLANG_STREAM_PATH_INFO_VERSION 3u
#define HPCLANG_STREAM_STORAGE_INFO_VERSION 1u
#define HPCLANG_STREAM_DIRECTORY_ENTRY_VERSION 2u
#define HPCLANG_STREAM_NAME_CAPACITY 256u
#define HPCLANG_STREAM_VOLUME_INFO_VERSION 1u
#define HPCLANG_STREAM_VOLUME_LIMIT 4u

    enum hpclang_stream_path_type
    {
        HPCLANG_STREAM_PATH_TYPE_UNKNOWN = 0,
        HPCLANG_STREAM_PATH_TYPE_FILE = 1,
        HPCLANG_STREAM_PATH_TYPE_DIRECTORY = 2,
        HPCLANG_STREAM_PATH_TYPE_VOLUME_LABEL = 3
    };

    enum hpclang_stream_path_attribute
    {
        HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY = 1u << 0,
        HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN = 1u << 1,
        HPCLANG_STREAM_PATH_ATTRIBUTE_SYSTEM = 1u << 2,
        HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE = 1u << 3
    };

    enum hpclang_stream_path_time_validity
    {
        HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID = 1u << 0,
        HPCLANG_STREAM_PATH_CREATED_VALID = 1u << 1,
        HPCLANG_STREAM_PATH_LAST_ACCESSED_VALID = 1u << 2
    };

    /*
     * Filesystem civil date/time values. They deliberately carry no timezone or
     * epoch interpretation: FAT stores local civil fields. Last-modified has
     * FAT's two-second resolution, as does created. Last-accessed is a stored
     * date only
     * (hour/minute/second zero); this backend does not maintain atime.
     * Absent or malformed
     * dates are zero-filled with their validity bit clear.
     */

    struct hpclang_stream_civil_datetime
    {
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
     * creation, modification and stored access dates without inventing a
     * timezone/epoch
     * conversion.
     */
    struct hpclang_stream_path_info
    {
        uint32_t size;
        uint32_t version;
        uint64_t byte_size;
        uint32_t type;
        uint32_t attributes;
        uint32_t time_validity;
        uint32_t reserved;
        struct hpclang_stream_civil_datetime last_modified;
        uint32_t reserved2[2];
        struct hpclang_stream_civil_datetime created;
        struct hpclang_stream_civil_datetime last_accessed;
    };

    enum hpclang_stream_storage_flag
    {
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
     * is retained for post-release ResetSD. Pre-takeover insertion/protect
     * sampling reads existing GPIO inputs without changing their configuration.
     * Before takeover, an absent card can be inserted and a later call retried.
     * During ownership, backend operations resample presence/protection. An
     * observed removal or block-transfer error invalidates the card; a later
     * path operation may remount it, while old file/directory objects fail with
     * EBADF and must be closed. Dirty old buffers are never flushed to a newly
     * mounted volume. During ownership, the card-detect pin's EINT3 falling-edge
     * latch also detects removal/reinsertion between software observations.
     * The lease masks/routes this card interrupt, consumes its notifications,
     * and restores the borrowed pin/trigger/mask/routing fields before Stock
     * ResetSD reconciles the current card. Other interrupt sources are retained.
     * Physical hot-plug remains a separate hardware-validation follow-up.
     * Unsupported pin modes fail before takeover with ENOTSUP; during ownership
     * an unreadable media state invalidates the card. Only the owned card-detect
     * pin changes from GPIO input to its equivalent external-interrupt input.
     */
    struct hpclang_stream_storage_info
    {
        uint32_t size;
        uint32_t version;
        uint32_t flags;
        uint32_t reserved;
        uint64_t free_bytes;
        uint64_t capacity_bytes;
    };

    /* Mounted FAT volumes are numbered densely in primary-partition order.
     * Unqualified paths retain volume 0 (the first usable FAT volume); an
     * explicit "N:/path" selects volume N. Each volume has an independent root.
     * MBR partition numbers are 1..4; zero denotes an unpartitioned FAT card.
     * Discovery acquires the same process-wide SD lease as file I/O. Volume
     * indexes must be discovered again after card replacement. No card-global
     * current-volume or current-directory state is changed in Stock. */
    struct hpclang_stream_volume_info
    {
        uint32_t size;
        uint32_t version;
        uint32_t index;
        uint32_t partition;
        uint64_t first_sector;
        struct hpclang_stream_storage_info storage;
    };

    int hpclang_stream_volume_count(unsigned *count);
    int hpclang_stream_volume_query(unsigned index, struct hpclang_stream_volume_info *info);

    /*
     * These services address the HP49g+/HP50g SD-card FAT volume. HOME/Port 0,
     * Port 1 and Port 2 are calculator/RPL object stores, not FAT byte-stream
     * volumes, and belong to the separate calculator-object service boundary.
     * Other calculator families return -1 with errno=ENOSYS rather than treating
     * calculator-object stores as POSIX/byte-stream filesystems.
     */
    int hpclang_stream_path_query(const char *path, struct hpclang_stream_path_info *info);
    /* Change only masked READ_ONLY/HIDDEN/SYSTEM/ARCHIVE bits. Unknown bits or
     * attributes
     * outside mask fail with EINVAL. Existing open descriptors retain
     * their access rights.

     * * Changes are synchronized before successful return. */
    int hpclang_stream_path_set_attributes(const char *path, uint32_t attributes, uint32_t mask);

    /*
     * Owned directory iterator.
     *
     * The implementation is intentionally opaque: unlike Stock KOS enumeration,
     * each iterator owns independent traversal state. Paths are explicit UTF-8
     * paths rooted at the SD volume; no calculator-global current directory is
     * changed.
     */
    struct hpclang_stream_directory;

    struct hpclang_stream_directory_entry
    {
        uint32_t size;
        uint32_t version;
        uint64_t byte_size;
        uint32_t type;
        uint32_t attributes;
        uint32_t time_validity;
        uint32_t reserved;
        struct hpclang_stream_civil_datetime last_modified;
        char name[HPCLANG_STREAM_NAME_CAPACITY];
        struct hpclang_stream_civil_datetime created;
        struct hpclang_stream_civil_datetime last_accessed;
    };

    struct hpclang_stream_directory *hpclang_stream_directory_open(const char *path);

    /*
     * Return 1 and fill entry for one item, 0 at end-of-directory, or -1 with
     * errno set on error.
     */
    int hpclang_stream_directory_read(struct hpclang_stream_directory *directory,
                                      struct hpclang_stream_directory_entry *entry);

    int hpclang_stream_directory_close(struct hpclang_stream_directory *directory);

    int hpclang_stream_storage_query(struct hpclang_stream_storage_info *info);

    /* Convenience query requiring present media and a valid free-byte count. */
    int hpclang_stream_storage_free_bytes(uint64_t *bytes);

#ifdef __cplusplus
}
#endif

#endif
