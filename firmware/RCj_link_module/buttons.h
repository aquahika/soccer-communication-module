#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>

// Configure the buttons (B1 test, B2 + B3 pairing) as pulled-up inputs.
void buttons_init();

// Returns true exactly once each time B2 and B3 have been held together for
// PAIRING_HOLD_TIME. Re-arms only after both buttons are released.
bool buttons_pairing_gesture();

// Returns true once per B1 press (debounced, fires on press edge).
bool buttons_test_pressed();

#endif // BUTTONS_H
