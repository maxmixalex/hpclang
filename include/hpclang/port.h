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
     * addresses are retained by the iterator. Currently Port 0 on classic Stock
     * is implemented; other valid ports/contexts return ENOSYS. This does not
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
     * objects remain at their original addresses. Currently Port 0 only. */
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
