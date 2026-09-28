#include <stdlib.h>
#include <stdint.h>

#include "ff.h"
#include "diskio.h"

#if FFCONF_DEF != 80386
#error unexpected FatFs configuration revision
#endif
#if FF_VOLUMES != 1 || FF_MULTI_PARTITION != 0
#error M7.2c requires one logical FAT volume
#endif
#if FF_MIN_SS != 512 || FF_MAX_SS != 512
#error hp4950 SD namespace requires fixed 512-byte sectors
#endif
#if FF_USE_LFN != 1 || FF_MAX_LFN != 255 || FF_LFN_UNICODE != 2
#error hpclang requires fixed-workspace UTF-8 long-file-name support
#endif
#if FF_FS_RPATH != 0
#error relative/current-directory semantics must stay disabled
#endif
#if FF_FS_EXFAT != 0 || FF_USE_MKFS != 0
#error initial M7.2c scope excludes exFAT and formatting
#endif
#if FF_FS_NORTC != 0
#error application-visible direct FAT writes require a real clock provider
#endif
#if FF_FS_REENTRANT != 0
#error whole-process namespace lease is intentionally single-threaded
#endif

static DSTATUS drive_status = STA_NOINIT;

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0u)
        return STA_NOINIT;
    drive_status = 0u;
    return drive_status;
}

DSTATUS disk_status(BYTE pdrv)
{
    return pdrv == 0u ? drive_status : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    (void)buff;
    (void)sector;
    (void)count;
    return pdrv == 0u ? RES_NOTRDY : RES_PARERR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    (void)buff;
    (void)sector;
    (void)count;
    return pdrv == 0u ? RES_NOTRDY : RES_PARERR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0u)
        return RES_PARERR;

    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (buff == NULL)
            return RES_PARERR;
        *(WORD *)buff = 512u;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (buff == NULL)
            return RES_PARERR;
        *(DWORD *)buff = 1u;
        return RES_OK;
    case GET_SECTOR_COUNT:
        if (buff == NULL)
            return RES_PARERR;
        *(LBA_t *)buff = 0u;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

DWORD get_fattime(void)
{
    return ((DWORD)(2026u - 1980u) << 25) | ((DWORD)1u << 21) | ((DWORD)1u << 16);
}

int main(void)
{
    FATFS fs = {0};

    /* opt=0 registers the work area without touching the stub disk. */
    if (f_mount(&fs, "", 0u) != FR_OK)
        return EXIT_FAILURE;
    if (f_mount(NULL, "", 0u) != FR_OK)
        return EXIT_FAILURE;

    if (sizeof(TCHAR) != 1u)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
