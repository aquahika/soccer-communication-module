#include <Arduino.h>

#include "definitions.h"
#include "espnow_link.h"
#include "storage.h"
#include "pairing.h"
#include "bridge.h"
#include "buzzer.h"
#include "buttons.h"
#include "monitor.h"
#include "display.h"
#if ENABLE_USB_TEST_CONSOLE
#include "usbtest.h"
#endif

void setup()
{
    Serial.begin(BRIDGE_UART_BAUD);   // robot data bridge on UART0 (U3 TX0/RX0)

    storage_init();
    display_init();
    display_screen_boot();
    buttons_init();
    buzzer_init();
    monitor_init();
    espnow_init();
    pairing_init();
#if ENABLE_USB_TEST_CONSOLE
    usbtest_init();
#endif
}

void loop()
{
    if (buttons_pairing_gesture()) {
        pairing_start();
    }
    if (buttons_test_pressed()) {
        bridge_send_test();
    }
    pairing_update();
    bridge_update();
    buzzer_update();
    monitor_update();
    display_update();
#if ENABLE_USB_TEST_CONSOLE
    usbtest_update();
#endif
}
