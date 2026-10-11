#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "hp4950-fatfs-diskio.h"
#include "s3c2410-sdio-card.h"
#include "storage-mmio.h"
#include "storage-media-events.h"
#include "datetime-internal.h"

static int present = 1, read_only, query_error, transfer_error;
static int remove_after_read, protect_after_write;
static unsigned opens, closes, reads, writes, queries;
static uint32_t clock_control = 0x123;
static int media_event, remove_during_open, remove_during_write;

int hpclang_storage_media_events_acquire(struct hpclang_storage_media_events *events)
{
    assert(!events->owned);
    events->owned = 1;
    media_event = 0;
    return 0;
}
int hpclang_storage_media_events_changed(struct hpclang_storage_media_events *events)
{
    assert(events->owned);
    int changed = media_event;
    media_event = 0;
    return changed;
}
void hpclang_storage_media_events_release(struct hpclang_storage_media_events *events)
{
    assert(events->owned);
    events->owned = 0;
}

uint32_t hpclang_storage_mmio_read32(uintptr_t address)
{
    if (address == 0x0720000c)
        return clock_control;
    if (address == 0x07200010)
        return 0x10; /* Slow mode: the existing clock decoder yields 12 MHz. */
    assert(address == 0x07200014);
    return 0;
}
void hpclang_storage_mmio_write32(uintptr_t address, uint32_t value)
{
    assert(address == 0x0720000c);
    clock_control = value;
}
int hpclang_storage_media_query(int *inserted, int *protected_card)
{
    ++queries;
    *inserted = present;
    *protected_card = read_only;
    return query_error ? -1 : 0;
}
int hpclang_s3c2410_sdio_card_open(struct hpclang_s3c2410_sdio_card *card,
                                   const struct hpclang_s3c2410_sdio_bus *bus, uint32_t clock)
{
    assert(present && clock == 12000000 && bus->read32 && bus->write32);
    ++opens;
    card->initialized = 1;
    card->sector_count = 32;
    media_event |= remove_during_open;
    return HPCLANG_SDIO_OK;
}
void hpclang_s3c2410_sdio_card_close(struct hpclang_s3c2410_sdio_card *card)
{
    ++closes;
    card->initialized = 0;
}
int hpclang_s3c2410_sdio_read_sector(struct hpclang_s3c2410_sdio_card *card, uint32_t sector,
                                     void *buffer)
{
    assert(card->initialized && sector < 32);
    ++reads;
    memset(buffer, 0xa5, 512);
    if (remove_after_read)
        present = 0;
    return transfer_error ? HPCLANG_SDIO_IO_ERROR : HPCLANG_SDIO_OK;
}
int hpclang_s3c2410_sdio_write_sector(struct hpclang_s3c2410_sdio_card *card, uint32_t sector,
                                      const void *buffer)
{
    assert(card->initialized && sector < 32 && buffer);
    ++writes;
    media_event |= remove_during_write;
    if (protect_after_write)
        read_only = 1;
    return transfer_error ? HPCLANG_SDIO_IO_ERROR : HPCLANG_SDIO_OK;
}
int hpclang_rtc_read(struct hpclang_datetime *value)
{
    (void)value;
    return 0;
}

int main(void)
{
    BYTE buffer[1024];
    assert(disk_status(0) == STA_NOINIT && queries == 0);
    assert(hpclang_hp4950_fatfs_controller_acquire(0) == 0);
    assert(disk_initialize(0) == 0 && opens == 1);
    read_only = 1;
    assert(disk_status(0) == STA_PROTECT);
    assert(disk_write(0, buffer, 0, 1) == RES_WRPRT && writes == 0);
    read_only = 0;
    remove_after_read = 1;
    assert(disk_read(0, buffer, 0, 2) == RES_NOTRDY && reads == 1 && closes == 1);
    assert(disk_status(0) == (STA_NOINIT | STA_NODISK));
    assert(disk_initialize(0) & STA_NOINIT);
    assert(opens == 1);
    present = 1;
    remove_after_read = 0;
    assert(disk_status(0) == STA_NOINIT);
    assert(disk_initialize(0) == 0 && opens == 2);
    protect_after_write = 1;
    assert(disk_write(0, buffer, 0, 2) == RES_WRPRT && writes == 1);
    protect_after_write = read_only = 0;
    transfer_error = 1;
    assert(disk_read(0, buffer, 0, 1) == RES_ERROR && closes == 2);
    assert(disk_status(0) == STA_NOINIT);
    transfer_error = 0;
    assert(disk_initialize(0) == 0 && opens == 3);
    query_error = 1;
    assert(disk_status(0) == STA_NOINIT && closes == 3);
    assert(disk_initialize(0) == STA_NOINIT && opens == 3);
    query_error = 0;
    assert(disk_initialize(0) == 0 && opens == 4);
    /* The card is present throughout both observations. The hardware latch
     * remembers a complete removal/reinsertion between them. */
    media_event = 1;
    assert(disk_write(0, buffer, 0, 1) == RES_NOTRDY && writes == 1);
    assert(closes == 4);
    remove_during_open = 1;
    assert(disk_initialize(0) == STA_NOINIT && opens == 5 && closes == 5);
    remove_during_open = 0;
    assert(disk_initialize(0) == 0 && opens == 6);
    remove_during_write = 1;
    assert(disk_write(0, buffer, 0, 1) == RES_NOTRDY && writes == 2 && closes == 6);
    remove_during_write = 0;
    assert(disk_initialize(0) == 0 && opens == 7);
    hpclang_hp4950_fatfs_controller_release();
    assert(closes == 7 && clock_control == 0x123);
    assert(!hpclang_hp4950_fatfs_controller_owned());
    return 0;
}
