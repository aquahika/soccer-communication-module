#ifndef PAIRING_H
#define PAIRING_H

#include <stdint.h>
#include <stdbool.h>

#include "espnow_link.h"

typedef enum {
    LINK_UNPAIRED = 0,
    LINK_PAIRING  = 1,
    LINK_PAIRED   = 2,
} link_state_t;

// Load any persisted pairing and bring the link up (auto-relink on boot).
void pairing_init();

// Enter pairing mode (from the B2+B3 gesture or the USB test console).
void pairing_start();

// Drain received frames, run the pairing handshake, send heartbeats. Call every loop.
void pairing_update();

// State accessors (for display + test console).
link_state_t pairing_state();
uint16_t pairing_code();
const uint8_t *pairing_peer();   // valid when paired
bool pairing_link_alive();       // paired AND a peer frame seen within LINK_TIMEOUT
bool pairing_rssi(int8_t *rssi_out);  // latest RSSI (dBm) from the peer; false if none yet

#endif // PAIRING_H
