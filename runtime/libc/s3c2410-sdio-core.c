#include "s3c2410-sdio-core.h"

uint8_t hpclang_s3c2410_sdio_prescaler(uint32_t pclk_hz, uint32_t target_hz)
{
    if (pclk_hz == 0u || target_hz == 0u)
        return 0xffu;

    /*
     * ceil(PCLK / (2 * target)) - 1 guarantees that the resulting SDCLK does
     * not exceed target_hz. Clamp to the controller's 8-bit prescaler.
     */
    uint64_t denominator = (uint64_t)target_hz * 2u;
    uint64_t divisor = ((uint64_t)pclk_hz + denominator - 1u) / denominator;

    if (divisor == 0u)
        divisor = 1u;
    if (divisor > 256u)
        divisor = 256u;

    return (uint8_t)(divisor - 1u);
}

uint32_t hpclang_s3c2410_sdio_command_control(uint8_t command, enum hpclang_sdio_response response,
                                              int with_data)
{
    uint32_t control = ((uint32_t)command & HPCLANG_S3C2410_SDICMDCON_INDEX_MASK) |
                       HPCLANG_S3C2410_SDICMDCON_SENDERHOST | HPCLANG_S3C2410_SDICMDCON_CMDSTART;

    if (response != HPCLANG_SDIO_RESPONSE_NONE)
        control |= HPCLANG_S3C2410_SDICMDCON_WAITRSP;
    if (response == HPCLANG_SDIO_RESPONSE_LONG)
        control |= HPCLANG_S3C2410_SDICMDCON_LONGRSP;
    if (with_data)
        control |= HPCLANG_S3C2410_SDICMDCON_WITHDATA;

    return control;
}

uint32_t hpclang_s3c2410_sdio_data_control(enum hpclang_sdio_data_direction direction,
                                           uint16_t blocks)
{
    if (direction == HPCLANG_SDIO_DATA_NONE || blocks == 0u ||
        blocks > HPCLANG_S3C2410_SDIDCON_BLOCK_COUNT_MASK)
        return 0u;

    uint32_t control = HPCLANG_S3C2410_SDIDCON_BLOCKMODE |
                       ((uint32_t)blocks & HPCLANG_S3C2410_SDIDCON_BLOCK_COUNT_MASK);

    if (direction == HPCLANG_SDIO_DATA_READ) {
        control |= HPCLANG_S3C2410_SDIDCON_RXAFTERCMD | HPCLANG_S3C2410_SDIDCON_XFER_RXSTART;
    } else if (direction == HPCLANG_SDIO_DATA_WRITE) {
        control |= HPCLANG_S3C2410_SDIDCON_TXAFTERRESP | HPCLANG_S3C2410_SDIDCON_XFER_TXSTART;
    } else {
        return 0u;
    }

    return control;
}

int hpclang_s3c2410_sdio_sector_argument(uint32_t sector, int high_capacity, uint32_t *out)
{
    if (out == 0)
        return 0;

    if (high_capacity) {
        *out = sector;
        return 1;
    }

    if (sector > UINT32_MAX / 512u)
        return 0;

    *out = sector * 512u;
    return 1;
}
