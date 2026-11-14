// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include "espnow_store.h"
#include "esp_log.h"
#include "nvs.h"

// Ablageort im NVS (gleicher Namespace wie die übrigen Einstellungen)
#define ESPNOW_STORE_NAMESPACE  "led_signal"
#define ESPNOW_STORE_KEY        "espnow"

static const char *TAG = "espnow_store";

esp_err_t espnow_store_load(espnow_config_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    espnow_config_default(out);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ESPNOW_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    uint8_t blob[ESPNOW_CONFIG_BYTES_MAX];
    size_t len = sizeof(blob);
    err = nvs_get_blob(handle, ESPNOW_STORE_KEY, blob, &len);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Keine ESP-NOW-Einstellungen gespeichert, nutze Gateway ohne Empfänger");
        return ESP_OK;
    }
    espnow_config_t tmp;
    if (err != ESP_OK || !espnow_config_from_bytes(blob, len, &tmp) ||
        espnow_config_validate(&tmp, NULL) != ESPNOW_CONFIG_OK) {
        ESP_LOGW(TAG, "Gespeicherte ESP-NOW-Einstellungen ungültig (%s), nutze Standard",
                 esp_err_to_name(err));
        return ESP_OK;
    }
    *out = tmp;
    return ESP_OK;
}

esp_err_t espnow_store_save(const espnow_config_t *cfg)
{
    if (cfg == NULL || espnow_config_validate(cfg, NULL) != ESPNOW_CONFIG_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t blob[ESPNOW_CONFIG_BYTES_MAX];
    int len = espnow_config_to_bytes(cfg, blob, sizeof(blob));
    if (len < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(ESPNOW_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(handle, ESPNOW_STORE_KEY, blob, (size_t)len);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW-Einstellungen speichern fehlgeschlagen: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "ESP-NOW-Einstellungen gespeichert (%s, %u Empfänger)",
                 cfg->role == ESPNOW_ROLE_RECEIVER ? "Empfänger" : "Gateway",
                 (unsigned)cfg->peer_count);
    }
    return err;
}
