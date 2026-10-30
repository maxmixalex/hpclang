#ifndef HPCLANG_OBJECT_STORE_H
#define HPCLANG_OBJECT_STORE_H

#include <stdint.h>

#include <hpclang/object.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HPCLANG_OBJECT_STORE_ENTRY_VERSION 1u
#define HPCLANG_OBJECT_STORE_NAME_CAPACITY 256u

    enum hpclang_object_store_entry_flag
    {
        HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY = 1u << 0
    };

    struct hpclang_object_store;
    struct hpclang_object_store_iterator;

    /*
     * Calculator-native named-object namespaces.
     *
     * Names are raw calculator bytes plus an explicit length. HP48/49/50 names are
     * not implicitly reinterpreted as UTF-8. Handles identify logical paths and
     * never expose or retain public Saturn addresses.
     */
    struct hpclang_object_store_entry
    {
        uint32_t size;
        uint32_t version;
        uint32_t kind;
        uint32_t flags;
        uint32_t name_length;
        uint32_t reserved;
        uint8_t name[HPCLANG_OBJECT_STORE_NAME_CAPACITY];
    };

    /* Snapshot the logical HOME or current-directory path without changing cwd. */
    struct hpclang_object_store *hpclang_object_store_home(void);
    struct hpclang_object_store *hpclang_object_store_current(void);

    /*
     * Open one direct child directory by exact raw-byte name. A zero-length name
     * is valid and addresses the classic RPL Hidden directory when present.
     */
    struct hpclang_object_store *
    hpclang_object_store_open_child(const struct hpclang_object_store *parent, const uint8_t *name,
                                    uint32_t name_length);

    void hpclang_object_store_close(struct hpclang_object_store *store);

    enum hpclang_object_store_put_mode
    {
        HPCLANG_OBJECT_STORE_UPSERT = 0,
        HPCLANG_OBJECT_STORE_CREATE_ONLY = 1,
        HPCLANG_OBJECT_STORE_REPLACE_ONLY = 2
    };

    /*
     * Copy an owned value into one exact name in this logical directory.
     * UPSERT creates or replaces. CREATE_ONLY rejects existing names (EEXIST);
     * REPLACE_ONLY rejects missing names (ENOENT). Directories are rejected
     * (EISDIR). Input snapshots and old Stock references retain their values.
     * Stock cwd is unchanged. Unsupported platforms return ENOSYS.
     */
    int hpclang_object_store_put(const struct hpclang_object_store *store, const uint8_t *name,
                                 uint32_t name_length, const struct hpclang_object *object,
                                 enum hpclang_object_store_put_mode mode);

    /* Remove one exact non-directory name; a missing name returns ENOENT. */
    int hpclang_object_store_remove(const struct hpclang_object_store *store, const uint8_t *name,
                                    uint32_t name_length);

    /* Create an empty child directory; any existing name returns EEXIST. */
    int hpclang_object_store_mkdir(const struct hpclang_object_store *store, const uint8_t *name,
                                   uint32_t name_length);

    enum hpclang_object_store_directory_remove_mode
    {
        HPCLANG_OBJECT_STORE_REMOVE_EMPTY = 0,
        HPCLANG_OBJECT_STORE_REMOVE_TREE = 1
    };

    /*
     * Remove a child directory. EMPTY rejects nonempty directories (ENOTEMPTY);
     * TREE explicitly removes the whole subtree. Non-directories return ENOTDIR.
     * Deleting Stock cwd or an ancestor returns EBUSY, without changing cwd.
     * Existing Stock value references retain a TEMP copy; logical path handles
     * re-resolve names and report ENOENT while their directory is absent.
     * Unsupported platforms return ENOSYS. Expected failures precede mutation.
     */
    int hpclang_object_store_rmdir(const struct hpclang_object_store *store, const uint8_t *name,
                                   uint32_t name_length,
                                   enum hpclang_object_store_directory_remove_mode mode);

    /*
     * Copy one non-directory named object into an owned hpclang_object snapshot.
     * Directories are opened with hpclang_object_store_open_child().
     */
    int hpclang_object_store_get(const struct hpclang_object_store *store, const uint8_t *name,
                                 uint32_t name_length, struct hpclang_object **object);

    /*
     * Independent snapshot iterator. Opening the iterator copies entry metadata
     * into ARM-side memory; subsequent reads do not retain borrowed Saturn
     * addresses and do not depend on Stock global cwd.
     */
    struct hpclang_object_store_iterator *
    hpclang_object_store_iterator_open(const struct hpclang_object_store *store);

    /* Initialize entry.size and entry.version before reading. Returns 1 for
     * an entry, 0 at end, or -1 with errno on error. End/error leaves entry
     * unchanged. Names are length-delimited bytes, not NUL-terminated text. */
    int hpclang_object_store_iterator_read(struct hpclang_object_store_iterator *iterator,
                                           struct hpclang_object_store_entry *entry);

    void hpclang_object_store_iterator_close(struct hpclang_object_store_iterator *iterator);

#ifdef __cplusplus
}
#endif

#endif
