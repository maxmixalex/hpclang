#ifndef HPCLANG_PORT_H
#define HPCLANG_PORT_H

#include <stdint.h>
#include <hpclang/object.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HPCLANG_PORT_ENTRY_VERSION 1u
#define HPCLANG_PORT_NAME_CAPACITY 256u
#define HPCLANG_PORT_INFO_VERSION 1u

    enum hpclang_port_medium
    {
        HPCLANG_PORT_ABSENT = 0,
        HPCLANG_PORT_MAIN_RAM = 1,
        HPCLANG_PORT_EXTENDED_RAM = 2,
        HPCLANG_PORT_FLASH = 3
    };

    enum hpclang_port_operation
    {
        HPCLANG_PORT_ENUMERATE = 1u << 0,
        HPCLANG_PORT_READ_BACKUP = 1u << 1,
        HPCLANG_PORT_READ_LIBRARY = 1u << 2,
        HPCLANG_PORT_CREATE_BACKUP = 1u << 3,
        HPCLANG_PORT_REPLACE_BACKUP = 1u << 4,
        HPCLANG_PORT_REMOVE_BACKUP = 1u << 5
    };

    struct hpclang_port_info
    {
        uint32_t size;
        uint32_t version;
        uint32_t port;
        uint32_t medium;
        uint32_t implemented;
        uint32_t available;
    };

    /* Set size/version before querying. medium describes the native model;
     * implemented lists SDK operations, available additionally checks launch
     * context and required ownership bounds. No hardware access/allocation.
     * Availability does not promise a valid live mapping, free space or safe
     * relocation of every object. Each operation still reports its own errors.
     * A known absent port succeeds with ABSENT and zero operation masks.
     * Unknown models return ENOSYS; invalid index/descriptor returns EINVAL.
     * Failure preserves output; success preserves errno and the caller's size.
     */
    int hpclang_port_query(uint32_t port, struct hpclang_port_info *info);

    enum hpclang_port_entry_kind
    {
        HPCLANG_PORT_BACKUP = 1,
        HPCLANG_PORT_LIBRARY = 2
    };

    struct hpclang_port_iterator;

    struct hpclang_port_entry
    {
        uint32_t size;
        uint32_t version;
        uint32_t kind;
        uint32_t library_number;
        uint32_t name_length;
        uint8_t name[HPCLANG_PORT_NAME_CAPACITY];
    };

    /* Snapshot port metadata. Names/titles are raw calculator bytes, not UTF-8.
     * Libraries are identified by number, backups by exact name. No native
     * addresses are retained by the iterator. Currently classic Stock Port 0
     * and HP49g+/HP50g Stock Port 1/2 reads are implemented; other valid
     * ports/contexts return ENOSYS. This does not
     * imply that the calculator lacks those ports. Indices above 2 return EINVAL.
     */
    struct hpclang_port_iterator *hpclang_port_iterator_open(uint32_t port);
    /* Initialize entry.size and entry.version. Returns 1/0/-1 for entry/end/error. */
    int hpclang_port_iterator_read(struct hpclang_port_iterator *iterator,
                                   struct hpclang_port_entry *entry);
    void hpclang_port_iterator_close(struct hpclang_port_iterator *iterator);

    /* Copy the value enclosed in a named backup into an owned object. The caller
     * releases it with hpclang_object_release(). Missing names return ENOENT;
     * malformed encodings return EILSEQ, unreadable mappings EIO. */
    struct hpclang_object *hpclang_port_backup_get(uint32_t port, const uint8_t *name,
                                                   uint32_t name_length);

    /* Copy a complete library encoding by its 12-bit native library number.
     * This does not attach, install or execute the library. */
    struct hpclang_object *hpclang_port_library_get(uint32_t port, uint32_t library_number);

    /* Create a new named backup from an owned snapshot. An existing exact name
     * returns EEXIST; insufficient native space returns ENOSPC. Existing port
     * objects remain at their original addresses. Supports classic Stock Port 0
     * and HP49g+/HP50g Stock Port 1. Port 1 requires a launcher-provided ERAM
     * ownership boundary; older launchers without it return ENOSYS. */
    int hpclang_port_backup_create(uint32_t port, const uint8_t *name, uint32_t name_length,
                                   const struct hpclang_object *value);

    /* Replace/remove an existing backup; missing names return ENOENT. Live
     * calculator references retain the old value. Currently Port 0 only;
     * operations requiring relocation of a preceding library return ENOTSUP.
     * Replacement never implicitly creates a missing backup. */
    int hpclang_port_backup_replace(uint32_t port, const uint8_t *name, uint32_t name_length,
                                    const struct hpclang_object *value);
    int hpclang_port_backup_remove(uint32_t port, const uint8_t *name, uint32_t name_length);

#ifdef __cplusplus
}
#endif
#endif
