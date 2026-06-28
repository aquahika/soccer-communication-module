#include <Arduino.h>

#include "definitions.h"
#include "buzzer.h"

// 50% duty at BUZZER_LEDC_RESOLUTION bits drives the passive buzzer loudest.
#define BUZZER_DUTY_ON  (1 << (BUZZER_LEDC_RESOLUTION - 1))

static bool active = false;
static uint32_t off_time = 0;

void buzzer_init()
{
    // Arduino-esp32 3.x pin-based LEDC API.
    ledcAttach(BUZZER_GPIO, BUZZER_FREQ_HZ, BUZZER_LEDC_RESOLUTION);
    ledcWrite(BUZZER_GPIO, 0);
}

void buzzer_beep(uint32_t ms)
{
    ledcWrite(BUZZER_GPIO, BUZZER_DUTY_ON);
    off_time = millis() + ms;
    active = true;
}

void buzzer_update()
{
    if (active && (int32_t)(millis() - off_time) >= 0) {
        ledcWrite(BUZZER_GPIO, 0);
        active = false;
    }
}
