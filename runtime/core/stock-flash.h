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

/* Recover the FF-marked tail of a caller-validated native user bank (0..5).
 * Uses the recovery result/ownership contract from rpl-flash-write.h. */
int hpclang_stock_flash_recover_tail(uint32_t bank_index);

/* Finish a caller-owned pending commit against its exact expected RAM record.
 * Same verification/result contract as hpclang_rpl_flash_finish_commit. */
int hpclang_stock_flash_finish_commit(uint32_t address, const uint8_t *record, uint32_t bytes);

/* Whole-port create under one mapping lease. Returns DIRTY only for a bank
 * already malformed before mutation. Own failed writes are discarded before
 * returning FAILED. Permanently unrepairable hardware retains RAM execution
 * and ownership, just as a permanently BUSY chip does; it cannot resume Stock. */
int hpclang_stock_flash_create_backup(const uint8_t *record, uint32_t bytes);

/* Whole-port deletion with the same exclusive ownership/fault policy.
 * Only the selected backup's status is cleared; no space is reclaimed. */
int hpclang_stock_flash_remove_backup(const uint8_t *name, uint32_t name_length);
/* Keep the old value on failed append; after a verified new commit, finish
 * deleting the old status before releasing exclusive mapping ownership. */
int hpclang_stock_flash_replace_backup(const uint8_t *record, uint32_t bytes);

#endif
