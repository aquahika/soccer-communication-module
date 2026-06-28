#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>

// UART bridge activity monitor: running TX/RX byte counts plus RGB LED activity
// flashes (red = TX to the peer, blue = RX from the peer, green always off).
void monitor_init();

// Call every loop to turn the activity LEDs off after LED_BLINK_MS.
void monitor_update();

// Record bridged data (PKT_DATA only): bump the counter and flash the LED.
void monitor_on_tx(uint16_t n);   // module -> peer : red LED
void monitor_on_rx(uint16_t n);   // peer -> module : blue LED

uint32_t monitor_tx_bytes();
uint32_t monitor_rx_bytes();

#endif // MONITOR_H
