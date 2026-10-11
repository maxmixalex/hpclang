#ifndef HPCLANG_S3C2410_SDIO_CARD_H
#define HPCLANG_S3C2410_SDIO_CARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct hpclang_s3c2410_sdio_bus
    {
        uint32_t (*read32)(void *context, uint32_t offset);
        void (*write32)(void *context, uint32_t offset, uint32_t value);
        void *context;
    };

    struct hpclang_s3c2410_sdio_card
    {
        struct hpclang_s3c2410_sdio_bus bus;
        uint32_t pclk_hz;
        uint32_t rca;
        uint32_t sector_count;
        uint8_t cid[16];
        uint8_t high_capacity;
        uint8_t initialized;
    };

    enum hpclang_s3c2410_sdio_result
    {
        HPCLANG_SDIO_OK = 0,
        HPCLANG_SDIO_IO_ERROR = -1,
        HPCLANG_SDIO_TIMEOUT = -2,
        HPCLANG_SDIO_RESPONSE_ERROR = -3,
        HPCLANG_SDIO_DATA_ERROR = -4,
        HPCLANG_SDIO_PROTOCOL_ERROR = -5,
        HPCLANG_SDIO_RANGE_ERROR = -6
    };

/* S3C2410 SDI register offsets. */
#define HPCLANG_S3C2410_SDICON UINT32_C(0x00)
#define HPCLANG_S3C2410_SDIPRE UINT32_C(0x04)
#define HPCLANG_S3C2410_SDICMDARG UINT32_C(0x08)
#define HPCLANG_S3C2410_SDICMDCON UINT32_C(0x0c)
#define HPCLANG_S3C2410_SDICMDSTAT UINT32_C(0x10)
#define HPCLANG_S3C2410_SDIRSP0 UINT32_C(0x14)
#define HPCLANG_S3C2410_SDIRSP1 UINT32_C(0x18)
#define HPCLANG_S3C2410_SDIRSP2 UINT32_C(0x1c)
#define HPCLANG_S3C2410_SDIRSP3 UINT32_C(0x20)
#define HPCLANG_S3C2410_SDITIMER UINT32_C(0x24)
#define HPCLANG_S3C2410_SDIBSIZE UINT32_C(0x28)
#define HPCLANG_S3C2410_SDIDCON UINT32_C(0x2c)
#define HPCLANG_S3C2410_SDIDCNT UINT32_C(0x30)
#define HPCLANG_S3C2410_SDIDSTA UINT32_C(0x34)
#define HPCLANG_S3C2410_SDIFSTA UINT32_C(0x38)
#define HPCLANG_S3C2410_SDIDATA UINT32_C(0x3c)
#define HPCLANG_S3C2410_SDIIMSK UINT32_C(0x40)

    int hpclang_s3c2410_sdio_card_open(struct hpclang_s3c2410_sdio_card *card,
                                       const struct hpclang_s3c2410_sdio_bus *bus,
                                       uint32_t pclk_hz);

    void hpclang_s3c2410_sdio_card_close(struct hpclang_s3c2410_sdio_card *card);

    int hpclang_s3c2410_sdio_read_sector(struct hpclang_s3c2410_sdio_card *card, uint32_t sector,
                                         void *buffer);

    int hpclang_s3c2410_sdio_write_sector(struct hpclang_s3c2410_sdio_card *card, uint32_t sector,
                                          const void *buffer);

#ifdef __cplusplus
}
#endif

#endif
