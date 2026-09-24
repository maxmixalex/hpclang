#include "hpclang-usbdev-host.h"

#include <string.h>

static size_t ep_maxpacket(const struct hpusb_core *u, unsigned ep)
{
    unsigned encoded = u->ep[ep].maxp & 0x0fu;
    size_t bytes = encoded ? (size_t)encoded * 8u : (ep == 0 ? 8u : 64u);
    if (bytes > HPUSB_FIFO_CAP) bytes = HPUSB_FIFO_CAP;
    return bytes;
}

static void update_irq(struct hpusb_core *u)
{
    int level = ((u->usb_int & u->usb_int_en) != 0u) ||
                ((u->ep_int & u->ep_int_en) != 0u);
    if (level == u->irq_level) return;
    u->irq_level = level;
    if (u->irq) u->irq(u->irq_opaque, level);
}

static void ep_irq(struct hpusb_core *u, unsigned ep)
{
    if (ep < HPUSB_EP_COUNT) u->ep_int |= (uint8_t)(1u << ep);
    update_irq(u);
}

static void clear_ep_fifos(struct hpusb_ep *ep)
{
    ep->in_count = 0;
    ep->out_count = 0;
    ep->out_pos = 0;
}

void hpusb_init(struct hpusb_core *u, hpusb_irq_fn irq, void *irq_opaque)
{
    memset(u, 0, sizeof(*u));
    u->irq = irq;
    u->irq_opaque = irq_opaque;
    u->enum_state = HPUSB_ENUM_WAIT_READY;
    u->ep[0].maxp = 1; /* 8 bytes */
    for (unsigned i = 1; i < HPUSB_EP_COUNT; ++i) u->ep[i].maxp = 8; /* 64 */
}

void hpusb_set_stream(struct hpusb_core *u, hpusb_stream_read_fn read_fn,
                      hpusb_stream_write_fn write_fn, void *opaque)
{
    u->stream_read = read_fn;
    u->stream_write = write_fn;
    u->stream_opaque = opaque;
}

void hpusb_reset_device(struct hpusb_core *u)
{
    hpusb_irq_fn irq = u->irq;
    void *irq_opaque = u->irq_opaque;
    hpusb_stream_read_fn read_fn = u->stream_read;
    hpusb_stream_write_fn write_fn = u->stream_write;
    void *stream_opaque = u->stream_opaque;
    int connected = u->connected;

    if (u->irq_level && irq) irq(irq_opaque, 0);
    hpusb_init(u, irq, irq_opaque);
    hpusb_set_stream(u, read_fn, write_fn, stream_opaque);
    hpusb_set_connected(u, connected);
}

void hpusb_set_connected(struct hpusb_core *u, int connected)
{
    connected = connected != 0;
    if (connected == u->connected) return;
    u->connected = connected;
    u->enum_state = HPUSB_ENUM_WAIT_READY;
    u->enum_wait_ticks = 0;
    u->ctrl_active = 0;
    u->ctrl_status_out = 0;
    u->bridge_in_pos = 0;
    u->bulk_in_ep = 0;
    u->bulk_out_ep = 0;
    u->configuration_value = 0;
    if (!connected) {
        u->usb_int = 0;
        u->ep_int = 0;
        for (unsigned i = 0; i < HPUSB_EP_COUNT; ++i) {
            clear_ep_fifos(&u->ep[i]);
            u->ep[i].in_csr1 = 0;
            u->ep[i].out_csr1 = 0;
        }
        update_irq(u);
    }
}

static unsigned fifo_ep(uint32_t offset)
{
    if (offset < HPUSB_FIFO0 || offset > HPUSB_FIFO4) return HPUSB_EP_COUNT;
    if (((offset - HPUSB_FIFO0) & 3u) != 0u) return HPUSB_EP_COUNT;
    return (unsigned)((offset - HPUSB_FIFO0) >> 2);
}

uint32_t hpusb_read(struct hpusb_core *u, uint32_t offset)
{
    unsigned idx = u->index < HPUSB_EP_COUNT ? u->index : 0;
    struct hpusb_ep *ep = &u->ep[idx];
    unsigned fep = fifo_ep(offset);
    if (fep < HPUSB_EP_COUNT) {
        struct hpusb_ep *f = &u->ep[fep];
        uint8_t value;
        if (f->out_pos >= f->out_count) return 0;
        value = f->out_fifo[f->out_pos++];
        /*
         * S3C2410 OUT_CSR2.AUTO_CLR retires OUT_PKT_RDY after the MCU
         * drains the received packet from the OUT FIFO.  Stock HP49/50
         * enables this on EP3 and does not perform an explicit CSR1 clear.
         */
        if (f->out_pos >= f->out_count && (f->out_csr2 & HPUSB_OUT_AUTOCLR)) {
            f->out_count = 0;
            f->out_pos = 0;
            f->out_csr1 &= (uint8_t)~HPUSB_OUT_PKTRDY;
        }
        return value;
    }

    switch (offset) {
    case HPUSB_FUNC_ADDR: return u->func_addr;
    case HPUSB_PWR: return u->pwr;
    case HPUSB_EP_INT: return u->ep_int;
    case HPUSB_USB_INT: return u->usb_int;
    case HPUSB_EP_INT_EN: return u->ep_int_en;
    case HPUSB_USB_INT_EN: return u->usb_int_en;
    case HPUSB_FRAME1: return (uint8_t)(u->frame & 0xffu);
    case HPUSB_FRAME2: return (uint8_t)(u->frame >> 8);
    case HPUSB_INDEX: return u->index;
    case HPUSB_MAXP0:
    case HPUSB_MAXP_ALT: return ep->maxp;
    case HPUSB_CSR1: return idx == 0 ? ep->in_csr1 : ep->in_csr1;
    case HPUSB_CSR2: return ep->in_csr2;
    case HPUSB_OUT_CSR1: return ep->out_csr1;
    case HPUSB_OUT_CSR2: return ep->out_csr2;
    case HPUSB_OUT_CNT1: return (uint8_t)((ep->out_count - ep->out_pos) & 0xffu);
    case HPUSB_OUT_CNT2: return (uint8_t)(((ep->out_count - ep->out_pos) >> 8) & 0xffu);
    default: return 0;
    }
}

static void write_ep0_csr(struct hpusb_core *u, uint8_t data)
{
    struct hpusb_ep *ep = &u->ep[0];
    if (data & HPUSB_EP0_SOPKRDY) {
        ep->in_csr1 &= (uint8_t)~HPUSB_EP0_OPKRDY;
        ep->out_count = ep->out_pos = 0;
    }
    if (data & HPUSB_EP0_IPKRDY) ep->in_csr1 |= HPUSB_EP0_IPKRDY;
    if (data & HPUSB_EP0_SENDSTL) ep->in_csr1 |= HPUSB_EP0_SENDSTL;
    if (data & HPUSB_EP0_SSE) ep->in_csr1 &= (uint8_t)~HPUSB_EP0_SE;
    if ((data & HPUSB_EP0_SENTSTL) == 0) ep->in_csr1 &= (uint8_t)~HPUSB_EP0_SENTSTL;
    if (data & HPUSB_EP0_DE) u->ctrl_data_end = 1;
}

static void write_in_csr1(struct hpusb_ep *ep, uint8_t data)
{
    if (data & HPUSB_IN_FFLUSH) ep->in_count = 0;
    if (data & HPUSB_IN_PKTRDY) ep->in_csr1 |= HPUSB_IN_PKTRDY;
    if (data & HPUSB_IN_SENDSTL) ep->in_csr1 |= HPUSB_IN_SENDSTL;
    else ep->in_csr1 &= (uint8_t)~HPUSB_IN_SENDSTL;
    if ((data & HPUSB_IN_SENTSTL) == 0) ep->in_csr1 &= (uint8_t)~HPUSB_IN_SENTSTL;
    if ((data & HPUSB_IN_UNDRUN) == 0) ep->in_csr1 &= (uint8_t)~HPUSB_IN_UNDRUN;
}

static void write_out_csr1(struct hpusb_ep *ep, uint8_t data)
{
    if (data & HPUSB_OUT_FFLUSH) {
        ep->out_count = ep->out_pos = 0;
        ep->out_csr1 &= (uint8_t)~HPUSB_OUT_PKTRDY;
    }
    if ((data & HPUSB_OUT_PKTRDY) == 0) {
        ep->out_count = ep->out_pos = 0;
        ep->out_csr1 &= (uint8_t)~HPUSB_OUT_PKTRDY;
    }
    if (data & HPUSB_OUT_SENDSTL) ep->out_csr1 |= HPUSB_OUT_SENDSTL;
    else ep->out_csr1 &= (uint8_t)~HPUSB_OUT_SENDSTL;
    if ((data & HPUSB_OUT_SENTSTL) == 0) ep->out_csr1 &= (uint8_t)~HPUSB_OUT_SENTSTL;
    if ((data & HPUSB_OUT_OVRRUN) == 0) ep->out_csr1 &= (uint8_t)~HPUSB_OUT_OVRRUN;
}

void hpusb_write(struct hpusb_core *u, uint32_t offset, uint32_t data32)
{
    uint8_t data = (uint8_t)data32;
    unsigned idx = u->index < HPUSB_EP_COUNT ? u->index : 0;
    struct hpusb_ep *ep = &u->ep[idx];
    unsigned fep = fifo_ep(offset);
    if (fep < HPUSB_EP_COUNT) {
        struct hpusb_ep *f = &u->ep[fep];
        if (fep != 0u && (f->in_csr1 & HPUSB_IN_PKTRDY)) return;
        if (f->in_count < HPUSB_FIFO_CAP) f->in_fifo[f->in_count++] = data;
        /*
         * With IN_CSR2.AUTO_SET the controller publishes a full MAXP
         * packet without requiring firmware to set IN_PKT_RDY explicitly.
         * Short packets still require the ordinary CSR1 write.
         */
        if (fep != 0u &&
            (f->in_csr2 & HPUSB_IN_AUTOSET) &&
            f->in_count >= ep_maxpacket(u, fep))
            f->in_csr1 |= HPUSB_IN_PKTRDY;
        return;
    }

    switch (offset) {
    case HPUSB_FUNC_ADDR: u->func_addr = data; break;
    case HPUSB_PWR:
        u->pwr = (uint8_t)((u->pwr & 0x08u) | (data & 0x87u));
        break;
    case HPUSB_EP_INT:
        u->ep_int &= (uint8_t)~data;
        update_irq(u);
        break;
    case HPUSB_USB_INT:
        u->usb_int &= (uint8_t)~data;
        if ((data & HPUSB_USBINT_RESET) != 0u) u->pwr &= (uint8_t)~0x08u;
        update_irq(u);
        break;
    case HPUSB_EP_INT_EN:
        u->ep_int_en = data & 0x1fu;
        update_irq(u);
        break;
    case HPUSB_USB_INT_EN:
        u->usb_int_en = data & 0x07u;
        update_irq(u);
        break;
    case HPUSB_INDEX:
        u->index = data & 0x07u;
        break;
    case HPUSB_MAXP0:
    case HPUSB_MAXP_ALT:
        ep->maxp = data & 0x0fu;
        break;
    case HPUSB_CSR1:
        if (idx == 0) write_ep0_csr(u, data);
        else write_in_csr1(ep, data);
        break;
    case HPUSB_CSR2:
        ep->in_csr2 = data;
        break;
    case HPUSB_OUT_CSR1:
        write_out_csr1(ep, data);
        break;
    case HPUSB_OUT_CSR2:
        ep->out_csr2 = data;
        break;
    default:
        break;
    }
}

static void bus_reset(struct hpusb_core *u)
{
    u->func_addr = 0;
    u->pwr |= 0x08u;
    u->usb_int |= HPUSB_USBINT_RESET;
    u->ep_int = 0;
    for (unsigned i = 0; i < HPUSB_EP_COUNT; ++i) {
        clear_ep_fifos(&u->ep[i]);
        u->ep[i].in_csr1 = 0;
        u->ep[i].out_csr1 = 0;
    }
    update_irq(u);
}

static void start_control(struct hpusb_core *u, uint8_t bm, uint8_t req,
                          uint16_t value, uint16_t index, uint16_t length)
{
    struct hpusb_ep *ep = &u->ep[0];
    uint8_t setup[8] = {
        bm, req,
        (uint8_t)value, (uint8_t)(value >> 8),
        (uint8_t)index, (uint8_t)(index >> 8),
        (uint8_t)length, (uint8_t)(length >> 8),
    };
    clear_ep_fifos(ep);
    memcpy(ep->out_fifo, setup, sizeof(setup));
    ep->out_count = sizeof(setup);
    ep->in_csr1 &= (uint8_t)~(HPUSB_EP0_IPKRDY | HPUSB_EP0_SENDSTL | HPUSB_EP0_SENTSTL);
    ep->in_csr1 |= HPUSB_EP0_OPKRDY;
    u->ctrl_active = 1;
    u->ctrl_dir_in = (bm & 0x80u) != 0u;
    u->ctrl_data_end = 0;
    u->ctrl_status_out = 0;
    u->ctrl_wlength = length;
    u->ctrl_response_len = 0;
    u->ctrl_last_packet = 0;
    ep_irq(u, 0);
}

/* 1 complete, 0 waiting, -1 stalled/invalid. */
static int progress_control(struct hpusb_core *u)
{
    struct hpusb_ep *ep = &u->ep[0];
    if (!u->ctrl_active) return 1;
    if (ep->in_csr1 & HPUSB_EP0_SENDSTL) {
        u->ctrl_active = 0;
        return -1;
    }

    if (u->ctrl_dir_in) {
        if (u->ctrl_status_out) {
            if (!(ep->in_csr1 & HPUSB_EP0_OPKRDY) || u->ctrl_data_end) {
                ep->in_csr1 &= (uint8_t)~HPUSB_EP0_OPKRDY;
                ep->out_count = ep->out_pos = 0;
                u->ctrl_active = 0;
                u->ctrl_status_out = 0;
                u->ctrl_data_end = 0;
                return 1;
            }
            return 0;
        }
        if (ep->in_csr1 & HPUSB_EP0_IPKRDY) {
            size_t room = HPUSB_CTRL_CAP - u->ctrl_response_len;
            size_t take = ep->in_count < room ? ep->in_count : room;
            if (take) memcpy(u->ctrl_response + u->ctrl_response_len, ep->in_fifo, take);
            u->ctrl_response_len += take;
            u->ctrl_last_packet = ep->in_count;
            ep->in_count = 0;
            ep->in_csr1 &= (uint8_t)~HPUSB_EP0_IPKRDY;
            ep_irq(u, 0); /* host ACK; device may queue the next packet */
            if (u->ctrl_data_end ||
                u->ctrl_last_packet < ep_maxpacket(u, 0)) {
                u->ctrl_active = 0;
                u->ctrl_data_end = 0;
                return 1;
            }
            if (u->ctrl_response_len >= u->ctrl_wlength) {
                /* Exact-multiple control reads still require the host's
                 * zero-length OUT status stage. Old HP39g+ firmware relies
                 * on that stage rather than setting DATA_END on the final
                 * full EP0 packet. */
                ep->out_count = ep->out_pos = 0;
                ep->in_csr1 |= HPUSB_EP0_OPKRDY;
                u->ctrl_status_out = 1;
                ep_irq(u, 0);
            }
        }
    } else if (!(ep->in_csr1 & HPUSB_EP0_OPKRDY) && u->ctrl_data_end) {
        u->ctrl_active = 0;
        u->ctrl_data_end = 0;
        return 1;
    }
    return 0;
}

static int parse_config(struct hpusb_core *u)
{
    size_t p = 0;
    u->bulk_in_ep = u->bulk_out_ep = 0;
    u->configuration_value = 1;
    while (p + 2 <= u->ctrl_response_len) {
        uint8_t len = u->ctrl_response[p];
        uint8_t type = u->ctrl_response[p + 1];
        if (len < 2 || p + len > u->ctrl_response_len) break;
        if (type == 2 && len >= 6) u->configuration_value = u->ctrl_response[p + 5];
        if (type == 5 && len >= 7 && (u->ctrl_response[p + 3] & 3u) == 2u) {
            uint8_t addr = u->ctrl_response[p + 2];
            uint8_t ep = addr & 0x0fu;
            if (ep < HPUSB_EP_COUNT) {
                if (addr & 0x80u) { if (!u->bulk_in_ep) u->bulk_in_ep = ep; }
                else { if (!u->bulk_out_ep) u->bulk_out_ep = ep; }
            }
        }
        p += len;
    }
    return u->bulk_in_ep != 0 && u->bulk_out_ep != 0;
}

static void enum_tick(struct hpusb_core *u)
{
    int r;
    switch (u->enum_state) {
    case HPUSB_ENUM_WAIT_READY:
        if ((u->usb_int_en & HPUSB_USBINT_RESET) && (u->ep_int_en & 1u)) {
            bus_reset(u);
            u->enum_state = HPUSB_ENUM_WAIT_RESET_ACK;
            u->enum_wait_ticks = 0;
        }
        break;
    case HPUSB_ENUM_WAIT_RESET_ACK:
        if ((u->usb_int & HPUSB_USBINT_RESET) == 0u) {
            if (++u->enum_wait_ticks >= 2u) {
                start_control(u, 0x80, 6, 0x0100, 0, 18);
                u->enum_state = HPUSB_ENUM_GET_DEVICE;
            }
        }
        break;
    case HPUSB_ENUM_GET_DEVICE:
        r = progress_control(u);
        if (r < 0) u->enum_state = HPUSB_ENUM_FAILED;
        else if (r > 0 && u->ctrl_response_len >= 8) {
            start_control(u, 0x00, 5, 1, 0, 0);
            u->enum_state = HPUSB_ENUM_SET_ADDRESS;
        }
        break;
    case HPUSB_ENUM_SET_ADDRESS:
        r = progress_control(u);
        if (r < 0) u->enum_state = HPUSB_ENUM_FAILED;
        else if (r > 0) {
            start_control(u, 0x80, 6, 0x0200, 0, 9);
            u->enum_state = HPUSB_ENUM_GET_CONFIG_HEAD;
        }
        break;
    case HPUSB_ENUM_GET_CONFIG_HEAD:
        r = progress_control(u);
        if (r < 0) u->enum_state = HPUSB_ENUM_FAILED;
        else if (r > 0 && u->ctrl_response_len >= 9) {
            u->config_total_length = (uint16_t)u->ctrl_response[2] |
                                     ((uint16_t)u->ctrl_response[3] << 8);
            if (u->config_total_length < 9 || u->config_total_length > HPUSB_CTRL_CAP) {
                u->enum_state = HPUSB_ENUM_FAILED;
            } else {
                start_control(u, 0x80, 6, 0x0200, 0, u->config_total_length);
                u->enum_state = HPUSB_ENUM_GET_CONFIG_FULL;
            }
        }
        break;
    case HPUSB_ENUM_GET_CONFIG_FULL:
        r = progress_control(u);
        if (r < 0) u->enum_state = HPUSB_ENUM_FAILED;
        else if (r > 0) {
            if (!parse_config(u)) u->enum_state = HPUSB_ENUM_FAILED;
            else {
                start_control(u, 0x00, 9, u->configuration_value, 0, 0);
                u->enum_state = HPUSB_ENUM_SET_CONFIG;
            }
        }
        break;
    case HPUSB_ENUM_SET_CONFIG:
        r = progress_control(u);
        if (r < 0) u->enum_state = HPUSB_ENUM_FAILED;
        else if (r > 0) u->enum_state = HPUSB_ENUM_STREAM;
        break;
    case HPUSB_ENUM_STREAM:
    case HPUSB_ENUM_FAILED:
        break;
    }
}

static void stream_tick(struct hpusb_core *u)
{
    struct hpusb_ep *in;
    struct hpusb_ep *out;
    if (!u->stream_read || !u->stream_write ||
        !u->bulk_in_ep || !u->bulk_out_ep) return;
    in = &u->ep[u->bulk_in_ep];
    out = &u->ep[u->bulk_out_ep];

    if (in->in_csr1 & HPUSB_IN_PKTRDY) {
        int n;
        if (u->bridge_in_pos > in->in_count) u->bridge_in_pos = 0;
        n = u->stream_write(u->stream_opaque,
                            in->in_fifo + u->bridge_in_pos,
                            in->in_count - u->bridge_in_pos);
        if (n > 0) u->bridge_in_pos += (size_t)n;
        if (u->bridge_in_pos >= in->in_count) {
            in->in_count = 0;
            u->bridge_in_pos = 0;
            in->in_csr1 &= (uint8_t)~HPUSB_IN_PKTRDY;
            ep_irq(u, u->bulk_in_ep);
        }
    }

    if (!(out->out_csr1 & HPUSB_OUT_PKTRDY)) {
        uint8_t buffer[HPUSB_FIFO_CAP];
        size_t cap = ep_maxpacket(u, u->bulk_out_ep);
        int n = u->stream_read(u->stream_opaque, buffer, cap);
        if (n > 0) {
            if ((size_t)n > cap) n = (int)cap;
            memcpy(out->out_fifo, buffer, (size_t)n);
            out->out_count = (size_t)n;
            out->out_pos = 0;
            out->out_csr1 |= HPUSB_OUT_PKTRDY;
            ep_irq(u, u->bulk_out_ep);
        }
    }
}

void hpusb_sof(struct hpusb_core *u)
{
    if (!u->connected) return;
    u->frame = (uint16_t)((u->frame + 1u) & 0x07ffu);
}

void hpusb_tick(struct hpusb_core *u)
{
    if (!u->connected) return;
    if (u->enum_state != HPUSB_ENUM_STREAM) enum_tick(u);
    else stream_tick(u);
}

int hpusb_is_streaming(const struct hpusb_core *u)
{
    return u->enum_state == HPUSB_ENUM_STREAM;
}