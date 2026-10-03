#include <stddef.h>
#include <stdint.h>

#include "../core/datetime-internal.h"

#include "../../third_party/fatfs/ff.h"
#include "../../third_party/fatfs/diskio.h"
#include "hp4950-fatfs-diskio.h"
#include "s3c2410-sdio-card.h"

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define MPLLCON REG32(UINT32_C(0x07200004))
#define CLKCON REG32(UINT32_C(0x0720000c))
#define CLKSLOW REG32(UINT32_C(0x07200010))
#define CLKDIVN REG32(UINT32_C(0x07200014))

#define SDI_BASE UINT32_C(0x07e00000)
#define CLKCON_SDI (UINT32_C(1) << 9)
#define XTAL_HZ UINT32_C(12000000)

struct hp4950_fatfs_state
{
    struct hpclang_s3c2410_sdio_card card;
    uint32_t saved_clkcon;
    uint8_t owned;
    uint8_t initialized;
    uint8_t read_only;
};

static struct hp4950_fatfs_state state;

static uint32_t mmio_read32(void *context, uint32_t offset)
{
    (void)context;
    return REG32(SDI_BASE + offset);
}

static void mmio_write32(void *context, uint32_t offset, uint32_t value)
{
    (void)context;
    REG32(SDI_BASE + offset) = value;
}

static uint32_t hp4950_pclk_hz(void)
{
    uint64_t numerator = XTAL_HZ;
    uint64_t denominator = 1u;

    uint32_t clkslow = CLKSLOW;
    uint32_t clkdivn = CLKDIVN;

    if ((clkslow & (UINT32_C(1) << 4)) != 0u) {
        uint32_t slow = clkslow & UINT32_C(0x7);
        if (slow != 0u)
            denominator *= (uint64_t)slow * 2u;
    } else {
        uint32_t mpllcon = MPLLCON;
        uint32_t m = ((mpllcon >> 12) & UINT32_C(0xff)) + 8u;
        uint32_t p = ((mpllcon >> 4) & UINT32_C(0x3f)) + 2u;
        uint32_t s = mpllcon & UINT32_C(0x3);
        numerator *= m;
        denominator *= (uint64_t)p << s;
    }

    if ((clkdivn & (UINT32_C(1) << 1)) != 0u)
        denominator *= 2u;
    if ((clkdivn & (UINT32_C(1) << 0)) != 0u)
        denominator *= 2u;

    if (denominator == 0u)
        return 0u;
    return (uint32_t)(numerator / denominator);
}

int hpclang_hp4950_fatfs_controller_acquire(int read_only)
{
    if (state.owned)
        return -1;

    state.saved_clkcon = CLKCON;
    CLKCON = state.saved_clkcon | CLKCON_SDI;
    state.owned = 1u;
    state.initialized = 0u;
    state.read_only = read_only != 0;
    return 0;
}

void hpclang_hp4950_fatfs_controller_release(void)
{
    if (!state.owned)
        return;

    if (state.initialized)
        hpclang_s3c2410_sdio_card_close(&state.card);

    state.initialized = 0u;
    state.read_only = 0u;
    CLKCON = state.saved_clkcon;
    state.owned = 0u;
}

int hpclang_hp4950_fatfs_controller_owned(void)
{
    return state.owned != 0u;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0u || !state.owned)
        return STA_NOINIT;
    if (state.initialized)
        return state.read_only ? STA_PROTECT : 0u;

    uint32_t pclk_hz = hp4950_pclk_hz();
    if (pclk_hz == 0u)
        return STA_NOINIT;

    const struct hpclang_s3c2410_sdio_bus bus = {
        .read32 = mmio_read32,
        .write32 = mmio_write32,
        .context = 0,
    };
    if (hpclang_s3c2410_sdio_card_open(&state.card, &bus, pclk_hz) != HPCLANG_SDIO_OK)
        return STA_NOINIT;

    state.initialized = 1u;
    return state.read_only ? STA_PROTECT : 0u;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0u || !state.owned || !state.initialized)
        return STA_NOINIT;
    return state.read_only ? STA_PROTECT : 0u;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0u || buff == 0 || count == 0u)
        return RES_PARERR;
    if ((disk_status(pdrv) & STA_NOINIT) != 0u)
        return RES_NOTRDY;

    for (UINT i = 0u; i < count; ++i) {
        uint64_t current = (uint64_t)sector + i;
        if (current > UINT32_MAX)
            return RES_PARERR;
        if (hpclang_s3c2410_sdio_read_sector(&state.card, (uint32_t)current,
                                             buff + (size_t)i * 512u) != HPCLANG_SDIO_OK)
            return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0u || buff == 0 || count == 0u)
        return RES_PARERR;
    DSTATUS status = disk_status(pdrv);
    if ((status & STA_NOINIT) != 0u)
        return RES_NOTRDY;
    if ((status & STA_PROTECT) != 0u)
        return RES_WRPRT;

    for (UINT i = 0u; i < count; ++i) {
        uint64_t current = (uint64_t)sector + i;
        if (current > UINT32_MAX)
            return RES_PARERR;
        if (hpclang_s3c2410_sdio_write_sector(&state.card, (uint32_t)current,
                                              buff + (size_t)i * 512u) != HPCLANG_SDIO_OK)
            return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0u)
        return RES_PARERR;
    if ((disk_status(pdrv) & STA_NOINIT) != 0u)
        return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        if (buff == 0)
            return RES_PARERR;
        *(LBA_t *)buff = (LBA_t)state.card.sector_count;
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (buff == 0)
            return RES_PARERR;
        *(WORD *)buff = 512u;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (buff == 0)
            return RES_PARERR;
        *(DWORD *)buff = 1u;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

DWORD get_fattime(void)
{
    struct hpclang_datetime now;
    if (hpclang_rtc_read(&now)) {
        return ((DWORD)(now.year - 1980u) << 25) | ((DWORD)now.month << 21) |
               ((DWORD)now.day << 16) | ((DWORD)now.hour << 11) | ((DWORD)now.minute << 5) |
               ((DWORD)now.second >> 1);
    }

    /*
     * A corrupt/uninitialized RTC must not prevent a FAT update. This value is
     * only the invalid-RTC fallback; normal writes use the calculator RTC.
     */
    return ((DWORD)(2026u - 1980u) << 25) | ((DWORD)1u << 21) | ((DWORD)1u << 16);
}
