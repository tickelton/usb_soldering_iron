usb_soldering_iron
------------------

COMMANDS
--------

------------------------------------------------------
| ?    | print firmware info                         |
------------------------------------------------------
| g    | get current duty cycle value                |
------------------------------------------------------
| ## s | set duty cycle value to ##                  |
|      | where '##' is a hex digit between 01 and FF |
------------------------------------------------------

EXAMPLE
-------

The below sequence of commands performs the following actions:
 * connects to the device's virtual COM port using 'screen'
 * reads the firmware version
 * sets the duty cycle to 1
 * reads back the currently active duty cycle value
 * sets the duty cycle to FF
 * reads back the currently active duty cycle value
 * sets the duty cycle to 0 (i.e. turns the soldering iron off)

$ screen /dev/ttyACM0
?
usb_solderin_iron v0.1
01 s
g
01
ff s
g
FF
00 s




LICENSE
-------

usb_soldering_iron is licensed under the terms of the GNU General Public License version 2 (GPL2).
See License.txt for details.

usb_soldering_iron contains code from Osamu Tamura/Recursion Co's AVR-CDC CDC-IO project which is licensed under the terms of the GNU General Public License version 2 (GPL2).
See License_CDC-IO.txt or https://www.recursion.jp/prose/avrcdc/ for details.

usb_soldering_iron also contains usbdrv from OBJECTIVE DEVELOPMENT GmbH's V-USB project which is distributed under the terms and conditions of the GNU GPL version 2 or the GNU GPL version 3.
See v-usb/usbdrv/License.txt or https://www.obdev.at/products/vusb/index.html for details.


