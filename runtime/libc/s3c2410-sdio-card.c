#include <stddef.h>
#include <string.h>

#include "s3c2410-sdio-card.h"
#include "s3c2410-sdio-core.h"

#define SDIO_POLL_LIMIT UINT32_C(100000)
#define SDIO_INIT_RETRIES UINT32_C(1000)
#define SDIO_SECTOR_SIZE UINT32_C(512)

#define SDIFSTA_TFHALF   (UINT32_C(1) << 11)
#define SDIFSTA_TFEMPTY  (UINT32_C(1) << 10)
#define SDIFSTA_COUNTMASK UINT32_C(0x7f)

#define SDIDSTA_ERROR_MASK \
    (HPCLANG_S3C2410_SDIDSTA_FIFOFAIL | \
     HPCLANG_S3C2410_SDIDSTA_CRCFAIL | \
     HPCLANG_S3C2410_SDIDSTA_RXCRCFAIL | \
     HPCLANG_S3C2410_SDIDSTA_DATATIMEOUT)

#define SDIDSTA_CLEAR_MASK \
    (HPCLANG_S3C2410_SDIDSTA_BUSYFINISH | \
     HPCLANG_S3C2410_SDIDSTA_XFERFINISH | \
     HPCLANG_S3C2410_SDIDSTA_DATATIMEOUT | \
     HPCLANG_S3C2410_SDIDSTA_RXCRCFAIL | \
     HPCLANG_S3C2410_SDIDSTA_CRCFAIL | \
     HPCLANG_S3C2410_SDIDSTA_FIFOFAIL)

struct command_result {
    uint32_t response[4];
};

static uint32_t bus_read(
    const struct hpclang_s3c2410_sdio_card *card,
    uint32_t offset)
{
    return card->bus.read32(card->bus.context, offset);
}

static void bus_write(
    const struct hpclang_s3c2410_sdio_card *card,
    uint32_t offset,
    uint32_t value)
{
    card->bus.write32(card->bus.context, offset, value);
}

static int send_command(
    struct hpclang_s3c2410_sdio_card *card,
    uint8_t command,
    uint32_t argument,
    enum hpclang_sdio_response response,
    int with_data,
    int response_has_crc,
    struct command_result *result)
{
    uint32_t wait_bit = response == HPCLANG_SDIO_RESPONSE_NONE
        ? HPCLANG_S3C2410_SDICMDSTAT_CMDSENT
        : HPCLANG_S3C2410_SDICMDSTAT_RSPFIN;

    bus_write(card, HPCLANG_S3C2410_SDICMDSTAT, UINT32_C(0x0f00));
    bus_write(card, HPCLANG_S3C2410_SDICMDARG, argument);
    bus_write(
        card,
        HPCLANG_S3C2410_SDICMDCON,
        hpclang_s3c2410_sdio_command_control(command, response, with_data));

    uint32_t status = 0u;
    uint32_t polls;
    for (polls = 0u; polls < SDIO_POLL_LIMIT; ++polls) {
        status = bus_read(card, HPCLANG_S3C2410_SDICMDSTAT);
        if ((status & HPCLANG_S3C2410_SDICMDSTAT_CMDTIMEOUT) != 0u) {
            bus_write(card, HPCLANG_S3C2410_SDICMDSTAT, UINT32_C(0x0f00));
            return HPCLANG_SDIO_TIMEOUT;
        }
        if ((status & wait_bit) != 0u)
            break;
    }

    if (polls == SDIO_POLL_LIMIT) {
        bus_write(card, HPCLANG_S3C2410_SDICMDSTAT, UINT32_C(0x0f00));
        return HPCLANG_SDIO_TIMEOUT;
    }

    if (response != HPCLANG_SDIO_RESPONSE_NONE &&
        response_has_crc &&
        (status & HPCLANG_S3C2410_SDICMDSTAT_CRCFAIL) != 0u) {
        bus_write(card, HPCLANG_S3C2410_SDICMDSTAT, UINT32_C(0x0f00));
        return HPCLANG_SDIO_RESPONSE_ERROR;
    }

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
        if (response != HPCLANG_SDIO_RESPONSE_NONE)
            result->response[0] = bus_read(card, HPCLANG_S3C2410_SDIRSP0);
        if (response == HPCLANG_SDIO_RESPONSE_LONG) {
            result->response[1] = bus_read(card, HPCLANG_S3C2410_SDIRSP1);
            result->response[2] = bus_read(card, HPCLANG_S3C2410_SDIRSP2);
            result->response[3] = bus_read(card, HPCLANG_S3C2410_SDIRSP3);
        }
    }

    bus_write(card, HPCLANG_S3C2410_SDICMDSTAT, UINT32_C(0x0f00));
    return HPCLANG_SDIO_OK;
}

static uint32_t response_bits(
    const uint32_t response[4],
    unsigned msb,
    unsigned lsb)
{
    uint32_t value = 0u;

    for (unsigned bit = msb;; --bit) {
        unsigned word = (127u - bit) / 32u;
        value = (value << 1) | ((response[word] >> (bit & 31u)) & 1u);
        if (bit == lsb)
            break;
    }
    return value;
}

static int decode_sector_count(
    const uint32_t response[4],
    uint32_t *sector_count)
{
    uint32_t structure = response_bits(response, 127u, 126u);
    uint64_t sectors;

    if (structure == 0u) {
        uint32_t read_block_length = response_bits(response, 83u, 80u);
        uint32_t c_size = response_bits(response, 73u, 62u);
        uint32_t c_size_mult = response_bits(response, 49u, 47u);

        if (read_block_length > 11u || c_size_mult > 7u)
            return 0;

        uint64_t bytes =
            ((uint64_t)c_size + 1u) *
            (UINT64_C(1) << (c_size_mult + 2u)) *
            (UINT64_C(1) << read_block_length);
        if ((bytes & (SDIO_SECTOR_SIZE - 1u)) != 0u)
            return 0;
        sectors = bytes / SDIO_SECTOR_SIZE;
    } else if (structure == 1u) {
        uint32_t c_size = response_bits(response, 69u, 48u);
        sectors = ((uint64_t)c_size + 1u) * UINT64_C(1024);
    } else {
        return 0;
    }

    if (sectors == 0u || sectors > UINT32_MAX)
        return 0;

    *sector_count = (uint32_t)sectors;
    return 1;
}

static int wait_data_finish(struct hpclang_s3c2410_sdio_card *card)
{
    for (uint32_t polls = 0u; polls < SDIO_POLL_LIMIT; ++polls) {
        uint32_t status = bus_read(card, HPCLANG_S3C2410_SDIDSTA);
        if ((status & SDIDSTA_ERROR_MASK) != 0u)
            return HPCLANG_SDIO_DATA_ERROR;
        if ((status & HPCLANG_S3C2410_SDIDSTA_XFERFINISH) != 0u)
            return HPCLANG_SDIO_OK;
    }
    return HPCLANG_SDIO_TIMEOUT;
}

static void finish_data(struct hpclang_s3c2410_sdio_card *card)
{
    bus_write(card, HPCLANG_S3C2410_SDIDSTA, SDIDSTA_CLEAR_MASK);
    bus_write(card, HPCLANG_S3C2410_SDIDCON, 0u);
}

int hpclang_s3c2410_sdio_card_open(
    struct hpclang_s3c2410_sdio_card *card,
    const struct hpclang_s3c2410_sdio_bus *bus,
    uint32_t pclk_hz)
{
    if (card == NULL || bus == NULL || bus->read32 == NULL ||
        bus->write32 == NULL || pclk_hz == 0u)
        return HPCLANG_SDIO_IO_ERROR;

    memset(card, 0, sizeof(*card));
    card->bus = *bus;
    card->pclk_hz = pclk_hz;

    bus_write(card, HPCLANG_S3C2410_SDIPRE,
        hpclang_s3c2410_sdio_prescaler(pclk_hz, 400000u));
    bus_write(card, HPCLANG_S3C2410_SDITIMER, UINT32_C(0xffff));
    bus_write(card, HPCLANG_S3C2410_SDIBSIZE, SDIO_SECTOR_SIZE);
    bus_write(card, HPCLANG_S3C2410_SDIIMSK, 0u);
    bus_write(card, HPCLANG_S3C2410_SDICON,
        HPCLANG_S3C2410_SDICON_FIFORESET |
        HPCLANG_S3C2410_SDICON_CLOCKTYPE);

    /* Continuous clock supplies the >=74 startup clocks before CMD0. */
    for (uint32_t i = 0u; i < 128u; ++i)
        (void)bus_read(card, HPCLANG_S3C2410_SDICMDSTAT);

    int rc = send_command(
        card, 0u, 0u, HPCLANG_SDIO_RESPONSE_NONE, 0, 0, NULL);
    if (rc != HPCLANG_SDIO_OK)
        goto fail;

    struct command_result response;
    int version2 = 0;
    rc = send_command(
        card, 8u, UINT32_C(0x1aa), HPCLANG_SDIO_RESPONSE_SHORT,
        0, 1, &response);
    if (rc == HPCLANG_SDIO_OK) {
        if ((response.response[0] & UINT32_C(0xfff)) != UINT32_C(0x1aa)) {
            rc = HPCLANG_SDIO_PROTOCOL_ERROR;
            goto fail;
        }
        version2 = 1;
    } else if (rc != HPCLANG_SDIO_TIMEOUT) {
        goto fail;
    }

    uint32_t ocr = 0u;
    for (uint32_t attempt = 0u; attempt < SDIO_INIT_RETRIES; ++attempt) {
        rc = send_command(
            card, 55u, 0u, HPCLANG_SDIO_RESPONSE_SHORT, 0, 1, NULL);
        if (rc != HPCLANG_SDIO_OK)
            goto fail;

        uint32_t argument = UINT32_C(0x00ff8000);
        if (version2)
            argument |= UINT32_C(1) << 30;

        rc = send_command(
            card, 41u, argument, HPCLANG_SDIO_RESPONSE_SHORT,
            0, 0, &response);
        if (rc != HPCLANG_SDIO_OK)
            goto fail;

        ocr = response.response[0];
        if ((ocr & (UINT32_C(1) << 31)) != 0u)
            break;
    }
    if ((ocr & (UINT32_C(1) << 31)) == 0u) {
        rc = HPCLANG_SDIO_TIMEOUT;
        goto fail;
    }
    card->high_capacity =
        (version2 && (ocr & (UINT32_C(1) << 30)) != 0u) ? 1u : 0u;

    rc = send_command(
        card, 2u, 0u, HPCLANG_SDIO_RESPONSE_LONG, 0, 1, NULL);
    if (rc != HPCLANG_SDIO_OK)
        goto fail;

    rc = send_command(
        card, 3u, 0u, HPCLANG_SDIO_RESPONSE_SHORT, 0, 1, &response);
    if (rc != HPCLANG_SDIO_OK)
        goto fail;
    card->rca = response.response[0] >> 16;
    if (card->rca == 0u) {
        rc = HPCLANG_SDIO_PROTOCOL_ERROR;
        goto fail;
    }

    rc = send_command(
        card, 9u, card->rca << 16, HPCLANG_SDIO_RESPONSE_LONG,
        0, 1, &response);
    if (rc != HPCLANG_SDIO_OK)
        goto fail;
    if (!decode_sector_count(response.response, &card->sector_count)) {
        rc = HPCLANG_SDIO_PROTOCOL_ERROR;
        goto fail;
    }

    rc = send_command(
        card, 7u, card->rca << 16, HPCLANG_SDIO_RESPONSE_SHORT,
        0, 1, NULL);
    if (rc != HPCLANG_SDIO_OK)
        goto fail;

    if (!card->high_capacity) {
        rc = send_command(
            card, 16u, SDIO_SECTOR_SIZE, HPCLANG_SDIO_RESPONSE_SHORT,
            0, 1, NULL);
        if (rc != HPCLANG_SDIO_OK)
            goto fail;
    }

    /* S3C2410 is kept at <= PCLK/4 for reliable data transfers. */
    bus_write(card, HPCLANG_S3C2410_SDIPRE,
        hpclang_s3c2410_sdio_prescaler(pclk_hz, pclk_hz / 4u));
    card->initialized = 1u;
    return HPCLANG_SDIO_OK;

fail:
    hpclang_s3c2410_sdio_card_close(card);
    return rc;
}

void hpclang_s3c2410_sdio_card_close(
    struct hpclang_s3c2410_sdio_card *card)
{
    if (card == NULL || card->bus.write32 == NULL)
        return;

    bus_write(card, HPCLANG_S3C2410_SDIDCON, 0u);
    bus_write(card, HPCLANG_S3C2410_SDIIMSK, 0u);
    bus_write(card, HPCLANG_S3C2410_SDICON, 0u);
    card->initialized = 0u;
}

int hpclang_s3c2410_sdio_read_sector(
    struct hpclang_s3c2410_sdio_card *card,
    uint32_t sector,
    void *buffer)
{
    if (card == NULL || buffer == NULL || !card->initialized)
        return HPCLANG_SDIO_IO_ERROR;
    if (sector >= card->sector_count)
        return HPCLANG_SDIO_RANGE_ERROR;

    uint32_t argument;
    if (!hpclang_s3c2410_sdio_sector_argument(
            sector, card->high_capacity != 0u, &argument))
        return HPCLANG_SDIO_RANGE_ERROR;

    bus_write(card, HPCLANG_S3C2410_SDIDSTA, SDIDSTA_CLEAR_MASK);
    bus_write(card, HPCLANG_S3C2410_SDIBSIZE, SDIO_SECTOR_SIZE);
    bus_write(card, HPCLANG_S3C2410_SDIDCON,
        hpclang_s3c2410_sdio_data_control(HPCLANG_SDIO_DATA_READ, 1u));

    int rc = send_command(
        card, 17u, argument, HPCLANG_SDIO_RESPONSE_SHORT, 1, 1, NULL);
    if (rc != HPCLANG_SDIO_OK) {
        finish_data(card);
        return rc;
    }

    uint8_t *bytes = buffer;
    for (uint32_t offset = 0u; offset < SDIO_SECTOR_SIZE; offset += 4u) {
        uint32_t polls;
        for (polls = 0u; polls < SDIO_POLL_LIMIT; ++polls) {
            uint32_t status = bus_read(card, HPCLANG_S3C2410_SDIDSTA);
            if ((status & SDIDSTA_ERROR_MASK) != 0u) {
                finish_data(card);
                return HPCLANG_SDIO_DATA_ERROR;
            }
            if ((bus_read(card, HPCLANG_S3C2410_SDIFSTA) &
                 SDIFSTA_COUNTMASK) >= 4u)
                break;
        }
        if (polls == SDIO_POLL_LIMIT) {
            finish_data(card);
            return HPCLANG_SDIO_TIMEOUT;
        }

        uint32_t word = bus_read(card, HPCLANG_S3C2410_SDIDATA);
        bytes[offset] = (uint8_t)word;
        bytes[offset + 1u] = (uint8_t)(word >> 8);
        bytes[offset + 2u] = (uint8_t)(word >> 16);
        bytes[offset + 3u] = (uint8_t)(word >> 24);
    }

    rc = wait_data_finish(card);
    finish_data(card);
    return rc;
}

int hpclang_s3c2410_sdio_write_sector(
    struct hpclang_s3c2410_sdio_card *card,
    uint32_t sector,
    const void *buffer)
{
    if (card == NULL || buffer == NULL || !card->initialized)
        return HPCLANG_SDIO_IO_ERROR;
    if (sector >= card->sector_count)
        return HPCLANG_SDIO_RANGE_ERROR;

    uint32_t argument;
    if (!hpclang_s3c2410_sdio_sector_argument(
            sector, card->high_capacity != 0u, &argument))
        return HPCLANG_SDIO_RANGE_ERROR;

    bus_write(card, HPCLANG_S3C2410_SDIDSTA, SDIDSTA_CLEAR_MASK);
    bus_write(card, HPCLANG_S3C2410_SDIBSIZE, SDIO_SECTOR_SIZE);
    bus_write(card, HPCLANG_S3C2410_SDIDCON,
        hpclang_s3c2410_sdio_data_control(HPCLANG_SDIO_DATA_WRITE, 1u));

    int rc = send_command(
        card, 24u, argument, HPCLANG_SDIO_RESPONSE_SHORT, 1, 1, NULL);
    if (rc != HPCLANG_SDIO_OK) {
        finish_data(card);
        return rc;
    }

    const uint8_t *bytes = buffer;
    for (uint32_t offset = 0u; offset < SDIO_SECTOR_SIZE; offset += 4u) {
        uint32_t polls;
        for (polls = 0u; polls < SDIO_POLL_LIMIT; ++polls) {
            uint32_t status = bus_read(card, HPCLANG_S3C2410_SDIDSTA);
            if ((status & SDIDSTA_ERROR_MASK) != 0u) {
                finish_data(card);
                return HPCLANG_SDIO_DATA_ERROR;
            }
            uint32_t fifo = bus_read(card, HPCLANG_S3C2410_SDIFSTA);
            if ((fifo & (SDIFSTA_TFHALF | SDIFSTA_TFEMPTY)) != 0u)
                break;
        }
        if (polls == SDIO_POLL_LIMIT) {
            finish_data(card);
            return HPCLANG_SDIO_TIMEOUT;
        }

        uint32_t word =
            (uint32_t)bytes[offset] |
            ((uint32_t)bytes[offset + 1u] << 8) |
            ((uint32_t)bytes[offset + 2u] << 16) |
            ((uint32_t)bytes[offset + 3u] << 24);
        bus_write(card, HPCLANG_S3C2410_SDIDATA, word);
    }

    rc = wait_data_finish(card);
    finish_data(card);
    return rc;
}
