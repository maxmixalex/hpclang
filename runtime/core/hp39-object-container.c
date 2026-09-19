#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hp39-object-container.h"

static const uint8_t hp39_binary_magic[] = {
    'H', 'P', '3', '9', 'B', 'i', 'n'
};

int hpclang_hp39_container_parse(
    const uint8_t *data,
    size_t size,
    struct hpclang_hp39_container_view *view)
{
    if (view == NULL || (data == NULL && size != 0u))
        return HPCLANG_HP39_CONTAINER_INVALID;

    memset(view, 0, sizeof(*view));

    const size_t header_size = sizeof(hp39_binary_magic) + 2u;
    if (size < header_size || data == NULL ||
        memcmp(data, hp39_binary_magic, sizeof(hp39_binary_magic)) != 0)
        return HPCLANG_HP39_CONTAINER_MALFORMED;

    uint8_t subtype = data[sizeof(hp39_binary_magic)];
    if (subtype < 'A' || subtype > 'J')
        return HPCLANG_HP39_CONTAINER_MALFORMED;

    size_t name_length = data[sizeof(hp39_binary_magic) + 1u];
    if (name_length > size - header_size)
        return HPCLANG_HP39_CONTAINER_MALFORMED;

    view->subtype = subtype;
    view->name = data + header_size;
    view->name_length = name_length;
    view->payload = data + header_size + name_length;
    view->payload_size = size - header_size - name_length;
    return HPCLANG_HP39_CONTAINER_OK;
}
