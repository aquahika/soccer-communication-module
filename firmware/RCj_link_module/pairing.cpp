#include <Arduino.h>
#include <string.h>

#include "esp_random.h"

#include "definitions.h"
#include "espnow_link.h"
#include "storage.h"
#include "bridge.h"
#include "buzzer.h"
#include "pairing.h"

static link_state_t state = LINK_UNPAIRED;
static link_state_t prev_state = LINK_UNPAIRED;

static uint8_t  peer_mac[ESPNOW_MAC_LEN];
static bool     have_peer = false;
static uint16_t code = 0;

// Pairing-session working state.
static uint32_t pairing_start_time = 0;
static uint32_t last_beacon_time = 0;
static bool     has_candidate = false;
static uint8_t  best_mac[ESPNOW_MAC_LEN];
static int8_t   best_rssi = -128;
static bool     handshake_started = false;
static bool     is_leader = false;

// Link-alive / heartbeat tracking.
static uint32_t last_peer_seen = 0;
static uint32_t last_heartbeat_time = 0;
static int8_t   last_rssi = 0;        // RSSI of the most recent frame from the peer
static bool     have_rssi = false;

static void finalize(const uint8_t *mac, uint16_t c)
{
    memcpy(peer_mac, mac, ESPNOW_MAC_LEN);
    have_peer = true;
    code = c;

    espnow_add_peer(mac);
    storage_save_pair(mac, c);
    bridge_set_peer(mac);

    state = LINK_PAIRED;
    last_peer_seen = millis();
    last_heartbeat_time = millis();
    buzzer_beep(PAIR_BUZZER_MS);
}

void pairing_init()
{
    if (storage_load_pair(peer_mac, &code)) {
        have_peer = true;
        espnow_add_peer(peer_mac);
        bridge_set_peer(peer_mac);
        state = LINK_PAIRED;
        last_peer_seen = millis();
        last_heartbeat_time = millis();
    } else {
        have_peer = false;
        state = LINK_UNPAIRED;
    }
}

void pairing_start()
{
    prev_state = state;
    state = LINK_PAIRING;
    pairing_start_time = millis();
    last_beacon_time = 0;
    has_candidate = false;
    best_rssi = -128;
    handshake_started = false;
    is_leader = false;
    // Stop bridging robot data while we (re)pair; restored on success/timeout.
    bridge_clear_peer();
}

static void handle(const link_frame_t *f)
{
    switch (f->type) {
    case PKT_PAIR_BEACON:
        if (state == LINK_PAIRING && f->rssi >= PAIR_RSSI_MIN) {
            if (!has_candidate || f->rssi > best_rssi) {
                memcpy(best_mac, f->src_mac, ESPNOW_MAC_LEN);
                best_rssi = f->rssi;
                has_candidate = true;
            }
        }
        break;

    case PKT_PAIR_CONFIRM:
        if (f->len >= 2 && f->rssi >= PAIR_RSSI_MIN) {
            uint16_t c = (uint16_t)f->data[0] | ((uint16_t)f->data[1] << 8);
            if (state == LINK_PAIRING) {
                // We are the follower: adopt the leader's code, ack, and finalize.
                espnow_add_peer(f->src_mac);
                espnow_send(f->src_mac, PKT_PAIR_ACK, f->data, 2);
                finalize(f->src_mac, c);
            } else if (state == LINK_PAIRED && have_peer &&
                       espnow_mac_cmp(f->src_mac, peer_mac) == 0) {
                // Re-ack a retransmitted confirm so the leader can finalize too.
                espnow_send(f->src_mac, PKT_PAIR_ACK, f->data, 2);
            }
        }
        break;

    case PKT_PAIR_ACK:
        if (state == LINK_PAIRING && is_leader && handshake_started && f->len >= 2 &&
            has_candidate && espnow_mac_cmp(f->src_mac, best_mac) == 0) {
            uint16_t c = (uint16_t)f->data[0] | ((uint16_t)f->data[1] << 8);
            finalize(best_mac, c);
        }
        break;

    case PKT_DATA:
        if (state == LINK_PAIRED && have_peer &&
            espnow_mac_cmp(f->src_mac, peer_mac) == 0) {
            bridge_write_uart(f->data, f->len);
            last_peer_seen = millis();
            last_rssi = f->rssi;
            have_rssi = true;
        }
        break;

    case PKT_HEARTBEAT:
        if (state == LINK_PAIRED && have_peer &&
            espnow_mac_cmp(f->src_mac, peer_mac) == 0) {
            last_peer_seen = millis();
            last_rssi = f->rssi;
            have_rssi = true;
        }
        break;

    default:
        break;
    }
}

void pairing_update()
{
    link_frame_t f;
    while (espnow_recv(&f)) {
        handle(&f);
    }

    uint32_t now = millis();

    if (state == LINK_PAIRING) {
        if (now - last_beacon_time >= PAIR_BEACON_INTERVAL) {
            last_beacon_time = now;
            espnow_send(NULL, PKT_PAIR_BEACON, NULL, 0);            // broadcast presence
            if (handshake_started && is_leader && has_candidate) {  // resend confirm (loss recovery)
                uint8_t cb[2] = { (uint8_t)(code & 0xFF), (uint8_t)(code >> 8) };
                espnow_send(best_mac, PKT_PAIR_CONFIRM, cb, 2);
            }
        }

        // After the discovery window, the lower-MAC module leads code agreement.
        if (!handshake_started && has_candidate &&
            (now - pairing_start_time) >= PAIR_SELECT_WINDOW) {
            handshake_started = true;
            if (espnow_mac_cmp(espnow_self_mac(), best_mac) < 0) {
                is_leader = true;
                code = (uint16_t)(esp_random() % 10000);
                espnow_add_peer(best_mac);
                uint8_t cb[2] = { (uint8_t)(code & 0xFF), (uint8_t)(code >> 8) };
                espnow_send(best_mac, PKT_PAIR_CONFIRM, cb, 2);
            } else {
                is_leader = false;  // follower waits for the leader's confirm
            }
        }

        if ((now - pairing_start_time) >= PAIRING_TIMEOUT) {
            if (prev_state == LINK_PAIRED && have_peer) {
                bridge_set_peer(peer_mac);   // restore the previous link
                state = LINK_PAIRED;
                last_peer_seen = now;
            } else {
                state = LINK_UNPAIRED;
            }
        }
    } else if (state == LINK_PAIRED) {
        if (have_peer && (now - last_heartbeat_time) >= HEARTBEAT_INTERVAL) {
            last_heartbeat_time = now;
            espnow_send(peer_mac, PKT_HEARTBEAT, NULL, 0);
        }
    }
}

link_state_t pairing_state()       { return state; }
uint16_t     pairing_code()        { return code; }
const uint8_t *pairing_peer()      { return peer_mac; }

bool pairing_link_alive()
{
    return state == LINK_PAIRED && have_peer &&
           (millis() - last_peer_seen) < LINK_TIMEOUT;
}

bool pairing_rssi(int8_t *rssi_out)
{
    if (!have_rssi) {
        return false;
    }
    *rssi_out = last_rssi;
    return true;
}
