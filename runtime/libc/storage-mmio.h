#ifndef HPCLANG_STORAGE_MMIO_H
#define HPCLANG_STORAGE_MMIO_H

#include <stdint.h>

uint32_t hpclang_storage_mmio_read32(uintptr_t address);
void hpclang_storage_mmio_write32(uintptr_t address, uint32_t value);

#endif
