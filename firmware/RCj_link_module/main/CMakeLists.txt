set(APP_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(OLED_DIR "${APP_DIR}/libraries/ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays/src")

idf_component_register(
    SRCS
        "app_main.cpp"
        "${APP_DIR}/espnow_link.cpp"
        "${APP_DIR}/storage.cpp"
        "${APP_DIR}/pairing.cpp"
        "${APP_DIR}/bridge.cpp"
        "${APP_DIR}/buzzer.cpp"
        "${APP_DIR}/buttons.cpp"
        "${APP_DIR}/monitor.cpp"
        "${APP_DIR}/usbtest.cpp"
        "${APP_DIR}/display.cpp"
        "${OLED_DIR}/OLEDDisplay.cpp"
        "${OLED_DIR}/OLEDDisplayUi.cpp"
    INCLUDE_DIRS
        "."
        "${APP_DIR}"
        "${OLED_DIR}"
    REQUIRES
        espressif__arduino-esp32
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=overloaded-virtual)
