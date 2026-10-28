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
        HPCLANG_PORT_REMOVE_BACKUP = 1u << 5,
        HPCLANG_PORT_CREATE_LIBRARY = 1u << 6
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
     * Opening Port 2 reads/enumeration first recovers recognized interrupted
     * Flash writes and replacement intents. This may write recovery metadata;
     * unknown corruption fails before writes. Existing owned snapshots remain
     * valid. Capability queries and reads of an already-open iterator do not
     * initiate recovery. This is not a calculator power-on hook.
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

    /* Store a complete owned library snapshot in classic Stock Port 0 or
     * HP49g+/HP50g Stock Port 1/2. Port 1 requires launcher-proven ERAM ownership
     * bounds and appends without relocating existing records. Its
     * embedded library number identifies it; an existing number in that port
     * returns EEXIST. This does not register, attach or execute the library.
     * Existing port records keep their addresses. Invalid encoding/CRC returns
     * EILSEQ, insufficient native space ENOSPC, and a non-library value EINVAL.
     * Port 2 validates all banks and appends to erased space without reclamation.
     * Other valid contexts return ENOSYS. The caller retains its snapshot.
     * Port 0 create/replace/remove stop on failure after a native-root change
     * or Saturn write attempt; root snapshot allocation failure returns ENOMEM.
     * An unexpected Port 1 backend failure during record publication stops the
     * runtime with ownership retained; it does not return a recoverable error.
     */
    int hpclang_port_library_create(uint32_t port, const struct hpclang_object *library);

    /* Port 2 access first recovers validated interrupted backups and marked
     * incomplete tails under the same exclusive Flash ownership. Recovery may
     * remain committed even if the requested operation then reports EEXIST,
     * ENOENT or ENOSPC. Unknown corruption and partial libraries are rejected;
     * duplicate replacements are resolved only with a validated durable intent.
     * No space is reclaimed. Reads and enumeration use the same recovery;
     * library creation and all backup mutations also recover before writing. */

    /* Create a new named backup from an owned snapshot. An existing exact name
     * returns EEXIST; insufficient native space returns ENOSPC. Existing port
     * objects remain at their original addresses. Supports classic Stock Port 0
     * and HP49g+/HP50g Stock Port 1/2. Port 1 requires a launcher-provided ERAM
     * ownership boundary; older launchers without it return ENOSYS. A Port 1
     * backend failure before publication returns EIO; a partially published
     * record stops the runtime without cleanup or Stock return. Port 2 uses
     * erased tail space without compaction; deleted slots are not reclaimed.
     * Unrecoverable existing Flash data is rejected before writing. Returned
     * Flash IO errors leave the attempted slot absent/deleted; permanently busy or
     * unrepairable hardware retains RAM execution instead of resuming Stock.
     * This is synchronous create, not a power-loss/restart recovery guarantee. */
    int hpclang_port_backup_create(uint32_t port, const uint8_t *name, uint32_t name_length,
                                   const struct hpclang_object *value);

    /* Replace an existing backup; missing names return ENOENT. Live
     * calculator references retain the old value. Supports Port 0 and
     * HP49g+/HP50g Stock Port 2. Port 0 operations requiring relocation of a
     * preceding library return ENOTSUP. Port 2 reserves a 44-byte transaction
     * record plus the new backup, records the intent before writing the new
     * value, then tombstones the old one, preserving its payload and pointers.
     * Insufficient erased tail space returns ENOSPC without deleting the old
     * value. Recovery determines whether an uncertain write retained the old
     * value or committed the new one before returning that outcome. Permanent
     * hardware failure retains RAM execution as with creation. No reclamation
     * or automatic power-on recovery. Replacement never creates a missing backup. */
    int hpclang_port_backup_replace(uint32_t port, const uint8_t *name, uint32_t name_length,
                                    const struct hpclang_object *value);
    /* Remove an existing backup; missing names return ENOENT. Supports Port 0
     * and HP49g+/HP50g Stock Port 2. Port 0 preserves live references as above.
     * After recovery, Port 2 validates the entire port and clears the selected
     * status: payload and native references stay put; no space is reclaimed.
     * Duplicate matching names or malformed banks return EILSEQ before the
     * requested deletion. An
     * uncertain status write is completed before return; permanent hardware
     * failure retains ownership/RAM execution as with Flash creation. This is
     * not a power-loss guarantee. Owned snapshots remain valid after removal. */
    int hpclang_port_backup_remove(uint32_t port, const uint8_t *name, uint32_t name_length);

#ifdef __cplusplus
}
#endif
#endif
