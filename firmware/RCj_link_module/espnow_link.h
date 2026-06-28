#ifndef ESPNOW_LINK_H
#define ESPNOW_LINK_H

#include <stdint.h>
#include <stdbool.h>

#include "definitions.h"

#define ESPNOW_MAC_LEN 6

// Packet type — first byte of every ESP-NOW frame this firmware sends.
typedef enum {
    PKT_PAIR_BEACON  = 0,  // broadcast while pairing; no payload
    PKT_PAIR_CONFIRM = 1,  // unicast leader -> follower; payload: 2-byte code (little-endian)
    PKT_PAIR_ACK     = 2,  // unicast follower -> leader; payload: 2-byte code (little-endian)
    PKT_DATA         = 3,  // unicast; payload: raw UART bytes
    PKT_HEARTBEAT    = 4,  // unicast; no payload
} pkt_type_t;

// One received frame, copied out of the ESP-NOW recv callback into a queue.
typedef struct {
    uint8_t  src_mac[ESPNOW_MAC_LEN];
    int8_t   rssi;
    uint8_t  type;
    uint8_t  len;                        // bytes valid in data[]
    uint8_t  data[ESPNOW_MAX_PAYLOAD];
} link_frame_t;

// Bring up Wi-Fi (STA, fixed channel, no AP) + ESP-NOW + recv queue + broadcast peer.
void espnow_init();

// This module's STA MAC (6 bytes, valid after espnow_init()).
const uint8_t *espnow_self_mac();

// Pop one received frame. Returns false if the queue is empty (non-blocking).
bool espnow_recv(link_frame_t *out);

// Manage unicast peers on the fixed channel.
void espnow_add_peer(const uint8_t *mac);
void espnow_remove_peer(const uint8_t *mac);

// Send one frame. mac == NULL broadcasts. Returns true if esp_now_send() accepted it.
bool espnow_send(const uint8_t *mac, uint8_t type, const uint8_t *data, uint8_t len);

// Compare two MACs (memcmp semantics): <0, 0, >0.
int espnow_mac_cmp(const uint8_t *a, const uint8_t *b);

#endif // ESPNOW_LINK_H
