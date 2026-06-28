#include <Arduino.h>
#include <HWCDC.h>
#include <string.h>
#include <stdio.h>

#include "definitions.h"
#include "espnow_link.h"
#include "pairing.h"
#include "bridge.h"
#include "monitor.h"
#include "usbtest.h"

// Our own USB-Serial-JTAG CDC instance (distinct from Serial = UART0). The Arduino
// HWCDCSerial global only exists when ARDUINO_USB_CDC_ON_BOOT is set; we keep that off
// so Serial stays on UART0, and nothing else uses the USB-Serial-JTAG, so owning one
// HWCDC here is safe. Flashing still works (reset-to-bootloader is hardware-level).
static HWCDC USBSerial;

#define USB_LINE_MAX 256
static char line[USB_LINE_MAX];
static uint16_t line_len = 0;
static link_state_t last_state = (link_state_t)0xFF;

static const char *state_name(link_state_t s)
{
    switch (s) {
        case LINK_UNPAIRED: return "UNPAIRED";
        case LINK_PAIRING:  return "PAIRING";
        case LINK_PAIRED:   return "PAIRED";
        default:            return "?";
    }
}

static void print_state()
{
    link_state_t s = pairing_state();
    if (s == LINK_PAIRED) {
        USBSerial.printf("STATE PAIRED code=%04u peer=", pairing_code());
        const uint8_t *m = pairing_peer();
        int8_t rssi = 0;
        bool has_rssi = pairing_rssi(&rssi);
        USBSerial.printf("%02X:%02X:%02X:%02X:%02X:%02X alive=%d rssi=%d tx=%lu rx=%lu\n",
                         m[0], m[1], m[2], m[3], m[4], m[5], pairing_link_alive() ? 1 : 0,
                         has_rssi ? rssi : 0,
                         (unsigned long)monitor_tx_bytes(), (unsigned long)monitor_rx_bytes());
    } else {
        USBSerial.printf("STATE %s\n", state_name(s));
    }
}

static void handle_line()
{
    // Trim trailing CR/whitespace.
    while (line_len > 0 && (line[line_len - 1] == '\r' || line[line_len - 1] == ' ')) {
        line_len--;
    }
    line[line_len] = '\0';

    if (strcmp(line, "PAIR") == 0) {
        pairing_start();
        USBSerial.println("OK PAIR");
    } else if (strncmp(line, "SEND ", 5) == 0) {
        const char *payload = line + 5;
        bridge_inject((const uint8_t *)payload, strlen(payload));
        USBSerial.printf("OK SEND %u\n", (unsigned)strlen(payload));
    } else if (strcmp(line, "TEST") == 0) {
        bridge_send_test();
        USBSerial.println("OK TEST");
    } else if (strcmp(line, "STATUS") == 0) {
        print_state();
    } else if (strcmp(line, "HELP") == 0 || line_len == 0) {
        USBSerial.println("CMDS: PAIR | SEND <text> | TEST | STATUS | HELP");
    } else {
        USBSerial.printf("ERR unknown: %s\n", line);
    }
}

void usbtest_init()
{
    USBSerial.begin(0);
    // CRITICAL: never let console writes block the main loop. With the default
    // 100 ms TX timeout, writing to USBSerial while the port is plugged into a
    // host that is NOT draining it (e.g. powered from a PC with no serial monitor
    // open) blocks ~2 s per write, stalling the loop so monitor_update() can't
    // clear the activity LEDs (blue stuck on = apparent freeze). A 0 ms timeout
    // makes writes drop instead of block when the buffer is full.
    USBSerial.setTxTimeoutMs(0);
    USBSerial.println("RCj_link_module USB test console ready");
    USBSerial.println("CMDS: PAIR | SEND <text> | TEST | STATUS | HELP");
}

void usbtest_update()
{
    while (USBSerial.available()) {
        char c = (char)USBSerial.read();
        if (c == '\n') {
            handle_line();
            line_len = 0;
        } else if (line_len < USB_LINE_MAX - 1) {
            line[line_len++] = c;
        }
    }

    // Announce pairing state transitions (e.g. PAIRING -> PAIRED with the code).
    link_state_t s = pairing_state();
    if (s != last_state) {
        last_state = s;
        print_state();
    }
}

void usbtest_report_rx(const uint8_t *data, uint8_t len)
{
    if (!USBSerial) {
        return;   // no host attached — skip (operator bool == CDC connected)
    }
    USBSerial.print("RX ");
    USBSerial.write(data, len);
    USBSerial.println();
}
