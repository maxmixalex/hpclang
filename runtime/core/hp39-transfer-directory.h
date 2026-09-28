#ifndef HPCLANG_RUNTIME_HP39_TRANSFER_DIRECTORY_H
#define HPCLANG_RUNTIME_HP39_TRANSFER_DIRECTORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * HP39AscA / HP39DIR.* describe the external typed transfer/disk protocol
 * used by Conn3x/serial-disk style import/export. They are not the calculator's
 * internal Aplet/Program/Variable namespace.
 */
#define HPCLANG_HP39_TRANSFER_DIRECTORY_ENTRY_LIMIT 4096u

    enum hpclang_hp39_transfer_directory_result
    {
        HPCLANG_HP39_TRANSFER_DIRECTORY_OK = 0,
        HPCLANG_HP39_TRANSFER_DIRECTORY_INVALID = -1,
        HPCLANG_HP39_TRANSFER_DIRECTORY_MALFORMED = -2,
        HPCLANG_HP39_TRANSFER_DIRECTORY_LIMIT = -3
    };

    struct hpclang_hp39_transfer_directory_entry_view
    {
        uint8_t subtype;
        const uint8_t *filename;
        size_t filename_length;
        const uint8_t *name;
        size_t name_length;
    };

    struct hpclang_hp39_transfer_directory_cursor
    {
        const uint8_t *data;
        size_t size;
        size_t offset;
        uint32_t entries;
    };

    int hpclang_hp39_transfer_directory_cursor_open(
        const uint8_t *data, size_t size, struct hpclang_hp39_transfer_directory_cursor *cursor);

    /* Return 1 for one record, 0 at end, or a negative directory result. */
    int hpclang_hp39_transfer_directory_cursor_next(
        struct hpclang_hp39_transfer_directory_cursor *cursor,
        struct hpclang_hp39_transfer_directory_entry_view *entry);

#ifdef __cplusplus
}
#endif

#endif
