#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "led_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lädt GPIO und Anzahl der LEDs aus dem NVS. Fehlende oder ungültige Werte
// (laut allowed_mask und LED_CONFIG_COUNT_MIN/MAX) werden durch defaults ersetzt.
// cfg ist danach immer befüllt, auch wenn ein NVS-Fehler zurückkommt.
esp_err_t led_store_load(led_config_t *cfg, const led_config_t *defaults, uint64_t allowed_mask);

// Prüft cfg (ESP_ERR_INVALID_ARG wenn ungültig) und speichert es im NVS.
esp_err_t led_store_save(const led_config_t *cfg, uint64_t allowed_mask);

#ifdef __cplusplus
}
#endif
