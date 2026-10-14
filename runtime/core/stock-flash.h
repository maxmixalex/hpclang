#ifndef HPCLANG_STOCK_FLASH_H
#define HPCLANG_STOCK_FLASH_H

#include "sst-flash.h"
#include "rpl-flash-write.h"

/* Internal probe. Owns/restores the Stock Flash mapping around the command.
 * Does not program or erase any array data. */
int hpclang_stock_flash_identify(struct hpclang_sst_device *device);

/* Internal physical primitives, NOT native Port object mutation. The caller
 * owns the contents of the selected word/sector in the user Flash area.
 * These keep mappings/interrupts owned until the chip finishes. A permanently
 * busy chip cannot safely return to Stock and therefore keeps polling in RAM. */
int hpclang_stock_flash_program_word(uint32_t address, uint16_t value);
/* Preflight the entire nonempty, halfword-aligned range before programming.
 * data must remain in RAM for the call. A rejected 0-to-1 transition writes
 * nothing; a hardware failure can leave a partially programmed range.
 * This is a physical operation, not an atomic native-object transaction. */
int hpclang_stock_flash_program(uint32_t address, const uint8_t *data, uint32_t bytes);
int hpclang_stock_flash_erase_sector(uint32_t address);

/* Internal native backup append under exclusive Flash mapping ownership.
 * Uses rpl-flash-write result codes and its reservation/uniqueness contract.
 * A DIRTY result requires recovery before allowing native Flash mutation. */
int hpclang_stock_flash_append(uint32_t address, const uint8_t *record, uint32_t bytes);

#endif
