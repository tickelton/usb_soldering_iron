/*
 * Authors: Osamu Tamura, tickelton@gmail.com
 * Licenses: AVR-CDC/CDC-IO: Proprietary, free under certain conditions. See License_CDC-IO.txt.
 *           usbdrv: GNU GPL version 2 or the GNU GPL version 3.
 *           everything else: GNU GPL version 2. See License.txt.
 * Copyright: (c) 2007 by Recursion Co., Ltd.
 *            (c) 2021 tickelton@gmail.com
 */

#include "cdc.h"


static const PROGMEM char configDescrCDC[] = {   /* USB configuration descriptor */
    9,          /* sizeof(usbDescrConfig): length of descriptor in bytes */
    USBDESCR_CONFIG,    /* descriptor type */
    67,
    0,          /* total length of data returned (including inlined descriptors) */
    2,          /* number of interfaces in this configuration */
    1,          /* index of this configuration */
    0,          /* configuration name string index */
    USBATTR_BUSPOWER,   /* attributes */
    USB_CFG_MAX_BUS_POWER/2,            /* max USB current in 2mA units */

    /* interface descriptor follows inline: */
    9,          /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE, /* descriptor type */
    0,          /* index of this interface */
    0,          /* alternate setting for this interface */
    USB_CFG_HAVE_INTRIN_ENDPOINT,   /* endpoints excl 0: number of endpoint descriptors to follow */
    USB_CFG_INTERFACE_CLASS,
    USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL,
    0,          /* string index for interface */

    /* CDC Class-Specific descriptor */
    5,           /* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    0,           /* header functional descriptor */
    0x10, 0x01,

    4,           /* sizeof(usbDescrCDC_AcmFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    2,           /* abstract control management functional descriptor */
    0x02,        /* SET_LINE_CODING,    GET_LINE_CODING, SET_CONTROL_LINE_STATE    */

    5,           /* sizeof(usbDescrCDC_UnionFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    6,           /* union functional descriptor */
    0,           /* CDC_COMM_INTF_ID */
    1,           /* CDC_DATA_INTF_ID */

    5,           /* sizeof(usbDescrCDC_CallMgtFn): length of descriptor in bytes */
    0x24,        /* descriptor type */
    1,           /* call management functional descriptor */
    3,           /* allow management on data interface, handles call management by itself */
    1,           /* CDC_DATA_INTF_ID */

    /* Endpoint Descriptor */
    7,           /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x80|USB_CFG_EP3_NUMBER,        /* IN endpoint number */
    0x03,        /* attrib: Interrupt endpoint */
    8, 0,        /* maximum packet size */
    USB_CFG_INTR_POLL_INTERVAL,        /* in ms */

    /* Interface Descriptor  */
    9,           /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE,           /* descriptor type */
    1,           /* index of this interface */
    0,           /* alternate setting for this interface */
    2,           /* endpoints excl 0: number of endpoint descriptors to follow */
    0x0A,        /* Data Interface Class Codes */
    0,
    0,           /* Data Interface Class Protocol Codes */
    0,           /* string index for interface */

    /* Endpoint Descriptor */
    7,           /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x01,        /* OUT endpoint number 1 */
    0x02,        /* attrib: Bulk endpoint */
    8, 0,        /* maximum packet size */
    0,           /* in ms */

    /* Endpoint Descriptor */
    7,           /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x81,        /* IN endpoint number 1 */
    0x02,        /* attrib: Bulk endpoint */
    8, 0,        /* maximum packet size */
    0,           /* in ms */
};


uchar usbFunctionDescriptor(usbRequest_t *rq)
{

    if(rq->wValue.bytes[1] == USBDESCR_DEVICE){
        usbMsgPtr = (uchar *)usbDescriptorDevice;
        return usbDescriptorDevice[0];
    }else{  /* must be config descriptor */
        usbMsgPtr = (uchar *)configDescrCDC;
        return sizeof(configDescrCDC);
    }
}


/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

uchar usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */

        if( rq->bRequest==GET_LINE_CODING || rq->bRequest==SET_LINE_CODING ){
            return 0xff;
        /*    GET_LINE_CODING -> usbFunctionRead()    */
        /*    SET_LINE_CODING -> usbFunctionWrite()    */
        }
        if(rq->bRequest == SET_CONTROL_LINE_STATE){
            /* Report serial state (carrier detect). On several Unix platforms,
             * tty devices can only be opened when carrier detect is set.
             */
            if( intr3Status==0 )
                intr3Status = 2;
        }
        /*  Prepare bulk-in endpoint to respond to early termination   */
        if((rq->bmRequestType & USBRQ_DIR_MASK) == USBRQ_DIR_HOST_TO_DEVICE)
            sendEmptyFrame  = 1;
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/* usbFunctionRead                                                           */
/*---------------------------------------------------------------------------*/

uchar usbFunctionRead( uchar *data, uchar len )
{
    memcpy( data, modeBuffer, 7 );
    return 7;
}

/*---------------------------------------------------------------------------*/
/* usbFunctionWrite                                                          */
/*---------------------------------------------------------------------------*/

uchar usbFunctionWrite( uchar *data, uchar len )
{
    memcpy( modeBuffer, data, 7 );
    return 1;
}



static uchar tos, val, val2;
static char rbuf[8];

static uchar u2h( uchar u )
{
    if( u>9 )
        u    += 7;
    return u+'0';
}

static uchar h2u( uchar h )
{
    h    -= '0';
    if( h>9 )
        h    -= 7;
    return h;
}

static void out_char( uchar c )
{
    tbuf[twcnt++]    = c;
#if TBUF_SZ<256
    twcnt   &= TBUF_MSK;
#endif
}


void usbFunctionWriteOut( uchar *data, uchar len )
{

    /*  postpone receiving next data    */
    usbDisableAllRequests();

    /*    host -> device:  request   */
    do {
        char    c;

        //    delimiter?
        c    = *data++;
        if( c>0x20 ) {
            if( 'a'<=c && c<='z' )
                c    -= 0x20;        //    to upper case
            rbuf[rcnt++]    = c;
            rcnt    &= 7;
            continue;
        }
        if( rcnt==0 )
            continue;

        //    command
        if( rcnt==1 ) {
            const char *ptr;
            volatile uchar  *addr   = (uchar *)((unsigned int)tos);
            uchar           x;

            switch( rbuf[0] ) {
            case '@':   //    who
                        ptr = PSTR( CMD_WHO );
                        while( (c=pgm_read_byte(ptr++))!=0 ) {
                            out_char(c); 
                        }
                        break;
            case '?':   //    get
                        x    = *addr;
                        out_char( u2h(x>>4) ); 
                        out_char( u2h(x&0x0f) ); 
                        break;
            case '=':   //    set
                        cli();
                        *addr    = val;
                        sei();
                        break;
            case '$':   //    set twice
                        cli();
                        *addr    = val;
                        *addr    = val2;
                        sei();
                        break;
            case '&':   //    and & set
                        cli();
                        *addr    &= val;
                        sei();
                        break;
            case '|':   //    or & set
                        cli();
                        *addr    |= val;
                        sei();
                        break;
            case '^':   //    xor & set
                        cli();
                        *addr    ^= val;
                        sei();
                        break;
            default:    //    error
                        out_char( '!' ); 
            }
            out_char( '\r' ); 
            out_char( '\n' ); 
            rcnt    = 0;
            continue;
        }

        //    number
        if( rcnt==2 ) {
            val2    = val;
            val     = tos;
            tos     = (h2u(rbuf[0])<<4) | h2u(rbuf[1]);
            rcnt    = 0;
            continue;
        }

        //    sfr
        if( rcnt>=4 ) {
            val2    = val;
            val    = tos;
            tos    = 0x30 + ( 'D' - rbuf[--rcnt] ) * 3;
            rbuf[rcnt]    = 0;
            if( !strcmp_P(rbuf,PSTR("PIN")) )
                tos    += 0;
            else if( !strcmp_P(rbuf,PSTR("DDR")) )
                tos    += 1;
            else if( !strcmp_P(rbuf,PSTR("PORT")) )
                tos    += 2;
            else
                tos    = 0x20;        //    error
            rcnt    = 0;
        }
    } while(--len);

    usbEnableAllRequests();
}



static uchar   intr_flag[4];

#define INTR_REG(x)     { intr_flag[x>>3] |= 1<<(x&7); }

#define INTR_MIN    2
#define INTR_MAX    15
ISR( INT0_vect )            INTR_REG(2)
ISR( TIMER1_COMPA_vect )    INTR_REG(4)
//ISR( TIMER1_OVF_vect )      INTR_REG(5)
//ISR( TIMER0_OVF_vect )      INTR_REG(6)
ISR( EE_RDY_vect )          INTR_REG(7)
ISR( ANA_COMP_vect )        INTR_REG(8)
ISR( ADC_vect )             INTR_REG(9)
ISR( TIMER1_COMPB_vect )    INTR_REG(10)
ISR( TIMER0_COMPA_vect )    INTR_REG(11)
ISR( TIMER0_COMPB_vect )    INTR_REG(12)
ISR( WDT_vect )             INTR_REG(13)
ISR( USI_START_vect )       INTR_REG(14)
ISR( USI_OVF_vect )         INTR_REG(15)

void report_interrupt(void)
{
uchar    i, j;

    for( i=INTR_MIN; i<=INTR_MAX; i++ ) {
        j   = i >> 3;
        if( intr_flag[j]==0 ) {
            i   = ( ++j << 3 ) - 1;
            continue;
        }
        if( intr_flag[j] & 1<<(i&7) ) {
            intr_flag[j] &= ~(1<<(i&7));

            out_char( '\\' ); 
            out_char( u2h(i>>4) ); 
            out_char( u2h(i&0x0f) ); 
            out_char( '\r' ); 
            out_char( '\n' ); 
            break;
        }
    }
}


void hardwareInit(void)
{
    uchar    i;

    /* activate pull-ups except on USB lines */
    USB_CFG_IOPORT   = (uchar)~((1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT));
    /* all pins input except USB (-> USB reset) */
    USBDDR    = (1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT);

    for(i=0;i<20;i++){  /* 300 ms disconnect */
        wdt_reset();
            _delay_ms(15);
    }

    USBDDR    = 0;      /*  remove USB reset condition */

}

