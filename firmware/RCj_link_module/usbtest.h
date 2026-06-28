#ifndef USBTEST_H
#define USBTEST_H

#include <stdint.h>

// Line-based test/diagnostic console on the USB-Serial-JTAG (USB-C), separate from
// the robot UART0 bridge. Lets a PC simulate user interaction and robot UART traffic.
//
// Commands (newline-terminated):
//   PAIR            -> enter pairing mode (simulates the B2+B3 5 s hold)
//   SEND <text>     -> inject <text> as if the robot sent it on UART0 (-> peer)
//   TEST            -> send a fixed test packet to the peer (simulates the B1 button)
//   STATUS          -> print link state, 4-digit code, peer MAC, link-alive, tx/rx bytes
//   HELP            -> list commands
// Output lines:
//   STATE <UNPAIRED|PAIRING|PAIRED> [code=NNNN]   (printed on every transition)
//   RX <text>       -> bytes received from the peer (would go out the robot UART0)
void usbtest_init();
void usbtest_update();

// Mirror peer-received bytes to the USB console (called from the bridge).
void usbtest_report_rx(const uint8_t *data, uint8_t len);

#endif // USBTEST_H
