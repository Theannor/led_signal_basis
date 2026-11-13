#pragma once

#include "esp_err.h"
#include "espnow_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lädt die ESP-NOW-Einstellungen aus dem NVS. Fehlen sie oder sind sie
// ungültig, steht in out der Standard (Gateway ohne Empfänger) und es kommt
// ESP_OK zurück. Bei einem NVS-Fehler steht ebenfalls der Standard in out und
// der Fehler wird zurückgegeben.
esp_err_t espnow_store_load(espnow_config_t *out);

// Prüft cfg (ESP_ERR_INVALID_ARG wenn ungültig, ohne Vergleich mit der
// eigenen MAC) und speichert es im NVS.
esp_err_t espnow_store_save(const espnow_config_t *cfg);

#ifdef __cplusplus
}
#endif
