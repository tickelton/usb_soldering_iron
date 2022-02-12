/*
 * Authors: Osamu Tamura, tickelton@gmail.com
 * Licenses: AVR-CDC/CDC-IO: Proprietary, free under certain conditions. See License_CDC-IO.txt.
 *           usbdrv: GNU GPL version 2 or the GNU GPL version 3.
 *           everything else: GNU GPL version 2. See License.txt.
 * Copyright: (c) 2007 by Recursion Co., Ltd.
 *            (c) 2021 tickelton@gmail.com
 */

#include <string.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usbdrv.h"

#define CMD_WHO     "cdc-io"
#define TBUF_SZ     128
#define TBUF_MSK    (TBUF_SZ-1)

enum {
    SEND_ENCAPSULATED_COMMAND = 0,
    GET_ENCAPSULATED_RESPONSE,
    SET_COMM_FEATURE,
    GET_COMM_FEATURE,
    CLEAR_COMM_FEATURE,
    SET_LINE_CODING = 0x20,
    GET_LINE_CODING,
    SET_CONTROL_LINE_STATE,
    SEND_BREAK
};

uchar    modeBuffer[7];
uchar    sendEmptyFrame;
uchar    intr3Status;    /* used to control interrupt endpoint transmissions */

uchar rcnt, twcnt, trcnt;
char tbuf[TBUF_SZ];

void hardwareInit(void);
void report_interrupt(void);

