#pragma once

#include "esp_err.h"
#include "net_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lädt die Client-Konfiguration aus dem NVS. Fehlende oder ungültige Werte
// werden ersetzt: SSID und Passwort leer, Hostname = default_hostname.
// cfg ist danach immer gültig befüllt, auch wenn ein Fehler zurückkommt.
esp_err_t config_store_load(net_config_t *cfg, const char *default_hostname);

// Prüft cfg (ESP_ERR_INVALID_ARG wenn ungültig) und speichert es im NVS.
esp_err_t config_store_save(const net_config_t *cfg);

#ifdef __cplusplus
}
#endif
