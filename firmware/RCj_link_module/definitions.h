#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION          (FW_VERSION_MAJOR*0xFF + FW_VERSION_MINOR)

/*** GPIOs (ESP32-C5, V7/2026 board) ***/
// I2C OLED
#define I2C_SDA_GPIO        2
#define I2C_SCL_GPIO        3

// Buttons (active-low)
#define BUTTON1_GPIO        10   // B1 (SW5) — sends a test packet
#define BUTTON2_GPIO        7    // B2 (SW1) — pairing gesture (with B3)
#define BUTTON3_GPIO        6    // B3 (SW2) — pairing gesture (with B2)

// Buzzer (passive, ~2.7 kHz, NPN-driven, active-high)
#define BUZZER_GPIO         26

// RGB LED (common-cathode, active-high). NOTE: the board has a Red/Green footprint
// swap, so the as-built colors differ from the schematic labels (see the hardware
// reference). These macros are named by the color that ACTUALLY lights:
#define LED_RED_GPIO        24   // schematic "Green" pad -> lights RED   (used for TX)
#define LED_BLUE_GPIO       23   // schematic "Blue" pad  -> lights BLUE  (used for RX)
#define LED_GREEN_GPIO      27   // schematic "Red" pad   -> lights GREEN (kept off)
#define LED_BLINK_MS        50   // TX/RX activity flash length (0.05 s)
// LEDs are PWM-dimmed so "on" is 30% brightness (not full glare).
#define LED_PWM_FREQ        5000 // Hz
#define LED_PWM_RES         8    // bits
#define LED_BRIGHTNESS_PCT  30   // %
#define LED_DUTY_ON         (((1 << LED_PWM_RES) - 1) * LED_BRIGHTNESS_PCT / 100)  // ~76/255

/*** Robot UART (UART0 = Serial, U3 header TX0/RX0) ***/
// Flashing happens over USB-C, so UART0 is free for robot data at runtime.
#define BRIDGE_UART_BAUD    115200

/*** ESP-NOW link ***/
// Both modules must sit on the same 2.4 GHz channel (neither joins an AP).
#define ESPNOW_WIFI_CHANNEL 1
// Max application payload carried in one PKT_DATA frame (ESP-NOW caps frames at 250 B;
// 1 byte is the type header, leave margin).
#define ESPNOW_MAX_PAYLOAD  200

/*** Pairing ***/
// Hold B2 + B3 together this long to (re-)enter pairing mode.
#define PAIRING_HOLD_TIME       5000   // ms
// A candidate must be at least this strong (closer = higher/less-negative RSSI).
#define PAIR_RSSI_MIN           (-30)  // dBm — tune on hardware (very close proximity required)
// While pairing, broadcast a beacon this often and collect candidates this long.
#define PAIR_BEACON_INTERVAL    200    // ms
#define PAIR_SELECT_WINDOW      1000   // ms
// Give up pairing after this long with no qualifying peer.
#define PAIRING_TIMEOUT         30000  // ms
// Buzzer confirmation length on successful pairing.
#define PAIR_BUZZER_MS          500    // ms

/*** Bridge ***/
// Flush buffered UART bytes when the buffer fills or after this idle gap.
#define BRIDGE_FLUSH_IDLE_MS    5      // ms
// Send a keep-alive this often so each side can show link-alive status.
#define HEARTBEAT_INTERVAL      1000   // ms
#define LINK_TIMEOUT            3000   // ms with no peer frame => link considered down

/*** Buzzer PWM ***/
#define BUZZER_FREQ_HZ          2700
#define BUZZER_LEDC_CHANNEL     0
#define BUZZER_LEDC_RESOLUTION  8      // bits

/*** USB test console ***/
// When 1, a line-based command console is exposed on the USB-Serial-JTAG (USB-C)
// so user interaction (pairing) and robot UART traffic can be simulated/observed
// over USB for bench testing — WITHOUT touching UART0 or blocking firmware flashing
// (reset-to-bootloader on the C5 is hardware-level on this same peripheral).
// Set to 0 for a production build.
#define ENABLE_USB_TEST_CONSOLE 1

#endif // DEFINITIONS_H
