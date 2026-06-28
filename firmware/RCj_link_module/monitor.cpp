#include <Arduino.h>

#include "definitions.h"
#include "monitor.h"

static uint32_t tx_bytes = 0;
static uint32_t rx_bytes = 0;

static bool red_on = false;
static bool blue_on = false;
static uint32_t red_off_time = 0;
static uint32_t blue_off_time = 0;

void monitor_init()
{
    // PWM-dimmed LEDs so "on" is 30% brightness (LED_DUTY_ON).
    ledcAttach(LED_RED_GPIO, LED_PWM_FREQ, LED_PWM_RES);
    ledcAttach(LED_BLUE_GPIO, LED_PWM_FREQ, LED_PWM_RES);
    ledcAttach(LED_GREEN_GPIO, LED_PWM_FREQ, LED_PWM_RES);
    ledcWrite(LED_RED_GPIO, 0);
    ledcWrite(LED_BLUE_GPIO, 0);
    ledcWrite(LED_GREEN_GPIO, 0);    // green stays off
}

void monitor_on_tx(uint16_t n)
{
    tx_bytes += n;
    ledcWrite(LED_RED_GPIO, LED_DUTY_ON);
    red_on = true;
    red_off_time = millis() + LED_BLINK_MS;
}

void monitor_on_rx(uint16_t n)
{
    rx_bytes += n;
    ledcWrite(LED_BLUE_GPIO, LED_DUTY_ON);
    blue_on = true;
    blue_off_time = millis() + LED_BLINK_MS;
}

void monitor_update()
{
    uint32_t now = millis();
    if (red_on && (int32_t)(now - red_off_time) >= 0) {
        ledcWrite(LED_RED_GPIO, 0);
        red_on = false;
    }
    if (blue_on && (int32_t)(now - blue_off_time) >= 0) {
        ledcWrite(LED_BLUE_GPIO, 0);
        blue_on = false;
    }
}

uint32_t monitor_tx_bytes() { return tx_bytes; }
uint32_t monitor_rx_bytes() { return rx_bytes; }
