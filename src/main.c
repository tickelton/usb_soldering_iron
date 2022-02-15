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
#define KEY_DDR         DDRB
#define KEY_PORT        PORTB
#define KEY_PIN         PINB
#define KEY0            3

#define LED_OFF(mask)   (PORTB |= (uint8_t)(1 << mask))
#define LED_ON(mask)   (PORTB &= (uint8_t)~(1 << mask))
#define LED_TOGGLE(mask)   (PORTB ^= (uint8_t)(1 << mask))

#define FET_OFF(mask)   (PORTB |= (uint8_t)(1 << mask))
#define FET_ON(mask)   (PORTB &= (uint8_t)~(1 << mask))
#define FET_TOGGLE(mask)   (PORTB ^= (uint8_t)(1 << mask))

#define DEBOUNCE_10MS (uint8_t)(int16_t)-(F_CPU / 1024 * 10e-3 + 0.5);  // timer preload for 10ms

#define PWR_STEPS_LEN 4
const uint8_t pwr_steps[PWR_STEPS_LEN] = {0, 200, 224, 255};
uint8_t pwr_idx = 0;

void ioInit(void)
{
  DDRB |= (1 << LED1 | 1 << MOSFET);
  DDRB &= ~(1 << DD_BUTTON);
  PORTB |= (1 << PORT_BUTTON);

  LED_OFF(LED1);
  FET_OFF(MOSFET);
}

uint8_t timer_counter = 0;
volatile uint8_t key_state;
volatile uint8_t key_press;

ISR( TIMER0_OVF_vect )
{
  if (timer_counter == 0) {
    LED_ON(LED1);
    FET_ON(MOSFET);
  }
  if (timer_counter == pwr_steps[pwr_idx]) {
    LED_OFF(LED1);
    FET_OFF(MOSFET);
  }

  ++timer_counter;
}

ISR( TIMER1_OVF_vect )
{
  static uint8_t ct0 = 0xFF, ct1 = 0xFF;
  uint8_t i;

  TCNT1 = DEBOUNCE_10MS;

  i = key_state ^ ~KEY_PIN;                       // key changed ?
  ct0 = ~( ct0 & i );                             // reset or count ct0
  ct1 = ct0 ^ (ct1 & i);                          // reset or count ct1
  i &= ct0 & ct1;                                 // count until roll over ?
  key_state ^= i;                                 // then toggle debounced state
  key_press |= key_state & i;                     // 0->1: key press detect
}


///////////////////////////////////////////////////////////////////
//
// check if a key has been pressed. Each pressed key is reported
// only once
//
uint8_t get_key_press( uint8_t key_mask )
{
  cli();                                          // read and clear atomic !
  key_mask &= key_press;                          // read key(s)
  key_press ^= key_mask;                          // clear key(s)
  sei();
  return key_mask;
}


///////////////////////////////////////////////////////////////////
//
// check if a key is pressed right now
//
uint8_t get_key_state( uint8_t key_mask )

{
  key_mask &= key_state;
  return key_mask;
}

int main(void)
{

    wdt_enable(WDTO_1S);
    odDebugInit();
    hardwareInit();
    usbInit();
    ioInit();

    intr3Status = 0;
    sendEmptyFrame  = 0;

    rcnt    = 0;
    twcnt   = 0;
    trcnt   = 0;


TCCR0A = 0;
TCCR0B = ( 1 << CS02 );

TCCR1 =  (1<<CS13)|(1<<CS11)|(1<<CS10);
TCNT1 = DEBOUNCE_10MS;

TIMSK = (1<<TOIE1)|( 1 << TOIE0 );


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


        if( get_key_press( 1<<KEY0 )){
          ++pwr_idx;
          pwr_idx = pwr_idx % PWR_STEPS_LEN;
        }


    }
    return 0;
}

