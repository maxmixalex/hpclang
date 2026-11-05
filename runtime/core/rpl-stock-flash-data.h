#ifndef HPCLANG_RPL_STOCK_FLASH_DATA_H
#define HPCLANG_RPL_STOCK_FLASH_DATA_H
#include "rpl-stock-view.h"

/* Physical nibble reader restricted to mutable user Flash [0x140000,0x200000).
 * Requires the captured identity mappings and the same exclusive, no-Flash-
 * operation lifetime as the Stock view. This is data access, never immutable
 * ROM identification or code authentication. Caller records consumed values
 * as dependencies and revalidates them before later use. No Flash commands or
 * bank switching; success 1, failure 0 with value untouched. */
int hpclang_rpl_stock_flash_data_read(void *context, uint32_t address, uint8_t *value);
#endif
