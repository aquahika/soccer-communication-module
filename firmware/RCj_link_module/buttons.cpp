#include <Arduino.h>

#include "definitions.h"
#include "buttons.h"

#define B1_DEBOUNCE_MS 30

static bool both_held = false;
static bool fired = false;
static uint32_t hold_start = 0;

// B1 edge detection.
static bool b1_last = false;          // debounced "pressed" state
static bool b1_raw_last = false;
static uint32_t b1_change_time = 0;

void buttons_init()
{
    // External 10k pull-ups exist on the board; enable internal too for safety.
    pinMode(BUTTON1_GPIO, INPUT_PULLUP);
    pinMode(BUTTON2_GPIO, INPUT_PULLUP);
    pinMode(BUTTON3_GPIO, INPUT_PULLUP);
}

bool buttons_test_pressed()
{
    bool raw = (digitalRead(BUTTON1_GPIO) == LOW);   // active-low

    if (raw != b1_raw_last) {
        b1_raw_last = raw;
        b1_change_time = millis();
    }

    bool fired_now = false;
    if ((millis() - b1_change_time) > B1_DEBOUNCE_MS && raw != b1_last) {
        b1_last = raw;
        if (raw) {
            fired_now = true;   // fire on the press edge
        }
    }
    return fired_now;
}

bool buttons_pairing_gesture()
{
    bool both = (digitalRead(BUTTON2_GPIO) == LOW) && (digitalRead(BUTTON3_GPIO) == LOW);

    if (!both) {
        both_held = false;
        fired = false;
        return false;
    }

    if (!both_held) {
        both_held = true;
        fired = false;
        hold_start = millis();
    }

    if (!fired && (millis() - hold_start) >= PAIRING_HOLD_TIME) {
        fired = true;          // one-shot until released
        return true;
    }
    return false;
}
