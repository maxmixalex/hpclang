#ifndef HPCLANG_STORAGE_MEDIA_H
#define HPCLANG_STORAGE_MEDIA_H

#include <stdint.h>

enum
{
    HPCLANG_STORAGE_GPDCON = 0x30u,
    HPCLANG_STORAGE_GPDDAT = 0x34u,
    HPCLANG_STORAGE_GPFCON = 0x50u,
    HPCLANG_STORAGE_GPFDAT = 0x54u
};

uint32_t hpclang_storage_media_read32(unsigned offset);

#endif
