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

/*
 * Keep all temporary aliases in the five adjacent, preflighted L1 slots
 * 0x0a0..0x0a4.  One L1 section is 1 MiB, so adjacent aliases advance by
 * 0x00100000, not by 0x01000000.
 */
#define USBD_BASE           0x0a100000u
#define CLK_BASE            0x0a200000u
#define GPIO_BASE           0x0a300000u
#define INT_BASE            0x0a400000u
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

#define LOCKTIME            REG32(CLK_BASE + 0x00)
#define MPLLCON             REG32(CLK_BASE + 0x04)
#define UPLLCON             REG32(CLK_BASE + 0x08)
#define CLKCON              REG32(CLK_BASE + 0x0c)
#define CLKSLOW             REG32(CLK_BASE + 0x10)
#define CLKDIVN             REG32(CLK_BASE + 0x14)
#define GPFCON              REG32(GPIO_BASE + 0x50)
#define GPFDAT              REG32(GPIO_BASE + 0x54)
#define MISCCR              REG32(GPIO_BASE + 0x80)
#define EXTINT0             REG32(GPIO_BASE + 0x88)
#define SRCPND              REG32(INT_BASE + 0x00)
#define INTMSK              REG32(INT_BASE + 0x08)
#define INTPND              REG32(INT_BASE + 0x10)
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
#define IRQ_EINT1           (1u << 1)

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
    u32 locktime;
    u32 mpllcon;
    u32 upllcon;
    u32 clkcon;
    u32 clkslow;
    u32 clkdivn;
    u32 misccr;
    u32 intmsk;
    u32 gpfcon;
    u32 extint0;
    u32 lcd[17];
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

static int prepare_cable_detect(void)
{
    /*
     * Match newRPL target_39gs: GPF1/EINT1 with a falling-edge disconnect
     * latch.  The main interrupt controller remains masked while the streamer
     * owns the hardware, so SRCPND is safe to poll directly.
     */
    GPFCON = (GPFCON & ~0x0cu) | 0x08u;
    EXTINT0 = (EXTINT0 & ~0x70u) | 0x20u;
    SRCPND = IRQ_EINT1;
    INTPND = IRQ_EINT1;
    return cable_connected();
}

static int disconnect_seen(void)
{
    return !cable_connected() || (SRCPND & IRQ_EINT1);
}

static void take_snapshot(struct hw_snapshot *s)
{
    u32 i;

    s->locktime = LOCKTIME;
    s->mpllcon = MPLLCON;
    s->upllcon = UPLLCON;
    s->clkcon = CLKCON;
    s->clkslow = CLKSLOW;
    s->clkdivn = CLKDIVN;
    s->misccr = MISCCR;
    s->intmsk = INTMSK;
    s->gpfcon = GPFCON;
    s->extint0 = EXTINT0;

    /*
     * HPGCC sys_lcdsave() saves LCD registers 0..10 and 19..24.  Preserve
     * the exact same set so the raw PrRUN payload can perform the same restore
     * that a normal HPGCC crt0 program would get.
     */
    for(i = 0; i < 11; ++i)
        s->lcd[i] = REG32(LCD_BASE + i * 4u);
    for(i = 0; i < 6; ++i)
        s->lcd[11 + i] = REG32(LCD_BASE + 0x4cu + i * 4u);

    /* Raw PrRUN has no HPGCC crt0 around us; own the interrupt mask here. */
    INTMSK = 0xffffffffu;
}

static void usb_hwshutdown(void)
{
    /*
     * Follow newRPL usb_shutdown(): quiesce the UDC first, then remove its
     * clocks.  No attempt is made to replay volatile endpoint status bits.
     */
    USB_INT_EN_REG = 0;
    EP_INT_EN_REG = 0;
    USB_INT_REG = 0x07u;
    EP_INT_REG = 0x1fu;
    CLKSLOW |= 0x80u;   /* UPLL off */
    CLKCON &= ~0xc0u;   /* USB host + device clocks off */
}

static void restore_platform(const struct hw_snapshot *s)
{
    u32 i;
    u32 lcd0;

    /*
     * Do not leak our disconnect edge into the stock firmware.  Then restore
     * GPIO/pad state that HPGCC crt0 does not know about.
     */
    SRCPND = IRQ_EINT1;
    INTPND = IRQ_EINT1;
    EXTINT0 = s->extint0;
    GPFCON = s->gpfcon;
    MISCCR = s->misccr;

    /*
     * Exact shape of HPGCC 1.1 sys_lcdrestore(): strip live LINECNT/status
     * bits from LCDCON1, restore registers 0..10 and 19..24, then enable LCD.
     */
    lcd0 = s->lcd[0] & 0x0003fffeu;
    REG32(LCD_BASE + 0x00) = lcd0;
    for(i = 1; i < 11; ++i)
        REG32(LCD_BASE + i * 4u) = s->lcd[i];
    for(i = 0; i < 6; ++i)
        REG32(LCD_BASE + 0x4cu + i * 4u) = s->lcd[11 + i];
    REG32(LCD_BASE + 0x00) |= 1u;

    /*
     * Exact HPGCC 1.1 sys_clkrestore() ordering.  This is deliberately not a
     * field-by-field reverse of our setup: the historical runtime first puts
     * CLKSLOW in its safe intermediate state, restores clock registers 0..3,
     * then CLKDIVN, then the original CLKSLOW value.
     */
    CLKSLOW &= 0xffffffe0u;
    LOCKTIME = s->locktime;
    MPLLCON = s->mpllcon;
    UPLLCON = s->upllcon;
    CLKCON = s->clkcon;
    CLKDIVN = s->clkdivn;
    CLKSLOW = s->clkslow;

    /* Same final boundary as HPGCC sys_intOn(). */
    INTMSK = s->intmsk;
}

static u32 normal_fclk_scaled(void)
{
    /*
     * Return FCLK in units of 1 MHz times the PLL denominator:
     *   numerator = (m + 8) * 12
     *   denominator = (p + 2) * 2^s
     *
     * Keeping numerator/denominator separate avoids pulling __aeabi_uidiv into
     * this freestanding payload.
     */
    u32 pll = MPLLCON;
    u32 m = ((pll >> 12) & 0xffu) + 8u;
    return m * 12u;
}

static u32 normal_fclk_denominator(void)
{
    u32 pll = MPLLCON;
    u32 p = ((pll >> 4) & 0x1fu) + 2u;
    u32 s = pll & 3u;
    return p << s;
}

static int pclk_at_least_20mhz(void)
{
    u32 numerator;
    u32 denominator;
    u32 pclk_div = 1u;

    if(CLKSLOW & 0x10u)
        return 0;

    numerator = normal_fclk_scaled();
    denominator = normal_fclk_denominator();
    if(CLKDIVN & 2u)
        pclk_div <<= 1;
    if(CLKDIVN & 1u)
        pclk_div <<= 1;

    return numerator >= 20u * denominator * pclk_div;
}

static int prepare_usb_clock(void)
{
    u32 numerator;
    u32 denominator;
    u32 divider;

    /*
     * HPGCC39's CRT deliberately enters slow mode before main; its own source
     * notes a typical 3 MHz PCLK there.  The newRPL S3C2410 USB driver requires
     * PCLK > 20 MHz.  Use the same divider thresholds as newRPL cpu_setspeed(),
     * but keep the stock MPLL value rather than programming a new CPU PLL.
     *
     * Keep the LCD controller off while HCLK/PCLK changes.  We restore the
     * exact saved LCDCON1 only after the original clock state is back.
     */
    LCDCON1 &= ~1u;

    numerator = normal_fclk_scaled();
    denominator = normal_fclk_denominator();

    if(numerator <= 24u * denominator)
        divider = 0u;
    else if(numerator <= 48u * denominator)
        divider = 1u;
    else if(numerator <= 75u * denominator)
        divider = 2u;
    else
        divider = 3u;

    CLKDIVN = (CLKDIVN & ~3u) | divider;
    CLKSLOW &= ~0x10u;

    /*
     * If an unexpected stock MPLL/divider combination still leaves PCLK below
     * 20 MHz, try the least-divided safe setting.  If even FCLK itself is too
     * low, abort and restore instead of attempting USB at an invalid clock.
     */
    if(!pclk_at_least_20mhz()) {
        CLKDIVN &= ~3u;
        if(!pclk_at_least_20mhz())
            return 0;
    }
    return 1;
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
     * Raw ARMToolbox39 PrRUN does not provide HPGCC crt0 hardware save/restore,
     * so snapshot the complete state we own before changing cable detection.
     */
    take_snapshot(&saved);
    if(prepare_cable_detect()) {
        restore_platform(&saved);
        return 1;
    }

    if(!prepare_usb_clock()) {
        restore_platform(&saved);
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
    /*
     * Keep the physical disconnect boundary, but do not rely solely on
     * GPFDAT polling: newRPL configures EINT1 falling-edge detection for this
     * exact event.  The edge remains latched in SRCPND while INTMSK is masked.
     */
    while(!disconnect_seen()) { }
    usb_hwshutdown();
    restore_platform(&saved);
    return 0;
}
