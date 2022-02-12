/*
 * Authors: Osamu Tamura, tickelton@gmail.com
 * Licenses: AVR-CDC/CDC-IO: Proprietary, free under certain conditions. See License_CDC-IO.txt.
 *           usbdrv: GNU GPL version 2 or the GNU GPL version 3.
 *           everything else: GNU GPL version 2. See License.txt.
 * Copyright: (c) 2007 by Recursion Co., Ltd.
 *            (c) 2021 tickelton@gmail.com
 */

#include "cdc.h"
#include "oddebug.h"


#define LED1 PB4
#define MOSFET PB1
#define PIN_BUTTON PINB3
#define PORT_BUTTON PB3
#define DD_BUTTON DDB3

#define LED_OFF(mask)   (PORTB |= (uint8_t)(1 << mask))
#define LED_ON(mask)   (PORTB &= (uint8_t)~(1 << mask))

#define FET_OFF(mask)   (PORTB |= (uint8_t)(1 << mask))
#define FET_ON(mask)   (PORTB &= (uint8_t)~(1 << mask))

void ioInit(void)
{
  DDRB |= (1 << LED1 | 1 << MOSFET);
  DDRB &= ~(1 << DD_BUTTON);
  PORTB |= (1 << PORT_BUTTON);
  
  LED_OFF(LED1);
  FET_OFF(MOSFET);
}

uint8_t compare_value = 127;
uint8_t timer_counter = 0;

ISR( TIMER0_OVF_vect )
{
  if (timer_counter == 0) {
    LED_ON(LED1);
  } else if (timer_counter == compare_value) {
    LED_OFF(LED1);
  }

  ++timer_counter;
}

int main(void)
{

    wdt_enable(WDTO_1S);
    odDebugInit();
    hardwareInit();
    usbInit();

    intr3Status = 0;
    sendEmptyFrame  = 0;

    rcnt    = 0;
    twcnt   = 0;
    trcnt   = 0;


TCCR0A = 0;
TCCR0B = ( 1 << CS02 );
TIMSK = ( 1 << TOIE0 );



    sei();
    for(;;){    /* main event loop */
        wdt_reset();
        usbPoll();

        /*    device -> host    */
        if( usbInterruptIsReady() ) {
            if( twcnt!=trcnt || sendEmptyFrame ) {
                uchar    tlen;

                tlen    = twcnt>=trcnt? (twcnt-trcnt):(TBUF_SZ-trcnt);
                if( tlen>8 )
                    tlen    = 8;
                usbSetInterrupt((uchar *)tbuf+trcnt, tlen);
                trcnt   += tlen;
                trcnt   &= TBUF_MSK;
                /* send an empty block after last data block to indicate transfer end */
                sendEmptyFrame = (tlen==8 && twcnt==trcnt)? 1:0;
            }
        }

        report_interrupt();

        /* We need to report rx and tx carrier after open attempt */
        if(intr3Status != 0 && usbInterruptIsReady3()){
            static uchar serialStateNotification[10] = {0xa1, 0x20, 0, 0, 0, 0, 2, 0, 3, 0};

            if(intr3Status == 2){
                usbSetInterrupt3(serialStateNotification, 8);
            }else{
                usbSetInterrupt3(serialStateNotification+8, 2);
            }
            intr3Status--;
        }
    }
    return 0;
}

