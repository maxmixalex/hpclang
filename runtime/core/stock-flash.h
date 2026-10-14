#ifndef HPCLANG_STOCK_FLASH_H
#define HPCLANG_STOCK_FLASH_H

#include "sst-flash.h"

/* Internal probe. Owns/restores the Stock Flash mapping around the command.
 * Does not program or erase any array data. */
int hpclang_stock_flash_identify(struct hpclang_sst_device *device);

/* Internal physical primitives, NOT native Port object mutation. The caller
 * owns the contents of the selected word/sector in the user Flash area.
 * These keep mappings/interrupts owned until the chip finishes. A permanently
 * busy chip cannot safely return to Stock and therefore keeps polling in RAM. */
int hpclang_stock_flash_program_word(uint32_t address, uint16_t value);
int hpclang_stock_flash_erase_sector(uint32_t address);

#endif
