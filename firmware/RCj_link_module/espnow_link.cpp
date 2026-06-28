#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "espnow_link.h"

#define RX_QUEUE_LEN 16

static const uint8_t BROADCAST_MAC[ESPNOW_MAC_LEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static QueueHandle_t rx_queue = NULL;
static uint8_t self_mac[ESPNOW_MAC_LEN] = {0};

// ESP-NOW recv callback (runs in the Wi-Fi task context, not an ISR).
// Keep it short: copy the frame and enqueue, dropping the oldest if full so the
// newest frame is never lost to a backlog (same policy as the comm firmware's BLE queue).
static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len < 1 || rx_queue == NULL) {
        return;
    }

    link_frame_t frame;
    memcpy(frame.src_mac, info->src_addr, ESPNOW_MAC_LEN);
    frame.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    frame.type = data[0];

    int payload = len - 1;
    if (payload > ESPNOW_MAX_PAYLOAD) {
        payload = ESPNOW_MAX_PAYLOAD;
    }
    frame.len = (uint8_t)payload;
    if (payload > 0) {
        memcpy(frame.data, data + 1, payload);
    }

    if (xQueueSend(rx_queue, &frame, 0) != pdTRUE) {
        link_frame_t dropped;
        xQueueReceive(rx_queue, &dropped, 0);  // make room
        xQueueSend(rx_queue, &frame, 0);
    }
}

void espnow_init()
{
    rx_queue = xQueueCreate(RX_QUEUE_LEN, sizeof(link_frame_t));

    // These may already be set up by the Arduino layer; ESP_ERR_INVALID_STATE is benign.
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    // No AP association: pin the radio to a fixed channel so both modules match.
    esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    esp_read_mac(self_mac, ESP_MAC_WIFI_STA);

    esp_now_init();
    esp_now_register_recv_cb(on_recv);

    // A broadcast peer is required before broadcasting pairing beacons.
    espnow_add_peer(BROADCAST_MAC);
}

const uint8_t *espnow_self_mac()
{
    return self_mac;
}

bool espnow_recv(link_frame_t *out)
{
    if (rx_queue == NULL) {
        return false;
    }
    return xQueueReceive(rx_queue, out, 0) == pdTRUE;
}

void espnow_add_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) {
        return;
    }
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, mac, ESPNOW_MAC_LEN);
    peer.channel = ESPNOW_WIFI_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void espnow_remove_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) {
        esp_now_del_peer(mac);
    }
}

bool espnow_send(const uint8_t *mac, uint8_t type, const uint8_t *data, uint8_t len)
{
    if (len > ESPNOW_MAX_PAYLOAD) {
        len = ESPNOW_MAX_PAYLOAD;
    }
    uint8_t buf[1 + ESPNOW_MAX_PAYLOAD];
    buf[0] = type;
    if (len > 0 && data != NULL) {
        memcpy(buf + 1, data, len);
    }
    const uint8_t *dest = (mac == NULL) ? BROADCAST_MAC : mac;
    return esp_now_send(dest, buf, 1 + len) == ESP_OK;
}

int espnow_mac_cmp(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, ESPNOW_MAC_LEN);
}
