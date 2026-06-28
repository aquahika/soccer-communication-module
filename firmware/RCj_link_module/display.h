#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Display library: https://github.com/ThingPulse/esp8266-oled-ssd1306

int8_t display_init();

// Boot splash: RC logo + firmware version.
int8_t display_screen_boot();

// Render the screen that matches the current pairing state (unpaired / pairing /
// paired). When paired it shows the shared 4-digit code + a link-alive marker.
int8_t display_update();

#endif // DISPLAY_H
