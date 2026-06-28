#include <Arduino.h>
#include <string.h>

#include "definitions.h"
#include "espnow_link.h"
#include "bridge.h"
#include "monitor.h"
#if ENABLE_USB_TEST_CONSOLE
#include "usbtest.h"
#endif

static bool have_peer = false;
static uint8_t peer_mac[ESPNOW_MAC_LEN];

static uint8_t tx_buf[ESPNOW_MAX_PAYLOAD];
static uint8_t tx_len = 0;
static uint32_t last_byte_time = 0;

static void flush_tx()
{
    if (tx_len == 0 || !have_peer) {
        tx_len = 0;
        return;
    }
    espnow_send(peer_mac, PKT_DATA, tx_buf, tx_len);
    monitor_on_tx(tx_len);
    tx_len = 0;
}

void bridge_set_peer(const uint8_t mac[ESPNOW_MAC_LEN])
{
    memcpy(peer_mac, mac, ESPNOW_MAC_LEN);
    have_peer = true;
}

void bridge_clear_peer()
{
    have_peer = false;
    tx_len = 0;
}

void bridge_update()
{
    if (!have_peer) {
        // Drop anything the robot sent while unpaired so it can't back up the FIFO.
        while (Serial.available()) {
            Serial.read();
        }
        return;
    }

    // Drain available UART bytes into the buffer, flushing whenever it fills.
    while (Serial.available()) {
        tx_buf[tx_len++] = (uint8_t)Serial.read();
        last_byte_time = millis();
        if (tx_len >= ESPNOW_MAX_PAYLOAD) {
            flush_tx();
        }
    }

    // Flush a partial buffer once the line goes briefly idle (keeps latency low).
    if (tx_len > 0 && (millis() - last_byte_time) >= BRIDGE_FLUSH_IDLE_MS) {
        flush_tx();
    }
}

void bridge_write_uart(const uint8_t *data, uint8_t len)
{
    if (len > 0) {
        Serial.write(data, len);          // out to the robot on UART0 (U3 header)
        monitor_on_rx(len);               // count + blue LED flash
#if ENABLE_USB_TEST_CONSOLE
        usbtest_report_rx(data, len);     // mirror to the USB console for observation
#endif
    }
}

void bridge_inject(const uint8_t *data, uint16_t len)
{
    if (!have_peer) {
        return;
    }
    uint16_t off = 0;
    while (off < len) {
        uint16_t chunk = len - off;
        if (chunk > ESPNOW_MAX_PAYLOAD) {
            chunk = ESPNOW_MAX_PAYLOAD;
        }
        espnow_send(peer_mac, PKT_DATA, data + off, (uint8_t)chunk);
        monitor_on_tx(chunk);             // count + red LED flash
        off += chunk;
    }
}

void bridge_send_test()
{
    if (!have_peer) {
        return;
    }
    static const char test_payload[] = "RCJLINK-TESTPKT\n";
    uint8_t len = (uint8_t)(sizeof(test_payload) - 1);   // exclude NUL
    espnow_send(peer_mac, PKT_DATA, (const uint8_t *)test_payload, len);
    monitor_on_tx(len);                   // count + red LED flash
}
