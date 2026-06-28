#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#include "espnow_link.h"

// Initialise NVS (call once at boot before any load/save).
void storage_init();

// Load a persisted pairing. Returns true and fills peer_mac/code if one is stored.
bool storage_load_pair(uint8_t peer_mac[ESPNOW_MAC_LEN], uint16_t *code);

// Persist (overwrite) the current pairing.
void storage_save_pair(const uint8_t peer_mac[ESPNOW_MAC_LEN], uint16_t code);

// Forget any stored pairing.
void storage_clear_pair();

#endif // STORAGE_H
