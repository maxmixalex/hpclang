#include <stdlib.h>
#include <stdint.h>

#include "s3c2410-sdio-core.h"

static int test_prescaler(void)
{
    /* 66 MHz PCLK -> <= 400 kHz initialization clock. */
    uint8_t slow = hpclang_s3c2410_sdio_prescaler(66000000u, 400000u);
    uint32_t slow_hz = 66000000u / (2u * ((uint32_t)slow + 1u));
    if (slow_hz > 400000u)
        return 0;

    /* Fast clock request above the input-derived maximum clamps to /2. */
    if (hpclang_s3c2410_sdio_prescaler(50000000u, 50000000u) != 0u)
        return 0;

    /* Very slow target clamps to the largest supported divisor. */
    if (hpclang_s3c2410_sdio_prescaler(100000000u, 1u) != 255u)
        return 0;

    return hpclang_s3c2410_sdio_prescaler(0u, 400000u) == 0xffu &&
           hpclang_s3c2410_sdio_prescaler(66000000u, 0u) == 0xffu;
}

static int test_commands(void)
{
    uint32_t cmd0 = hpclang_s3c2410_sdio_command_control(
        0u, HPCLANG_SDIO_RESPONSE_NONE, 0);
    if (cmd0 !=
        (HPCLANG_S3C2410_SDICMDCON_SENDERHOST |
         HPCLANG_S3C2410_SDICMDCON_CMDSTART))
        return 0;

    uint32_t cmd17 = hpclang_s3c2410_sdio_command_control(
        17u, HPCLANG_SDIO_RESPONSE_SHORT, 1);
    if ((cmd17 & HPCLANG_S3C2410_SDICMDCON_INDEX_MASK) != 17u ||
        (cmd17 & HPCLANG_S3C2410_SDICMDCON_WAITRSP) == 0u ||
        (cmd17 & HPCLANG_S3C2410_SDICMDCON_WITHDATA) == 0u ||
        (cmd17 & HPCLANG_S3C2410_SDICMDCON_LONGRSP) != 0u)
        return 0;

    uint32_t cmd2 = hpclang_s3c2410_sdio_command_control(
        2u, HPCLANG_SDIO_RESPONSE_LONG, 0);
    return (cmd2 & HPCLANG_S3C2410_SDICMDCON_LONGRSP) != 0u &&
           (cmd2 & HPCLANG_S3C2410_SDICMDCON_WAITRSP) != 0u;
}

static int test_data_control(void)
{
    uint32_t read = hpclang_s3c2410_sdio_data_control(
        HPCLANG_SDIO_DATA_READ, 1u);
    if (read !=
        (HPCLANG_S3C2410_SDIDCON_BLOCKMODE |
         HPCLANG_S3C2410_SDIDCON_RXAFTERCMD |
         HPCLANG_S3C2410_SDIDCON_XFER_RXSTART |
         1u))
        return 0;

    uint32_t write = hpclang_s3c2410_sdio_data_control(
        HPCLANG_SDIO_DATA_WRITE, 7u);
    if (write !=
        (HPCLANG_S3C2410_SDIDCON_BLOCKMODE |
         HPCLANG_S3C2410_SDIDCON_TXAFTERRESP |
         HPCLANG_S3C2410_SDIDCON_XFER_TXSTART |
         7u))
        return 0;

    return hpclang_s3c2410_sdio_data_control(
               HPCLANG_SDIO_DATA_NONE, 1u) == 0u &&
           hpclang_s3c2410_sdio_data_control(
               HPCLANG_SDIO_DATA_READ, 0u) == 0u;
}

static int test_addressing(void)
{
    uint32_t argument = 0u;

    if (!hpclang_s3c2410_sdio_sector_argument(123u, 1, &argument) ||
        argument != 123u)
        return 0;
    if (!hpclang_s3c2410_sdio_sector_argument(123u, 0, &argument) ||
        argument != 123u * 512u)
        return 0;
    if (hpclang_s3c2410_sdio_sector_argument(
            UINT32_MAX / 512u + 1u, 0, &argument))
        return 0;

    return !hpclang_s3c2410_sdio_sector_argument(0u, 0, 0);
}

int main(void)
{
    return test_prescaler() &&
           test_commands() &&
           test_data_control() &&
           test_addressing()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
