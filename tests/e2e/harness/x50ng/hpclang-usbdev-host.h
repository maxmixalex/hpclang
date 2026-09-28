#ifndef HPCLANG_USBDEV_HOST_H
#define HPCLANG_USBDEV_HOST_H

#include <stddef.h>
#include <stdint.h>

#define HPUSB_EP_COUNT 5u
#define HPUSB_FIFO_CAP 128u
#define HPUSB_CTRL_CAP 256u

#define HPUSB_FUNC_ADDR 0x0140u
#define HPUSB_PWR 0x0144u
#define HPUSB_EP_INT 0x0148u
#define HPUSB_USB_INT 0x0158u
#define HPUSB_EP_INT_EN 0x015cu
#define HPUSB_USB_INT_EN 0x016cu
#define HPUSB_FRAME1 0x0170u
#define HPUSB_FRAME2 0x0174u
#define HPUSB_INDEX 0x0178u
#define HPUSB_MAXP0 0x0180u
#define HPUSB_CSR1 0x0184u
#define HPUSB_CSR2 0x0188u
#define HPUSB_MAXP_ALT 0x018cu
#define HPUSB_OUT_CSR1 0x0190u
#define HPUSB_OUT_CSR2 0x0194u
#define HPUSB_OUT_CNT1 0x0198u
#define HPUSB_OUT_CNT2 0x019cu
#define HPUSB_FIFO0 0x01c0u
#define HPUSB_FIFO1 0x01c4u
#define HPUSB_FIFO2 0x01c8u
#define HPUSB_FIFO3 0x01ccu
#define HPUSB_FIFO4 0x01d0u

#define HPUSB_USBINT_RESET 0x04u
#define HPUSB_FUNC_ADDR_UPDATE 0x80u
#define HPUSB_EP0_OPKRDY 0x01u
#define HPUSB_EP0_IPKRDY 0x02u
#define HPUSB_EP0_SENTSTL 0x04u
#define HPUSB_EP0_DE 0x08u
#define HPUSB_EP0_SE 0x10u
#define HPUSB_EP0_SENDSTL 0x20u
#define HPUSB_EP0_SOPKRDY 0x40u
#define HPUSB_EP0_SSE 0x80u
#define HPUSB_IN_PKTRDY 0x01u
#define HPUSB_IN_SENTSTL 0x20u
#define HPUSB_IN_SENDSTL 0x10u
#define HPUSB_IN_FFLUSH 0x08u
#define HPUSB_IN_UNDRUN 0x04u
#define HPUSB_IN_AUTOSET 0x80u
#define HPUSB_IN_CLRDT 0x40u
#define HPUSB_IN_MODEIN 0x20u
#define HPUSB_OUT_AUTOCLR 0x80u
#define HPUSB_OUT_PKTRDY 0x01u
#define HPUSB_OUT_SENTSTL 0x40u
#define HPUSB_OUT_SENDSTL 0x20u
#define HPUSB_OUT_FFLUSH 0x10u
#define HPUSB_OUT_OVRRUN 0x04u
#define HPUSB_OUT_CLRDT 0x80u

typedef void (*hpusb_irq_fn)(void *opaque, int level);
typedef int (*hpusb_stream_read_fn)(void *opaque, uint8_t *buffer, size_t cap);
typedef int (*hpusb_stream_write_fn)(void *opaque, const uint8_t *buffer, size_t len);

struct hpusb_ep
{
    uint8_t maxp;
    uint8_t in_csr1;
    uint8_t in_csr2;
    uint8_t out_csr1;
    uint8_t out_csr2;
    uint8_t in_fifo[HPUSB_FIFO_CAP];
    size_t in_count;
    uint8_t out_fifo[HPUSB_FIFO_CAP];
    size_t out_count;
    size_t out_pos;
};

enum hpusb_enum_state
{
    HPUSB_ENUM_WAIT_READY = 0,
    HPUSB_ENUM_WAIT_RESET_ACK,
    HPUSB_ENUM_GET_DEVICE,
    HPUSB_ENUM_SET_ADDRESS,
    HPUSB_ENUM_GET_CONFIG_HEAD,
    HPUSB_ENUM_GET_CONFIG_FULL,
    HPUSB_ENUM_SET_CONFIG,
    HPUSB_ENUM_VERIFY_CONFIG,
    HPUSB_ENUM_STREAM,
    HPUSB_ENUM_FAILED,
};

struct hpusb_core
{
    uint8_t func_addr;
    uint8_t pwr;
    uint8_t ep_int;
    uint8_t usb_int;
    uint8_t ep_int_en;
    uint8_t usb_int_en;
    uint16_t frame;
    uint8_t index;
    struct hpusb_ep ep[HPUSB_EP_COUNT];

    int connected;
    int irq_level;
    hpusb_irq_fn irq;
    void *irq_opaque;

    hpusb_stream_read_fn stream_read;
    hpusb_stream_write_fn stream_write;
    void *stream_opaque;

    enum hpusb_enum_state enum_state;
    unsigned enum_wait_ticks;
    int ctrl_active;
    int ctrl_dir_in;
    int ctrl_data_end;
    int ctrl_status_out;
    uint16_t ctrl_wlength;
    uint8_t ctrl_response[HPUSB_CTRL_CAP];
    size_t ctrl_response_len;
    size_t ctrl_last_packet;
    uint16_t config_total_length;
    uint8_t bulk_in_ep;
    uint8_t bulk_out_ep;
    uint8_t configuration_value;
    size_t bridge_in_pos;
};

void hpusb_init(struct hpusb_core *u, hpusb_irq_fn irq, void *irq_opaque);
void hpusb_set_stream(struct hpusb_core *u, hpusb_stream_read_fn read_fn,
                      hpusb_stream_write_fn write_fn, void *opaque);
void hpusb_set_connected(struct hpusb_core *u, int connected);
void hpusb_reset_device(struct hpusb_core *u);
uint32_t hpusb_read(struct hpusb_core *u, uint32_t offset);
void hpusb_write(struct hpusb_core *u, uint32_t offset, uint32_t data);
void hpusb_sof(struct hpusb_core *u);
void hpusb_tick(struct hpusb_core *u);
int hpusb_is_streaming(const struct hpusb_core *u);

#endif