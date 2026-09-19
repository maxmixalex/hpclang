#ifndef HPCLANG_S3C2410_SDIO_CORE_H
#define HPCLANG_S3C2410_SDIO_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hpclang_sdio_response {
    HPCLANG_SDIO_RESPONSE_NONE = 0,
    HPCLANG_SDIO_RESPONSE_SHORT = 1,
    HPCLANG_SDIO_RESPONSE_LONG = 2
};

enum hpclang_sdio_data_direction {
    HPCLANG_SDIO_DATA_NONE = 0,
    HPCLANG_SDIO_DATA_READ = 1,
    HPCLANG_SDIO_DATA_WRITE = 2
};

/* S3C2410 SDICON */
#define HPCLANG_S3C2410_SDICON_CLOCKTYPE   (UINT32_C(1) << 0)
#define HPCLANG_S3C2410_SDICON_FIFORESET   (UINT32_C(1) << 1)
#define HPCLANG_S3C2410_SDICON_BYTEORDER   (UINT32_C(1) << 4)

/* S3C2410 SDICMDCON */
#define HPCLANG_S3C2410_SDICMDCON_INDEX_MASK UINT32_C(0x3f)
#define HPCLANG_S3C2410_SDICMDCON_SENDERHOST (UINT32_C(1) << 6)
#define HPCLANG_S3C2410_SDICMDCON_CMDSTART   (UINT32_C(1) << 8)
#define HPCLANG_S3C2410_SDICMDCON_WAITRSP    (UINT32_C(1) << 9)
#define HPCLANG_S3C2410_SDICMDCON_LONGRSP    (UINT32_C(1) << 10)
#define HPCLANG_S3C2410_SDICMDCON_WITHDATA   (UINT32_C(1) << 11)

/* S3C2410 SDICMDSTAT */
#define HPCLANG_S3C2410_SDICMDSTAT_RSPFIN     (UINT32_C(1) << 9)
#define HPCLANG_S3C2410_SDICMDSTAT_CMDTIMEOUT (UINT32_C(1) << 10)
#define HPCLANG_S3C2410_SDICMDSTAT_CMDSENT    (UINT32_C(1) << 11)
#define HPCLANG_S3C2410_SDICMDSTAT_CRCFAIL    (UINT32_C(1) << 12)

/* S3C2410 SDIDCON */
#define HPCLANG_S3C2410_SDIDCON_XFER_RXSTART  (UINT32_C(2) << 12)
#define HPCLANG_S3C2410_SDIDCON_XFER_TXSTART  (UINT32_C(3) << 12)
#define HPCLANG_S3C2410_SDIDCON_BLOCKMODE      (UINT32_C(1) << 17)
#define HPCLANG_S3C2410_SDIDCON_RXAFTERCMD     (UINT32_C(1) << 19)
#define HPCLANG_S3C2410_SDIDCON_TXAFTERRESP    (UINT32_C(1) << 20)
#define HPCLANG_S3C2410_SDIDCON_BLOCK_COUNT_MASK UINT32_C(0x0fff)

/* S3C2410 SDIDSTA */
#define HPCLANG_S3C2410_SDIDSTA_RXDATAON     (UINT32_C(1) << 0)
#define HPCLANG_S3C2410_SDIDSTA_TXDATAON     (UINT32_C(1) << 1)
#define HPCLANG_S3C2410_SDIDSTA_BUSYFINISH   (UINT32_C(1) << 3)
#define HPCLANG_S3C2410_SDIDSTA_XFERFINISH   (UINT32_C(1) << 4)
#define HPCLANG_S3C2410_SDIDSTA_DATATIMEOUT  (UINT32_C(1) << 5)
#define HPCLANG_S3C2410_SDIDSTA_RXCRCFAIL    (UINT32_C(1) << 6)
#define HPCLANG_S3C2410_SDIDSTA_CRCFAIL      (UINT32_C(1) << 7)
#define HPCLANG_S3C2410_SDIDSTA_FIFOFAIL     (UINT32_C(1) << 8)

/*
 * SDIPRE is an 8-bit divisor field. On S3C2410, SDCLK is derived from PCLK as
 * PCLK / (2 * (prescaler + 1)). Return 0xff when target_hz is zero; callers
 * use that as "clock disabled / invalid request".
 */
uint8_t hpclang_s3c2410_sdio_prescaler(
    uint32_t pclk_hz,
    uint32_t target_hz);

uint32_t hpclang_s3c2410_sdio_command_control(
    uint8_t command,
    enum hpclang_sdio_response response,
    int with_data);

uint32_t hpclang_s3c2410_sdio_data_control(
    enum hpclang_sdio_data_direction direction,
    uint16_t blocks);

/*
 * SDSC cards use byte addressing; SDHC/SDXC cards use 512-byte block
 * addressing. The block backend itself is always expressed in 512-byte LBAs.
 * Returns 0 on overflow/invalid input and writes the command argument to out.
 */
int hpclang_s3c2410_sdio_sector_argument(
    uint32_t sector,
    int high_capacity,
    uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif
