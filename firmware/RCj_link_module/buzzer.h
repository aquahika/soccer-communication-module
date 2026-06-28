#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

// Configure the LEDC channel driving the passive buzzer.
void buzzer_init();

// Start a non-blocking beep for `ms` milliseconds.
void buzzer_beep(uint32_t ms);

// Call every loop: turns the buzzer off once the beep duration has elapsed.
void buzzer_update();

#endif // BUZZER_H
