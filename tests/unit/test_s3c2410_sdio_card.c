#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "s3c2410-sdio-card.h"

#define CMDSTAT_RSPFIN (UINT32_C(1) << 9)
#define CMDSTAT_TIMEOUT (UINT32_C(1) << 10)
#define CMDSTAT_CMDSENT (UINT32_C(1) << 11)
#define CMD_WAITRSP (UINT32_C(1) << 9)
#define DATA_RXSTART (UINT32_C(2) << 12)
#define DATA_TXSTART (UINT32_C(3) << 12)
#define DATA_MODE_MASK (UINT32_C(3) << 12)
#define DATA_XFERFINISH (UINT32_C(1) << 4)
#define FIFO_TFEMPTY (UINT32_C(1) << 10)

struct fake_sdi
{
    uint32_t regs[17];
    uint8_t media[4][512];
    uint32_t read_offset;
    uint32_t write_offset;
    uint32_t last_data_arg;
    uint32_t response[4];
    uint32_t data_index;
    uint8_t high_capacity;
    uint8_t reading;
    uint8_t writing;
};

static unsigned reg_index(uint32_t offset)
{
    return offset / 4u;
}

static void fake_set_csd(struct fake_sdi *fake)
{
    if (!fake->high_capacity) {
        uint32_t c_size = 4095u;
        uint32_t read_bl_len = 9u;
        uint32_t c_size_mult = 7u;
        fake->response[0] = UINT32_C(0x000f005a);
        fake->response[1] =
            UINT32_C(0x0f508000) | ((c_size & UINT32_C(0xffc)) >> 2) | (read_bl_len << 16);
        fake->response[2] =
            UINT32_C(0x2dd47c1f) | ((c_size & UINT32_C(0x003)) << 30) | (c_size_mult << 15);
        fake->response[3] = UINT32_C(0x8a0040ff) | (read_bl_len << 22);
        return;
    }

    /* CSD v2, C_SIZE=3 -> 4096 sectors. */
    fake->response[0] = UINT32_C(1) << 30;
    fake->response[1] = 0u;
    fake->response[2] = UINT32_C(3) << 16;
    fake->response[3] = 0u;
}

static void fake_command(struct fake_sdi *fake, uint32_t control)
{
    uint32_t command = control & UINT32_C(0x3f);
    uint32_t argument = fake->regs[reg_index(HPCLANG_S3C2410_SDICMDARG)];
    fake->regs[reg_index(HPCLANG_S3C2410_SDICMDSTAT)] =
        (control & CMD_WAITRSP) != 0u ? CMDSTAT_RSPFIN : CMDSTAT_CMDSENT;
    memset(fake->response, 0, sizeof(fake->response));

    switch (command) {
    case 8u:
        if (fake->high_capacity) {
            fake->response[0] = UINT32_C(0x1aa);
        } else {
            fake->regs[reg_index(HPCLANG_S3C2410_SDICMDSTAT)] = CMDSTAT_TIMEOUT;
        }
        break;
    case 41u:
        fake->response[0] = UINT32_C(0x80ff8000);
        if (fake->high_capacity)
            fake->response[0] |= UINT32_C(1) << 30;
        break;
    case 3u:
        fake->response[0] = UINT32_C(0x12340600);
        break;
    case 9u:
        fake_set_csd(fake);
        break;
    case 17u:
        fake->last_data_arg = argument;
        fake->read_offset = fake->high_capacity ? argument * 512u : argument;
        fake->data_index = 0u;
        fake->reading = 1u;
        fake->writing = 0u;
        break;
    case 24u:
        fake->last_data_arg = argument;
        fake->write_offset = fake->high_capacity ? argument * 512u : argument;
        fake->data_index = 0u;
        fake->writing = 1u;
        fake->reading = 0u;
        break;
    default:
        break;
    }

    fake->regs[reg_index(HPCLANG_S3C2410_SDIRSP0)] = fake->response[0];
    fake->regs[reg_index(HPCLANG_S3C2410_SDIRSP1)] = fake->response[1];
    fake->regs[reg_index(HPCLANG_S3C2410_SDIRSP2)] = fake->response[2];
    fake->regs[reg_index(HPCLANG_S3C2410_SDIRSP3)] = fake->response[3];
}

static uint32_t fake_read32(void *context, uint32_t offset)
{
    struct fake_sdi *fake = context;

    if (offset == HPCLANG_S3C2410_SDIFSTA) {
        if (fake->reading && fake->data_index < 512u)
            return (512u - fake->data_index) > 127u ? 127u : 512u - fake->data_index;
        if (fake->writing)
            return FIFO_TFEMPTY;
        return 0u;
    }

    if (offset == HPCLANG_S3C2410_SDIDSTA) {
        uint32_t mode = fake->regs[reg_index(HPCLANG_S3C2410_SDIDCON)] & DATA_MODE_MASK;
        if ((fake->reading || fake->writing) && fake->data_index >= 512u)
            return DATA_XFERFINISH;
        if (fake->reading && mode == DATA_RXSTART)
            return 1u;
        if (fake->writing && mode == DATA_TXSTART)
            return 2u;
        return 0u;
    }

    if (offset == HPCLANG_S3C2410_SDIDATA) {
        uint32_t sector = fake->read_offset / 512u;
        uint32_t index = fake->data_index;
        if (!fake->reading || sector >= 4u || index + 4u > 512u)
            return 0u;
        uint8_t *p = &fake->media[sector][index];
        uint32_t word = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
                        ((uint32_t)p[3] << 24);
        fake->data_index += 4u;
        return word;
    }

    return fake->regs[reg_index(offset)];
}

static void fake_write32(void *context, uint32_t offset, uint32_t value)
{
    struct fake_sdi *fake = context;

    if (offset == HPCLANG_S3C2410_SDICMDSTAT) {
        fake->regs[reg_index(offset)] &= ~value;
        return;
    }
    if (offset == HPCLANG_S3C2410_SDIDSTA)
        return;
    if (offset == HPCLANG_S3C2410_SDICMDCON) {
        fake->regs[reg_index(offset)] = value;
        fake_command(fake, value);
        return;
    }
    if (offset == HPCLANG_S3C2410_SDIDATA) {
        uint32_t sector = fake->write_offset / 512u;
        uint32_t index = fake->data_index;
        if (!fake->writing || sector >= 4u || index + 4u > 512u)
            return;
        uint8_t *p = &fake->media[sector][index];
        p[0] = (uint8_t)value;
        p[1] = (uint8_t)(value >> 8);
        p[2] = (uint8_t)(value >> 16);
        p[3] = (uint8_t)(value >> 24);
        fake->data_index += 4u;
        return;
    }
    if (offset == HPCLANG_S3C2410_SDIDCON && value == 0u) {
        fake->reading = 0u;
        fake->writing = 0u;
    }
    fake->regs[reg_index(offset)] = value;
}

static int exercise_card(int high_capacity)
{
    struct fake_sdi fake;
    memset(&fake, 0, sizeof(fake));
    fake.high_capacity = high_capacity != 0;

    for (uint32_t i = 0u; i < 512u; ++i)
        fake.media[1][i] = (uint8_t)(i ^ UINT32_C(0x5a));

    struct hpclang_s3c2410_sdio_bus bus = {
        .read32 = fake_read32,
        .write32 = fake_write32,
        .context = &fake,
    };
    struct hpclang_s3c2410_sdio_card card;

    if (hpclang_s3c2410_sdio_card_open(&card, &bus, 66000000u) != HPCLANG_SDIO_OK)
        return 0;
    if (card.rca != UINT32_C(0x1234) || card.high_capacity != (uint8_t)(high_capacity != 0))
        return 0;
    if ((!high_capacity && card.sector_count != UINT32_C(2097152)) ||
        (high_capacity && card.sector_count != UINT32_C(4096)))
        return 0;

    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    if (hpclang_s3c2410_sdio_read_sector(&card, 1u, buffer) != HPCLANG_SDIO_OK)
        return 0;
    if (memcmp(buffer, fake.media[1], sizeof(buffer)) != 0)
        return 0;
    if (fake.last_data_arg != (high_capacity ? 1u : 512u))
        return 0;

    for (uint32_t i = 0u; i < 512u; ++i)
        buffer[i] = (uint8_t)(255u - i);
    if (hpclang_s3c2410_sdio_write_sector(&card, 2u, buffer) != HPCLANG_SDIO_OK)
        return 0;
    if (memcmp(buffer, fake.media[2], sizeof(buffer)) != 0)
        return 0;
    if (fake.last_data_arg != (high_capacity ? 2u : 1024u))
        return 0;

    if (hpclang_s3c2410_sdio_read_sector(&card, card.sector_count, buffer) !=
        HPCLANG_SDIO_RANGE_ERROR)
        return 0;

    hpclang_s3c2410_sdio_card_close(&card);
    return card.initialized == 0u;
}

int main(void)
{
    return exercise_card(0) && exercise_card(1) ? EXIT_SUCCESS : EXIT_FAILURE;
}
