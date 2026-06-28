#include <string.h>

#include "nvs_flash.h"
#include "nvs.h"

#include "storage.h"

#define NVS_NAMESPACE "pair"
#define KEY_PEER_MAC  "peer_mac"
#define KEY_CODE      "code"
#define KEY_PAIRED    "paired"

void storage_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

bool storage_load_pair(uint8_t peer_mac[ESPNOW_MAC_LEN], uint16_t *code)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }

    bool ok = false;
    uint8_t paired = 0;
    if (nvs_get_u8(h, KEY_PAIRED, &paired) == ESP_OK && paired == 1) {
        size_t mac_len = ESPNOW_MAC_LEN;
        uint16_t stored_code = 0;
        if (nvs_get_blob(h, KEY_PEER_MAC, peer_mac, &mac_len) == ESP_OK &&
            mac_len == ESPNOW_MAC_LEN &&
            nvs_get_u16(h, KEY_CODE, &stored_code) == ESP_OK) {
            *code = stored_code;
            ok = true;
        }
    }

    nvs_close(h);
    return ok;
}

void storage_save_pair(const uint8_t peer_mac[ESPNOW_MAC_LEN], uint16_t code)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_blob(h, KEY_PEER_MAC, peer_mac, ESPNOW_MAC_LEN);
    nvs_set_u16(h, KEY_CODE, code);
    nvs_set_u8(h, KEY_PAIRED, 1);
    nvs_commit(h);
    nvs_close(h);
}

void storage_clear_pair()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_u8(h, KEY_PAIRED, 0);
    nvs_commit(h);
    nvs_close(h);
}
