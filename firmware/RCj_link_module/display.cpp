#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>
#include "WString.h"
#include "OLEDDisplay.h"
#include "esp_err.h"
#include <SSD1306.h>

#include "definitions.h"
#include "images.h"
#include "fonts.h"
#include "pairing.h"
#include "monitor.h"
#include "display.h"

#define COUNT_REFRESH_MS 150   // throttle for the live TX/RX byte counts

static SSD1306 display(0x3c, I2C_SDA_GPIO, I2C_SCL_GPIO);

// Only redraw when the rendered content actually changes (avoids flicker / I2C churn).
static link_state_t last_state = (link_state_t)0xFF;
static uint16_t last_code = 0xFFFF;
static int last_alive = -1;
static uint32_t last_tx = 0xFFFFFFFF;
static uint32_t last_rx = 0xFFFFFFFF;
static int last_rssi_shown = 0x7FFF;
static uint32_t last_draw_time = 0;

int8_t display_init()
{
    display.init();
    return ESP_OK;
}

int8_t display_screen_boot()
{
    display.clear();
    display.drawXbm(0, 0, 128, 64, RC_logo);

    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 54, ("v " + String(FW_VERSION_MAJOR) + "." + String(FW_VERSION_MINOR)));

    display.display();
    return ESP_OK;
}

static void draw_unpaired()
{
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 8, "Not paired");
    display.drawString(64, 36, "Hold B2 + B3 (5s)\nto pair");
    display.display();
}

static void draw_pairing()
{
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 8, "Pairing...");
    display.drawString(64, 36, "Bring the two\nmodules together");
    display.display();
}

static void draw_paired(uint16_t code, bool alive, uint32_t tx, uint32_t rx,
                        bool has_rssi, int8_t rssi)
{
    char codebuf[8];
    snprintf(codebuf, sizeof(codebuf), "%04u", code);

    display.clear();

    // Top row: connection status (left) + RSSI (right).
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, alive ? "LINKED" : "NO LINK");
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    if (alive && has_rssi) {
        display.drawString(128, 0, String(rssi) + "dBm");
    } else {
        display.drawString(128, 0, "--");
    }

    // Large, centered 4-digit pairing code.
    display.setFont(Dialog_plain_40);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 12, codebuf);

    // Bottom row: small TX / RX byte counters (the UART monitor).
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 54, "TX:" + String(tx) + "  RX:" + String(rx));

    display.display();
}

int8_t display_update()
{
    link_state_t s = pairing_state();
    uint16_t code = pairing_code();
    int alive = pairing_link_alive() ? 1 : 0;
    uint32_t tx = monitor_tx_bytes();
    uint32_t rx = monitor_rx_bytes();
    int8_t rssi = 0;
    bool has_rssi = pairing_rssi(&rssi);

    bool structural = (s != last_state || code != last_code || alive != last_alive);
    bool live_changed = (s == LINK_PAIRED &&
                         (tx != last_tx || rx != last_rx || (int)rssi != last_rssi_shown));

    if (!structural && !live_changed) {
        return ESP_OK;   // nothing to do
    }
    // Throttle live (count/RSSI) refreshes so rapid traffic can't thrash the I2C bus.
    if (!structural && (millis() - last_draw_time) < COUNT_REFRESH_MS) {
        return ESP_OK;
    }

    last_state = s;
    last_code = code;
    last_alive = alive;
    last_tx = tx;
    last_rx = rx;
    last_rssi_shown = (int)rssi;
    last_draw_time = millis();

    switch (s) {
        case LINK_PAIRING: draw_pairing();                                  break;
        case LINK_PAIRED:  draw_paired(code, alive, tx, rx, has_rssi, rssi); break;
        case LINK_UNPAIRED:
        default:           draw_unpaired();                                 break;
    }
    return ESP_OK;
}
