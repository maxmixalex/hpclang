#ifndef HPCLANG_RUNTIME_HP39_OBJECT_CONTAINER_H
#define HPCLANG_RUNTIME_HP39_OBJECT_CONTAINER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hpclang_hp39_container_result {
    HPCLANG_HP39_CONTAINER_OK = 0,
    HPCLANG_HP39_CONTAINER_INVALID = -1,
    HPCLANG_HP39_CONTAINER_MALFORMED = -2
};

struct hpclang_hp39_container_view {
    uint8_t subtype;
    const uint8_t *name;
    size_t name_length;
    const uint8_t *payload;
    size_t payload_size;
};

/*
 * Parse one concrete HP39Bin backing-object container.
 *
 * Stock uses HP39Bin followed by a concrete subtype A..J, then one byte of
 * name length, the calculator-name bytes and the remaining native payload.
 * Subtype K is the transfer-directory wildcard and is not a concrete backing
 * object. The caller owns the source bytes and therefore also the returned
 * slices.
 */
int hpclang_hp39_container_parse(
    const uint8_t *data,
    size_t size,
    struct hpclang_hp39_container_view *view);

#ifdef __cplusplus
}
#endif

#endif
