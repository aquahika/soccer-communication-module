#ifndef BRIDGE_H
#define BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#include "espnow_link.h"

// Set/clear the peer the bridge forwards UART data to. While unset, the bridge
// neither reads nor forwards UART bytes.
void bridge_set_peer(const uint8_t mac[ESPNOW_MAC_LEN]);
void bridge_clear_peer();

// Pump robot UART (UART0/Serial) -> ESP-NOW. Call every loop.
void bridge_update();

// Write bytes received over ESP-NOW out to the robot UART (UART0/Serial).
void bridge_write_uart(const uint8_t *data, uint8_t len);

// Inject simulated robot-TX bytes (USB test console): forward them to the peer
// exactly as if they had arrived on UART0. Splits into ESP-NOW-sized chunks.
void bridge_inject(const uint8_t *data, uint16_t len);

// Send a fixed test packet to the peer (B1 button / USB "TEST" command).
void bridge_send_test();

#endif // BRIDGE_H
