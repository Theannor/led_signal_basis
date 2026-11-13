#pragma once

#include "esp_err.h"
#include "led_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lädt die Farbpalette aus dem NVS. Fehlt sie oder ist sie ungültig, ist
// out leer (count 0) und es kommt ESP_OK zurück. Bei einem NVS-Fehler ist
// out ebenfalls leer und der Fehler wird zurückgegeben.
esp_err_t palette_store_load(led_palette_t *out);

// Speichert die Palette im NVS (ESP_ERR_INVALID_ARG bei mehr als LED_PALETTE_MAX Farben)
esp_err_t palette_store_save(const led_palette_t *palette);

#ifdef __cplusplus
}
#endif
