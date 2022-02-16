// Authors: Osamu Tamura, tickelton@gmail.com
// Licenses: AVR-CDC/CDC-IO: Proprietary, free under certain conditions.
//                           See License_CDC-IO.txt.
//           usbdrv: GNU GPL version 2 or the GNU GPL version 3.
//           everything else: GNU GPL version 2. See License.txt.
// Copyright: (c) 2007 by Recursion Co., Ltd.
//            (c) 2021 tickelton@gmail.com

#include <avr/io.h>
#include <avr/iotn85.h>

#include "cdc.h"
#include "oddebug.h"

#define LED1 PB4
#define MOSFET PB1
#define BUTTON_PORT PB3
#define BUTTON_DD DDB3
#define BUTTON_PIN PINB
#define BUTTON_PIN_NUM PINB3

#define PIN_OFF(mask) (PORTB |= (uint8_t)(1 << mask))
#define PIN_ON(mask) (PORTB &= (uint8_t) ~(1 << mask))
#define PIN_TOGGLE(mask) (PORTB ^= (uint8_t)(1 << mask))

#define DEBOUNCE_10MS \
  (uint8_t)(int16_t) - (F_CPU / 1024 * 10e-3 + 0.5);  // timer preload for 10ms

volatile uint8_t timer_counter = 0;
volatile uint8_t key_state;
volatile uint8_t key_press;

void ioInit(void) {
  DDRB |= (1 << LED1 | 1 << MOSFET);  // Set LED and FET ports as outputs.
  DDRB &= ~(1 << BUTTON_DD);          // Set button port as input.
  PORTB |= (1 << BUTTON_PORT);        // Enable pullup on button port.

  PIN_OFF(LED1);
  PIN_OFF(MOSFET);
}

void timersInit(void) {
  TCCR0B = (1 << CS02);  // Set clk/256 prescaler for T1.

  TCCR1 = (1 << CS13) | (1 << CS11) |
          (1 << CS10);    // Set clk/1024 prescaler for T1.
  TCNT1 = DEBOUNCE_10MS;  // Preload counter with value for 10ms overflow.

  TIMSK =
      (1 << TOIE1) | (1 << TOIE0);  // Enable overflow interrupts for T0 and T1.
}

ISR(TIMER0_OVF_vect) {
  if (timer_counter == 0) {
    PIN_ON(LED1);
    PIN_ON(MOSFET);
  }
  if (timer_counter == pwr_steps[pwr_idx]) {
    PIN_OFF(LED1);
    PIN_OFF(MOSFET);
  }

  ++timer_counter;
}

ISR(TIMER1_OVF_vect) {
  static uint8_t ct0 = 0xFF, ct1 = 0xFF;
  uint8_t i;

  TCNT1 = DEBOUNCE_10MS;

  i = key_state ^ ~BUTTON_PIN;  // key changed ?
  ct0 = ~(ct0 & i);             // reset or count ct0
  ct1 = ct0 ^ (ct1 & i);        // reset or count ct1
  i &= ct0 & ct1;               // count until roll over ?
  key_state ^= i;               // then toggle debounced state
  key_press |= key_state & i;   // 0->1: key press detect
}

// Check if a key has been pressed.
// Each pressed key is reported only once.
uint8_t get_key_press(uint8_t key_mask) {
  cli();                  // read and clear atomic !
  key_mask &= key_press;  // read key(s)
  key_press ^= key_mask;  // clear key(s)
  sei();
  return key_mask;
}

// Check if a key is pressed right now.
uint8_t get_key_state(uint8_t key_mask)

{
  key_mask &= key_state;
  return key_mask;
}

int main(void) {
  pwr_steps[0] = 0;
  pwr_steps[1] = 200;
  pwr_steps[2] = 224;
  pwr_steps[3] = 255;
  pwr_idx = 0;

  wdt_enable(WDTO_1S);
  odDebugInit();
  hardwareInit();
  usbInit();
  ioInit();
  timersInit();

  intr3Status = 0;
  sendEmptyFrame = 0;

  rcnt = 0;
  twcnt = 0;
  trcnt = 0;

  sei();
  for (;;) {
    wdt_reset();
    usbPoll();

    // device -> host
    if (usbInterruptIsReady()) {
      if (twcnt != trcnt || sendEmptyFrame) {
        uchar tlen;

        tlen = twcnt >= trcnt ? (twcnt - trcnt) : (TBUF_SZ - trcnt);
        if (tlen > 8) tlen = 8;
        usbSetInterrupt((uchar *)tbuf + trcnt, tlen);
        trcnt += tlen;
        trcnt &= TBUF_MSK;
        // Send an empty block after last data block to indicate transfer end.
        sendEmptyFrame = (tlen == 8 && twcnt == trcnt) ? 1 : 0;
      }
    }

    report_interrupt();

    // We need to report rx and tx carrier after open attempt.
    if (intr3Status != 0 && usbInterruptIsReady3()) {
      static uchar serialStateNotification[10] = {0xa1, 0x20, 0, 0, 0,
                                                  0,    2,    0, 3, 0};

      if (intr3Status == 2) {
        usbSetInterrupt3(serialStateNotification, 8);
      } else {
        usbSetInterrupt3(serialStateNotification + 8, 2);
      }
      intr3Status--;
    }

    if (get_key_press(1 << BUTTON_PIN_NUM)) {
      cli();
      if (++pwr_idx >= PWR_STEPS_LEN) {
        pwr_idx = 0;
      }
      sei();
    }
  }
  return 0;
}
