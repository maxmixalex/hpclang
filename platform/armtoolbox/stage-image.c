#include "stage-image.h"
#include <hpclang/bootstrap.h>
#include "../../third_party/tinf/tinf.h"

uint32_t hpclang_stage_image_capabilities(const struct hpclang_image_header *image,
                                          const void *payload, size_t capacity)
{
    if (!image || !payload || image->image_size > capacity || image->entry_offset < 8 ||
        image->entry_offset >= image->image_size)
        return 0;
    const uint8_t *bytes = payload;
    uint32_t magic = 0, capabilities = 0;
    for (unsigned i = 0; i < 4; ++i) {
        magic |= (uint32_t)bytes[i] << (i * 8);
        capabilities |= (uint32_t)bytes[i + 4] << (i * 8);
    }
    return magic == HPCLANG_IMAGE_CAPABILITIES_MAGIC ? capabilities : 0;
}

int hpclang_stage_image_valid(const struct hpclang_image_header *image, size_t stored_size)
{
    if (!image || stored_size <= sizeof(*image) || image->magic[0] != 'H' ||
        image->magic[1] != 'P' || image->magic[2] != 'C' || image->image_size == 0u ||
        image->image_size > HPCLANG_CODE_PAGE_CAPACITY * HPCLANG_PAGE_SIZE ||
        image->rw_static_size > HPCLANG_RW_PAGE_CAPACITY * HPCLANG_PAGE_SIZE ||
        image->entry_offset >= image->image_size)
        return 0;
    size_t payload_size = stored_size - sizeof(*image);
    return (image->magic[3] == '1' && payload_size == image->image_size) ||
           (image->magic[3] == 'Z' && payload_size < image->image_size);
}

int hpclang_stage_image_unpack(const struct hpclang_image_header *image, size_t stored_size,
                               void *destination, size_t capacity)
{
    if (!destination || !hpclang_stage_image_valid(image, stored_size) ||
        capacity < image->image_size)
        return -1;
    const unsigned char *source = (const unsigned char *)(image + 1);
    unsigned char *output = destination;
    if (image->magic[3] == 'Z') {
        unsigned int size = image->image_size;
        if (tinf_uncompress(output, &size, source, (unsigned int)(stored_size - sizeof(*image))) !=
                TINF_OK ||
            size != image->image_size)
            return -1;
    } else {
        for (size_t i = 0; i < image->image_size; ++i)
            output[i] = source[i];
    }
    for (size_t i = image->image_size; i < capacity; ++i)
        output[i] = 0;
    return 0;
}
