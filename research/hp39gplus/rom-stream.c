/*
 * Experimental HP39g+ physical-ROM streamer.
 *
 * Transport: polled S3C2410 USB device controller, Raw HID compatible with
 * the controller setup used by newRPL's target_39gs/target_50g driver.
 * Flash access is read-only.  No NOR erase/program command exists here.
 *
 * Safety contract:
 *   - Caller maps aliases before entry and removes them after return.
 *   - Start with the USB cable unplugged.  If it is already connected, return.
 *   - All SoC IRQ sources are masked while this polling payload owns clocks
 *     and the USB controller; the exact prior mask is restored afterwards.
 *   - After host FINISH, wait for cable removal before restoring hardware.
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define REG32(a) (*(volatile u32 *)(a))

#define FLASH_BASE          0x0a000000u
#define FLASH_BYTES         0x00100000u
#define BLOCK_BYTES         0x00004000u

#define USBD_BASE           0x0b000000u
#define CLK_BASE            0x0c000000u
#define GPIO_BASE           0x0d000000u
#define INT_BASE            0x0e000000u
/* HPGCC39 exposes the stock LCD controller at this established ARM VA. */
#define LCD_BASE            0x07300000u

#define FUNC_ADDR_REG       REG32(USBD_BASE + 0x140)
#define PWR_REG             REG32(USBD_BASE + 0x144)
#define EP_INT_REG          REG32(USBD_BASE + 0x148)
#define USB_INT_REG         REG32(USBD_BASE + 0x158)
#define EP_INT_EN_REG       REG32(USBD_BASE + 0x15c)
#define USB_INT_EN_REG      REG32(USBD_BASE + 0x16c)
#define INDEX_REG           REG32(USBD_BASE + 0x178)
#define MAXP_REG            REG32(USBD_BASE + 0x180)
#define EP0_CSR             REG32(USBD_BASE + 0x184)
#define IN_CSR1_REG         REG32(USBD_BASE + 0x184)
#define IN_CSR2_REG         REG32(USBD_BASE + 0x188)
#define MAXP_REG2           REG32(USBD_BASE + 0x18c)
#define OUT_CSR1_REG        REG32(USBD_BASE + 0x190)
#define OUT_CSR2_REG        REG32(USBD_BASE + 0x194)
#define OUT_FIFO_CNT1_REG   REG32(USBD_BASE + 0x198)
#define OUT_FIFO_CNT2_REG   REG32(USBD_BASE + 0x19c)
#define EP0_FIFO            REG32(USBD_BASE + 0x1c0)
#define EP1_FIFO            REG32(USBD_BASE + 0x1c4)
#define EP2_FIFO            REG32(USBD_BASE + 0x1c8)

#define MPLLCON             REG32(CLK_BASE + 0x04)
#define UPLLCON             REG32(CLK_BASE + 0x08)
#define CLKCON              REG32(CLK_BASE + 0x0c)
#define CLKSLOW             REG32(CLK_BASE + 0x10)
#define CLKDIVN             REG32(CLK_BASE + 0x14)
#define GPFCON              REG32(GPIO_BASE + 0x50)
#define GPFDAT              REG32(GPIO_BASE + 0x54)
#define MISCCR              REG32(GPIO_BASE + 0x80)
#define INTMSK              REG32(INT_BASE + 0x08)
#define LCDCON1             REG32(LCD_BASE + 0x00)

#define EP0_OUT_PKT_RDY             0x01u
#define EP0_IN_PKT_RDY              0x02u
#define EP0_SENT_STALL              0x04u
#define EP0_DATA_END                0x08u
#define EP0_SETUP_END               0x10u
#define EP0_SEND_STALL              0x20u
#define EP0_SERVICED_OUT_PKT_RDY    0x40u
#define EP0_SERVICED_SETUP_END      0x80u
#define EPN_IN_PKT_RDY              0x01u
#define EPN_IN_FIFO_FLUSH           0x08u
#define EPN_IN_CLR_DATA_TOGGLE      0x40u
#define EPN_OUT_PKT_RDY             0x01u
#define EPN_OUT_FIFO_FLUSH          0x10u
#define EPN_OUT_CLR_DATA_TOGGLE     0x80u

#define USB_RESET_BIT       0x04u

#define GET_STATUS          0u
#define CLEAR_FEATURE       1u
#define SET_FEATURE         3u
#define SET_ADDRESS         5u
#define GET_DESCRIPTOR      6u
#define GET_CONFIGURATION   8u
#define SET_CONFIGURATION   9u
#define GET_INTERFACE       10u
#define SET_INTERFACE       11u
#define HID_GET_REPORT      1u
#define HID_SET_REPORT      9u
#define HID_SET_IDLE        10u
#define HID_SET_PROTOCOL    11u

#define VID_HP              0x03f0u
#define PID_HP39            0x0121u
#define PROTO_VERSION       1u
#define REPORT_BYTES        64u
#define DATA_BYTES          56u
#define FNV_OFFSET          2166136261u
#define FNV_PRIME           16777619u

struct hw_snapshot {
    u32 clkcon;
    u32 upllcon;
    u32 clkslow;
    u32 clkdivn;
    u32 lcdcon1;
    u32 misccr;
    u32 intmsk;
    u32 gpfcon;
    u32 func_addr;
    u32 pwr;
    u32 ep_int_en;
    u32 usb_int_en;
    u32 index;
    u32 ep0_maxp;
    u32 ep0_maxp2;
    u32 ep0_csr;
    u32 ep1_maxp;
    u32 ep1_maxp2;
    u32 ep1_in1;
    u32 ep1_in2;
    u32 ep1_out1;
    u32 ep1_out2;
    u32 ep2_maxp;
    u32 ep2_maxp2;
    u32 ep2_in1;
    u32 ep2_in2;
    u32 ep2_out1;
    u32 ep2_out2;
};

static void put16(u8 *p, u16 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

static void put32(u8 *p, u32 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static u32 get32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void zero64(u8 *p)
{
    u32 i;
    for(i = 0; i < REPORT_BYTES; ++i)
        p[i] = 0;
}

static int cable_connected(void)
{
    return (GPFDAT & 2u) != 0;
}

static int prepare_cable_detect(struct hw_snapshot *s)
{
    /*
     * GPF1 does not necessarily enter the HPGCC aplet as EINT1.  newRPL's
     * target_39gs USB driver explicitly selects EINT1 before reading GPFDAT.
     * Without this mux step a stock HP39g+ can keep reporting the GPIO latch
     * value instead of the physical cable state and wait forever.
     */
    s->gpfcon = GPFCON;
    GPFCON = (s->gpfcon & ~0x0cu) | 0x08u;
    return cable_connected();
}

static void snapshot_ep(struct hw_snapshot *s, u32 ep)
{
    INDEX_REG = ep;
    if(ep == 0) {
        s->ep0_maxp = MAXP_REG;
        s->ep0_maxp2 = MAXP_REG2;
        s->ep0_csr = EP0_CSR;
    }
    else if(ep == 1) {
        s->ep1_maxp = MAXP_REG;
        s->ep1_maxp2 = MAXP_REG2;
        s->ep1_in1 = IN_CSR1_REG;
        s->ep1_in2 = IN_CSR2_REG;
        s->ep1_out1 = OUT_CSR1_REG;
        s->ep1_out2 = OUT_CSR2_REG;
    }
    else {
        s->ep2_maxp = MAXP_REG;
        s->ep2_maxp2 = MAXP_REG2;
        s->ep2_in1 = IN_CSR1_REG;
        s->ep2_in2 = IN_CSR2_REG;
        s->ep2_out1 = OUT_CSR1_REG;
        s->ep2_out2 = OUT_CSR2_REG;
    }
}

static void take_snapshot(struct hw_snapshot *s)
{
    s->clkcon = CLKCON;
    s->upllcon = UPLLCON;
    s->clkslow = CLKSLOW;
    s->clkdivn = CLKDIVN;
    s->lcdcon1 = LCDCON1;
    s->misccr = MISCCR;
    s->intmsk = INTMSK;

    /*
     * This payload is entirely polled.  Keep every stock IRQ handler out while
     * clocks and USB state are temporarily different from the OS's view.
     */
    INTMSK = 0xffffffffu;

    /* Temporarily clock the device block so indexed register state is readable. */
    CLKCON = (s->clkcon & ~0x40u) | 0x80u;
    s->func_addr = FUNC_ADDR_REG;
    s->pwr = PWR_REG;
    s->ep_int_en = EP_INT_EN_REG;
    s->usb_int_en = USB_INT_EN_REG;
    s->index = INDEX_REG;
    snapshot_ep(s, 0);
    snapshot_ep(s, 1);
    snapshot_ep(s, 2);
    INDEX_REG = s->index;
    CLKCON = s->clkcon;
}

static void restore_hw(const struct hw_snapshot *s)
{
    /* Restore indexed UDC configuration while its PCLK gate is open. */
    CLKCON = (s->clkcon & ~0x40u) | 0x80u;
    EP_INT_EN_REG = 0;
    USB_INT_EN_REG = 0;
    EP_INT_REG = 0x1fu;
    USB_INT_REG = 0x07u;

    INDEX_REG = 0;
    MAXP_REG = s->ep0_maxp;
    MAXP_REG2 = s->ep0_maxp2;
    EP0_CSR = 0xc0u;

    INDEX_REG = 1;
    MAXP_REG = s->ep1_maxp;
    MAXP_REG2 = s->ep1_maxp2;
    IN_CSR1_REG = EPN_IN_FIFO_FLUSH | EPN_IN_CLR_DATA_TOGGLE;
    IN_CSR2_REG = s->ep1_in2;
    OUT_CSR1_REG = EPN_OUT_FIFO_FLUSH | EPN_OUT_CLR_DATA_TOGGLE;
    OUT_CSR2_REG = s->ep1_out2;

    INDEX_REG = 2;
    MAXP_REG = s->ep2_maxp;
    MAXP_REG2 = s->ep2_maxp2;
    IN_CSR1_REG = EPN_IN_FIFO_FLUSH | EPN_IN_CLR_DATA_TOGGLE;
    IN_CSR2_REG = s->ep2_in2;
    OUT_CSR1_REG = EPN_OUT_FIFO_FLUSH | EPN_OUT_CLR_DATA_TOGGLE;
    OUT_CSR2_REG = s->ep2_out2;

    FUNC_ADDR_REG = s->func_addr;
    PWR_REG = s->pwr;
    EP_INT_EN_REG = s->ep_int_en;
    USB_INT_EN_REG = s->usb_int_en;
    INDEX_REG = s->index;

    GPFCON = s->gpfcon;
    MISCCR = s->misccr;
    UPLLCON = s->upllcon;
    CLKDIVN = s->clkdivn;
    CLKSLOW = s->clkslow;
    CLKCON = s->clkcon;
    LCDCON1 = s->lcdcon1;
    INTMSK = s->intmsk;
}

static u32 current_pclk(void)
{
    u32 fclk = 12000000u;
    u32 slow = CLKSLOW;
    u32 div = CLKDIVN;
    if(slow & 0x10u) {
        u32 slow_div = slow & 7u;
        if(slow_div)
            fclk /= slow_div << 1;
    }
    else {
        u32 pll = MPLLCON;
        u32 m = ((pll >> 12) & 0xffu) + 8u;
        u32 p = ((pll >> 4) & 0x1fu) + 2u;
        u32 s = pll & 3u;
        fclk = (m * 12000000u) / (p << s);
    }
    if(div & 2u)
        fclk >>= 1;
    if(div & 1u)
        fclk >>= 1;
    return fclk;
}

static int prepare_usb_clock(void)
{
    /*
     * HPGCC39's CRT deliberately enters slow mode before main; its own source
     * notes a typical 3 MHz PCLK there.  The newRPL S3C2410 USB driver requires
     * PCLK > 20 MHz.  Reuse the stock normal-mode MPLL, but choose the same
     * 75 MHz divider policy as newRPL (HCLK=FCLK/2, PCLK=HCLK).
     *
     * Keep the LCD controller off while HCLK changes.  We restore the exact
     * saved LCDCON1 only after the original clock state is back.
     */
    LCDCON1 &= ~1u;
    if(current_pclk() < 20000000u) {
        CLKDIVN = (CLKDIVN & ~3u) | 2u;
        CLKSLOW &= ~0x10u;
    }
    return current_pclk() >= 20000000u;
}

static void usb_hwsetup(void)
{
    CLKCON &= ~0x40u;   /* USB host off */
    CLKCON |= 0x80u;    /* USB device on */
    UPLLCON = 0x78023u; /* 48 MHz USB PLL, same value as newRPL target_39gs */
    CLKSLOW &= ~0x80u;  /* UPLL on */
    MISCCR &= ~((1u << 13) | (1u << 12) | (1u << 3));

    PWR_REG = 0;
    FUNC_ADDR_REG = 0x80u;

    INDEX_REG = 0;
    MAXP_REG = 1;       /* 8-byte EP0 */
    MAXP_REG2 = 1;
    EP0_CSR = 0xc0u;

    INDEX_REG = 1;
    MAXP_REG = 8;       /* 64-byte EP1 IN */
    MAXP_REG2 = 8;
    IN_CSR1_REG = EPN_IN_FIFO_FLUSH | EPN_IN_CLR_DATA_TOGGLE;
    IN_CSR2_REG = 0x20u;
    OUT_CSR1_REG = EPN_OUT_CLR_DATA_TOGGLE;
    OUT_CSR2_REG = 0;

    INDEX_REG = 2;
    MAXP_REG = 8;       /* 64-byte EP2 OUT */
    MAXP_REG2 = 8;
    IN_CSR1_REG = EPN_IN_FIFO_FLUSH | EPN_IN_CLR_DATA_TOGGLE;
    IN_CSR2_REG = 0;
    OUT_CSR1_REG = EPN_OUT_CLR_DATA_TOGGLE;
    OUT_CSR2_REG = 0;

    USB_INT_EN_REG = 0x07u;
    EP_INT_EN_REG = 0x07u;
    USB_INT_REG = 0x07u;
    EP_INT_REG = 0x1fu;
    INDEX_REG = 0;
}

static u32 make_device_desc(u8 *d)
{
    d[0]=18; d[1]=1; d[2]=0x10; d[3]=0x01;
    d[4]=0; d[5]=0; d[6]=0; d[7]=8;
    put16(d+8, VID_HP); put16(d+10, PID_HP39);
    d[12]=0x01; d[13]=0x01;
    d[14]=1; d[15]=2; d[16]=0; d[17]=1;
    return 18;
}

static u32 make_hid_report_desc(u8 *d)
{
    u32 i=0;
    d[i++]=0x06; d[i++]=0xab; d[i++]=0xff;
    d[i++]=0x0a; d[i++]=0x00; d[i++]=0x02;
    d[i++]=0xa1; d[i++]=0x01;
    d[i++]=0x75; d[i++]=0x08;
    d[i++]=0x15; d[i++]=0x00;
    d[i++]=0x26; d[i++]=0xff; d[i++]=0x00;
    d[i++]=0x95; d[i++]=64;
    d[i++]=0x09; d[i++]=0x01;
    d[i++]=0x81; d[i++]=0x02;
    d[i++]=0x95; d[i++]=64;
    d[i++]=0x09; d[i++]=0x02;
    d[i++]=0x91; d[i++]=0x02;
    d[i++]=0xc0;
    return i;
}

static u32 make_config_desc(u8 *d)
{
    u32 i=0;
    d[i++]=9; d[i++]=2; d[i++]=41; d[i++]=0; d[i++]=1; d[i++]=1; d[i++]=0; d[i++]=0xc0; d[i++]=50;
    d[i++]=9; d[i++]=4; d[i++]=0; d[i++]=0; d[i++]=2; d[i++]=3; d[i++]=0; d[i++]=0; d[i++]=0;
    d[i++]=9; d[i++]=0x21; d[i++]=0x11; d[i++]=0x01; d[i++]=0; d[i++]=1; d[i++]=0x22; d[i++]=28; d[i++]=0;
    d[i++]=7; d[i++]=5; d[i++]=0x81; d[i++]=3; d[i++]=64; d[i++]=0; d[i++]=2;
    d[i++]=7; d[i++]=5; d[i++]=0x02; d[i++]=3; d[i++]=64; d[i++]=0; d[i++]=2;
    return i;
}

static void put_utf16_char(u8 *d, u32 index, u8 ch)
{
    d[2 + 2 * index] = ch;
    d[3 + 2 * index] = 0;
}

static u32 make_string_desc(u8 *d, u32 which)
{
    if(which == 0) {
        d[0]=4; d[1]=3; d[2]=0x09; d[3]=0x04;
        return 4;
    }
    d[1]=3;
    if(which == 1) {
        d[0]=16;
        put_utf16_char(d,0,'h'); put_utf16_char(d,1,'p');
        put_utf16_char(d,2,'c'); put_utf16_char(d,3,'l');
        put_utf16_char(d,4,'a'); put_utf16_char(d,5,'n');
        put_utf16_char(d,6,'g');
        return 16;
    }
    if(which == 2) {
        d[0]=36;
        put_utf16_char(d,0,'H'); put_utf16_char(d,1,'P');
        put_utf16_char(d,2,'3'); put_utf16_char(d,3,'9');
        put_utf16_char(d,4,'g'); put_utf16_char(d,5,'+');
        put_utf16_char(d,6,' '); put_utf16_char(d,7,'R');
        put_utf16_char(d,8,'O'); put_utf16_char(d,9,'M');
        put_utf16_char(d,10,' '); put_utf16_char(d,11,'S');
        put_utf16_char(d,12,'t'); put_utf16_char(d,13,'r');
        put_utf16_char(d,14,'e'); put_utf16_char(d,15,'a');
        put_utf16_char(d,16,'m');
        return 36;
    }
    return 0;
}

static void ep0_ack(void)
{
    INDEX_REG=0;
    EP0_CSR |= EP0_SERVICED_OUT_PKT_RDY | EP0_DATA_END;
}

static void ep0_stall(void)
{
    INDEX_REG=0;
    EP0_CSR |= EP0_SEND_STALL | EP0_SERVICED_OUT_PKT_RDY;
}

static void ep0_send(const u8 *data, u32 count, u32 requested)
{
    u32 sent=0, n, i;
    if(count > requested) count=requested;
    INDEX_REG=0;
    EP0_CSR |= EP0_SERVICED_OUT_PKT_RDY;
    while(sent < count) {
        n=count-sent;
        if(n>8) n=8;
        while(EP0_CSR & EP0_IN_PKT_RDY) { }
        for(i=0;i<n;++i) EP0_FIFO=data[sent+i];
        sent += n;
        if(sent == count && !(count < requested && (count & 7u)==0))
            EP0_CSR |= EP0_IN_PKT_RDY | EP0_DATA_END;
        else
            EP0_CSR |= EP0_IN_PKT_RDY;
    }
    if(count==0 || (count < requested && (count & 7u)==0)) {
        while(EP0_CSR & EP0_IN_PKT_RDY) { }
        EP0_CSR |= EP0_IN_PKT_RDY | EP0_DATA_END;
    }
}

static void reset_endpoints(void)
{
    int was_masked = 0;
    (void)was_masked;
    FUNC_ADDR_REG=0x80u;
    INDEX_REG=0; MAXP_REG=1; MAXP_REG2=1; EP0_CSR=0xc0u;
    INDEX_REG=1; MAXP_REG=8; MAXP_REG2=8; IN_CSR1_REG=0x48u; IN_CSR2_REG=0x20u; OUT_CSR1_REG=0x80u; OUT_CSR2_REG=0;
    INDEX_REG=2; MAXP_REG=8; MAXP_REG2=8; IN_CSR1_REG=0x48u; IN_CSR2_REG=0; OUT_CSR1_REG=0x80u; OUT_CSR2_REG=0;
    INDEX_REG=0;
}

static int service_ep0(int *configured)
{
    u8 b[64];
    u32 reqtype, request, value, index, length, n;
    INDEX_REG=0;
    if(EP0_CSR & EP0_SETUP_END)
        EP0_CSR |= EP0_SERVICED_SETUP_END;
    if(EP0_CSR & EP0_SENT_STALL)
        EP0_CSR = 0;
    if(!(EP0_CSR & EP0_OUT_PKT_RDY))
        return 0;

    reqtype=(u8)EP0_FIFO; request=(u8)EP0_FIFO;
    value=(u8)EP0_FIFO; value|=(u32)(u8)EP0_FIFO<<8;
    index=(u8)EP0_FIFO; index|=(u32)(u8)EP0_FIFO<<8;
    length=(u8)EP0_FIFO; length|=(u32)(u8)EP0_FIFO<<8;

    if((reqtype & 0x60u)==0) {
        if(request==GET_DESCRIPTOR) {
            n=0;
            if(value==0x0100u) n=make_device_desc(b);
            else if(value==0x0200u) n=make_config_desc(b);
            else if(value==0x2200u && index==0) n=make_hid_report_desc(b);
            else if(value==0x2100u && index==0) { make_config_desc(b); for(n=0;n<9;++n) b[n]=b[18+n]; }
            else if((value & 0xff00u)==0x0300u) n=make_string_desc(b,value & 0xffu);
            if(n) { ep0_send(b,n,length); return 1; }
            ep0_stall(); return 1;
        }
        if(request==SET_ADDRESS) {
            FUNC_ADDR_REG=(value & 0x7fu) | 0x80u;
            ep0_ack(); return 1;
        }
        if(request==SET_CONFIGURATION) {
            *configured = value ? 1 : 0;
            ep0_ack(); return 1;
        }
        if(request==GET_CONFIGURATION) {
            b[0]=(u8)(*configured ? 1 : 0); ep0_send(b,1,length); return 1;
        }
        if(request==GET_STATUS) {
            b[0]=0; b[1]=0; ep0_send(b,2,length); return 1;
        }
        if(request==GET_INTERFACE) {
            b[0]=0; ep0_send(b,1,length); return 1;
        }
        if(request==SET_INTERFACE || request==CLEAR_FEATURE || request==SET_FEATURE) {
            ep0_ack(); return 1;
        }
        ep0_stall(); return 1;
    }

    if((reqtype & 0x61u)==0x21u && index==0) {
        if(request==HID_SET_IDLE || request==HID_SET_PROTOCOL) {
            ep0_ack(); return 1;
        }
        if(request==HID_GET_REPORT) {
            zero64(b); ep0_send(b,64,length); return 1;
        }
        if(request==HID_SET_REPORT) {
            /* Raw HID data uses EP2 OUT.  Do not ACK a control transfer whose
               data stage we are not consuming. */
            ep0_stall(); return 1;
        }
    }
    ep0_stall();
    return 1;
}

static void pump_usb(int *configured)
{
    u32 u=USB_INT_REG;
    u32 e=EP_INT_REG;
    if(u & USB_RESET_BIT) {
        *configured=0;
        reset_endpoints();
        USB_INT_REG=USB_RESET_BIT;
        return;
    }
    if(u & 0x03u)
        USB_INT_REG=u & 0x03u;
    if(e & 1u) {
        service_ep0(configured);
        EP_INT_REG=1u;
    }
    if(e & 6u)
        EP_INT_REG=e & 6u;
}

static int recv_report(u8 *b, int *configured)
{
    u32 n,i;
    for(;;) {
        pump_usb(configured);
        if(!cable_connected()) return -1;
        if(!*configured) continue;
        INDEX_REG=2;
        if(OUT_CSR1_REG & EPN_OUT_PKT_RDY) {
            n=(OUT_FIFO_CNT1_REG & 0xffu) | ((OUT_FIFO_CNT2_REG & 0xffu)<<8);
            for(i=0;i<REPORT_BYTES;++i)
                b[i]=(i<n)?(u8)EP2_FIFO:0;
            while(i<n) { (void)EP2_FIFO; ++i; }
            OUT_CSR1_REG &= ~EPN_OUT_PKT_RDY;
            INDEX_REG=0;
            return (int)n;
        }
        INDEX_REG=0;
    }
}

static int send_report(const u8 *b, int *configured)
{
    u32 i;
    for(;;) {
        pump_usb(configured);
        if(!cable_connected() || !*configured) return -1;
        INDEX_REG=1;
        if(!(IN_CSR1_REG & EPN_IN_PKT_RDY)) break;
        INDEX_REG=0;
    }
    for(i=0;i<REPORT_BYTES;++i) EP1_FIFO=b[i];
    IN_CSR1_REG |= EPN_IN_PKT_RDY;
    INDEX_REG=0;
    return 0;
}

static int command_is(const u8 *b, u8 cmd)
{
    return b[0]==cmd && b[1]==PROTO_VERSION && b[2]=='H' && b[3]=='3' && b[4]=='9';
}

static int wait_command(u8 wanted, u8 *b, int *configured)
{
    for(;;) {
        if(recv_report(b,configured)<0) return -1;
        if(command_is(b,'X')) return -2;
        if(command_is(b,wanted)) return 0;
        if(command_is(b,'?')) {
            zero64(b);
            b[0]='I'; b[1]=PROTO_VERSION; b[2]='H'; b[3]='3'; b[4]='9'; b[5]='R'; b[6]='O'; b[7]='M';
            put32(b+8,FLASH_BYTES); put32(b+12,BLOCK_BYTES); put32(b+16,DATA_BYTES);
            if(send_report(b,configured)<0) return -1;
        }
    }
}

static int send_block(u32 block, u8 *pkt, int *configured, u32 *sum_out, u32 *fnv_out)
{
    u32 base=block*BLOCK_BYTES;
    u32 pos=0, seq=0, n, i, sum=0, fnv=FNV_OFFSET;
    volatile const u8 *flash=(volatile const u8 *)(FLASH_BASE+base);
    while(pos<BLOCK_BYTES) {
        n=BLOCK_BYTES-pos; if(n>DATA_BYTES)n=DATA_BYTES;
        zero64(pkt);
        pkt[0]='D'; pkt[1]=(u8)n; put16(pkt+2,(u16)seq); put32(pkt+4,base+pos);
        for(i=0;i<n;++i) {
            u8 v=flash[pos+i];
            pkt[8+i]=v; sum+=v; fnv=(fnv ^ v)*FNV_PRIME;
        }
        if(send_report(pkt,configured)<0) return -1;
        pos+=n; ++seq;
    }
    *sum_out=sum; *fnv_out=fnv;
    return 0;
}

int hp39_rom_stream_main(void)
{
    struct hw_snapshot saved;
    u8 pkt[64];
    int configured=0;
    u32 block, sum, fnv, total_sum=0;

    /*
     * Physical connect/disconnect is our safe ownership boundary.  Configure
     * GPF1 exactly as the target_39gs USB driver does before trusting GPFDAT.
     */
    if(prepare_cable_detect(&saved)) {
        GPFCON = saved.gpfcon;
        return 1;
    }

    take_snapshot(&saved);
    if(!prepare_usb_clock()) {
        restore_hw(&saved);
        return 2;
    }
    usb_hwsetup();

    /* Wait for physical insertion and USB enumeration. */
    while(!cable_connected()) { }
    while(!configured) {
        pump_usb(&configured);
        if(!cable_connected())
            goto out_wait_disconnect;
    }

    /* Host may probe info repeatedly; START begins the one-shot stream. */
    if(wait_command('S',pkt,&configured)<0)
        goto out_wait_disconnect;

    for(block=0;block<FLASH_BYTES/BLOCK_BYTES;++block) {
retry_block:
        if(send_block(block,pkt,&configured,&sum,&fnv)<0)
            goto out_wait_disconnect;

        zero64(pkt);
        pkt[0]='C'; pkt[1]=PROTO_VERSION; pkt[2]=(u8)block;
        put32(pkt+4,block*BLOCK_BYTES); put32(pkt+8,BLOCK_BYTES);
        put32(pkt+12,sum); put32(pkt+16,fnv);
        if(send_report(pkt,&configured)<0)
            goto out_wait_disconnect;

        for(;;) {
            if(recv_report(pkt,&configured)<0)
                goto out_wait_disconnect;
            if(command_is(pkt,'X'))
                goto out_wait_disconnect;
            if(command_is(pkt,'R') && pkt[5]==(u8)block)
                goto retry_block;
            if(command_is(pkt,'A') && pkt[5]==(u8)block && get32(pkt+8)==block*BLOCK_BYTES)
                break;
            if(command_is(pkt,'?')) {
                zero64(pkt); pkt[0]='I'; pkt[1]=PROTO_VERSION; pkt[2]='H'; pkt[3]='3'; pkt[4]='9'; pkt[5]='R'; pkt[6]='O'; pkt[7]='M';
                put32(pkt+8,FLASH_BYTES); put32(pkt+12,BLOCK_BYTES); put32(pkt+16,DATA_BYTES);
                if(send_report(pkt,&configured)<0) goto out_wait_disconnect;
            }
        }
        total_sum += sum;
    }

    zero64(pkt);
    pkt[0]='E'; pkt[1]=PROTO_VERSION; pkt[2]='H'; pkt[3]='3'; pkt[4]='9'; pkt[5]='R'; pkt[6]='O'; pkt[7]='M';
    put32(pkt+8,FLASH_BYTES); put32(pkt+12,total_sum); put32(pkt+16,0x50415353u);
    if(send_report(pkt,&configured)<0)
        goto out_wait_disconnect;

    /* FINISH is explicit so the host has persisted and hashed the full image. */
    if(wait_command('F',pkt,&configured)<0)
        goto out_wait_disconnect;

out_wait_disconnect:
    /* Never hand a live, reconfigured USB controller back to the stock OS. */
    while(cable_connected()) { }
    restore_hw(&saved);
    return 0;
}
