#ifndef HPCLANG_RUNTIME_RPL_OBJECT_STORE_H
#define HPCLANG_RUNTIME_RPL_OBJECT_STORE_H

#include <stdint.h>

#include "rpl-object-span.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define HPCLANG_RPL_STORE_NAME_CAPACITY 256u
#define HPCLANG_RPL_STORE_PATH_CAPACITY 4096u

    enum hpclang_rpl_store_result
    {
        HPCLANG_RPL_STORE_OK = 0,
        HPCLANG_RPL_STORE_UNAVAILABLE = -1,
        HPCLANG_RPL_STORE_MALFORMED = -2,
        HPCLANG_RPL_STORE_NOT_FOUND = -3,
        HPCLANG_RPL_STORE_NOT_DIRECTORY = -4,
        HPCLANG_RPL_STORE_LIMIT = -5
    };

    struct hpclang_rpl_directory_entry_view
    {
        uint32_t object_address;
        uint32_t prologue;
        uint32_t name_length;
        uint8_t name[HPCLANG_RPL_STORE_NAME_CAPACITY];
    };

    struct hpclang_rpl_directory_cursor
    {
        uint32_t directory;
        uint32_t chain;
        uint32_t item;
        uint32_t steps;
        uint32_t finished;
    };

    int hpclang_rpl_store_home_object(const struct hpclang_rpl_reader *reader,
                                      uint32_t *object_address);

    int hpclang_rpl_store_current_object(const struct hpclang_rpl_reader *reader,
                                         uint32_t *object_address);

    int hpclang_rpl_directory_cursor_open(const struct hpclang_rpl_reader *reader,
                                          uint32_t directory_object,
                                          struct hpclang_rpl_directory_cursor *cursor);

    /* Return 1 for one entry, 0 at end, or a negative store result. */
    int hpclang_rpl_directory_cursor_next(const struct hpclang_rpl_reader *reader,
                                          struct hpclang_rpl_directory_cursor *cursor,
                                          struct hpclang_rpl_directory_entry_view *entry);

    int hpclang_rpl_directory_find(const struct hpclang_rpl_reader *reader,
                                   uint32_t directory_object, const uint8_t *name,
                                   uint32_t name_length,
                                   struct hpclang_rpl_directory_entry_view *entry);

    /*
     * Encode the current directory as [u8 length][raw bytes] components relative
     * to HOME. The caller supplies storage; no Stock state is changed.
     */
    int hpclang_rpl_directory_path_to(const struct hpclang_rpl_reader *reader,
                                      uint32_t target_directory, uint8_t *encoded_path,
                                      uint32_t capacity, uint32_t *encoded_size,
                                      uint32_t *component_count);

#ifdef __cplusplus
}
#endif

#endif
