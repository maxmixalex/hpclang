#include "hpclang-usbdev-host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int irq_level;
static void irq_cb(void *opaque, int level) { (void)opaque; irq_level = level; }

struct stream { uint8_t in[256], out[256]; size_t in_len,in_pos,out_len; };
static int sread(void *o,uint8_t *b,size_t n){struct stream*s=o;size_t r=s->in_len-s->in_pos;if(r>n)r=n;if(!r)return 0;memcpy(b,s->in+s->in_pos,r);s->in_pos+=r;return (int)r;}
static int swrite(void *o,const uint8_t*b,size_t n){struct stream*s=o;if(n>sizeof(s->out)-s->out_len)n=sizeof(s->out)-s->out_len;memcpy(s->out+s->out_len,b,n);s->out_len+=n;return (int)n;}

static const uint8_t dev_desc[18]={18,1,0x10,1,0xff,0,0,8,0xf0,0x03,0x21,0x01,0,1,1,2,0,1};
static const uint8_t cfg_desc[32]={9,2,32,0,1,1,0,0x80,50,9,4,0,0,2,0xff,0,0,0,7,5,0x81,2,64,0,2,7,5,0x03,2,64,0,2};

struct fake_guest { uint8_t response[64]; size_t len,pos; int active; };

static void fake_ep0_service(struct hpusb_core *u, struct fake_guest *g)
{
    hpusb_write(u,HPUSB_INDEX,0);
    if (hpusb_read(u,HPUSB_USB_INT)&HPUSB_USBINT_RESET) hpusb_write(u,HPUSB_USB_INT,HPUSB_USBINT_RESET);
    if ((hpusb_read(u,HPUSB_CSR1)&HPUSB_EP0_OPKRDY)!=0) {
        if (hpusb_read(u,HPUSB_OUT_CNT1)==0) {
            hpusb_write(u,HPUSB_CSR1,HPUSB_EP0_SOPKRDY|HPUSB_EP0_DE);
        } else {
        uint8_t q[8]; for(int i=0;i<8;i++)q[i]=(uint8_t)hpusb_read(u,HPUSB_FIFO0);
        hpusb_write(u,HPUSB_CSR1,HPUSB_EP0_SOPKRDY);
        uint16_t value=q[2]|((uint16_t)q[3]<<8), wlen=q[6]|((uint16_t)q[7]<<8);
        g->len=g->pos=0; g->active=0;
        if(q[1]==6 && (value>>8)==1){g->len=sizeof(dev_desc);memcpy(g->response,dev_desc,g->len);g->active=1;}
        else if(q[1]==6 && (value>>8)==2){g->len=wlen<sizeof(cfg_desc)?wlen:sizeof(cfg_desc);memcpy(g->response,cfg_desc,g->len);g->active=1;}
        else if(q[1]==5){hpusb_write(u,HPUSB_FUNC_ADDR,(value&0x7f)|0x80);hpusb_write(u,HPUSB_CSR1,HPUSB_EP0_DE);}
        else if(q[1]==9){hpusb_write(u,HPUSB_CSR1,HPUSB_EP0_DE);}
        else {hpusb_write(u,HPUSB_CSR1,HPUSB_EP0_SENDSTL);}
        }
    }
    if(g->active && !(hpusb_read(u,HPUSB_CSR1)&HPUSB_EP0_IPKRDY)){
        size_t left=g->len-g->pos, n=left>8?8:left;
        for(size_t i=0;i<n;i++)hpusb_write(u,HPUSB_FIFO0,g->response[g->pos+i]);
        g->pos+=n;
        hpusb_write(u,HPUSB_CSR1,HPUSB_EP0_IPKRDY|(g->pos==g->len?HPUSB_EP0_DE:0));
        if(g->pos==g->len)g->active=0;
    }
    if(hpusb_read(u,HPUSB_EP_INT)&1)hpusb_write(u,HPUSB_EP_INT,1);
}

int main(void)
{
    struct hpusb_core u; struct fake_guest g={0}; struct stream s={0};
    hpusb_init(&u,irq_cb,NULL); hpusb_set_stream(&u,sread,swrite,&s); hpusb_set_connected(&u,1);
    hpusb_write(&u,HPUSB_USB_INT_EN,HPUSB_USBINT_RESET); hpusb_write(&u,HPUSB_EP_INT_EN,0x0f);
    for(int i=0;i<1000 && !hpusb_is_streaming(&u);i++){hpusb_tick(&u);fake_ep0_service(&u,&g);}
    assert(hpusb_is_streaming(&u)); assert(u.bulk_in_ep==1); assert(u.bulk_out_ep==3); assert(u.configuration_value==1);
    /* guest -> host */
    hpusb_write(&u,HPUSB_INDEX,1); hpusb_write(&u,HPUSB_CSR2,HPUSB_IN_MODEIN);
    hpusb_write(&u,HPUSB_FIFO1,'O'); hpusb_write(&u,HPUSB_FIFO1,'K'); hpusb_write(&u,HPUSB_CSR1,HPUSB_IN_PKTRDY);
    hpusb_tick(&u); assert(s.out_len==2 && !memcmp(s.out,"OK",2)); assert(!(u.ep[1].in_csr1&HPUSB_IN_PKTRDY));
    /* host -> guest: explicit OUT_PKT_RDY retirement */
    memcpy(s.in,"HI",2);s.in_len=2;hpusb_tick(&u); assert(u.ep[3].out_csr1&HPUSB_OUT_PKTRDY);
    hpusb_write(&u,HPUSB_INDEX,3); assert(hpusb_read(&u,HPUSB_OUT_CNT1)==2); assert(hpusb_read(&u,HPUSB_FIFO3)=='H'); assert(hpusb_read(&u,HPUSB_FIFO3)=='I');
    hpusb_write(&u,HPUSB_OUT_CSR1,0); assert(!(u.ep[3].out_csr1&HPUSB_OUT_PKTRDY));
    /* Stock HP49/50 uses OUT_CSR2.AUTO_CLR on EP3.  Draining the FIFO must
     * retire OUT_PKT_RDY so the next host packet can be accepted. */
    hpusb_write(&u,HPUSB_OUT_CSR2,HPUSB_OUT_AUTOCLR);
    memcpy(s.in+s.in_len,"BY",2);s.in_len+=2;hpusb_tick(&u);
    assert(u.ep[3].out_csr1&HPUSB_OUT_PKTRDY);
    assert(hpusb_read(&u,HPUSB_OUT_CNT1)==2);
    assert(hpusb_read(&u,HPUSB_FIFO3)=='B');
    assert(hpusb_read(&u,HPUSB_FIFO3)=='Y');
    assert(!(u.ep[3].out_csr1&HPUSB_OUT_PKTRDY));
    /* Stock HP49/50 also uses IN_CSR2.AUTO_SET on EP1.  A full MAXP packet
     * becomes ready automatically; short packets retain explicit CSR1. */
    s.out_len=0;
    hpusb_write(&u,HPUSB_INDEX,1);
    hpusb_write(&u,HPUSB_MAXP_ALT,1); /* 8 bytes */
    hpusb_write(&u,HPUSB_CSR2,HPUSB_IN_MODEIN|HPUSB_IN_AUTOSET);
    for(int i=0;i<8;i++)hpusb_write(&u,HPUSB_FIFO1,(uint8_t)('0'+i));
    assert(u.ep[1].in_csr1&HPUSB_IN_PKTRDY);
    hpusb_tick(&u);
    assert(s.out_len==8 && !memcmp(s.out,"01234567",8));
    assert(!(u.ep[1].in_csr1&HPUSB_IN_PKTRDY));
    /* Device reset preserves the host bridge but requires fresh enumeration. */
    hpusb_reset_device(&u);
    assert(u.connected); assert(!hpusb_is_streaming(&u));
    hpusb_write(&u,HPUSB_USB_INT_EN,HPUSB_USBINT_RESET); hpusb_write(&u,HPUSB_EP_INT_EN,0x0f);
    memset(&g,0,sizeof(g));
    for(int i=0;i<1000 && !hpusb_is_streaming(&u);i++){hpusb_tick(&u);fake_ep0_service(&u,&g);}
    assert(hpusb_is_streaming(&u)); assert(u.bulk_in_ep==1); assert(u.bulk_out_ep==3);
    puts("PASS usbdev host core: enumerate + bulk IN/OUT + AUTOSET/AUTOCLR + reset/re-enumerate");
    return 0;
}