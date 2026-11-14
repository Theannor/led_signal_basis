// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "ble_name_store.h"
#include "esp_log.h"
#include "net_config.h"
#include "nvs.h"

// Ablageort im NVS (gleicher Namespace wie die übrigen Einstellungen)
#define BLE_NAME_STORE_NAMESPACE  "led_signal"
#define BLE_NAME_STORE_KEY        "ble_name"

static const char *TAG = "ble_name_store";

esp_err_t ble_name_store_load(char *out, size_t len, const char *default_name)
{
    if (out == NULL || len == 0 || default_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(out, default_name, len);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(BLE_NAME_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    char tmp[NET_CONFIG_BLE_NAME_MAX + 1] = {0};
    size_t tmp_len = sizeof(tmp);
    err = nvs_get_str(handle, BLE_NAME_STORE_KEY, tmp, &tmp_len);
    nvs_close(handle);

    if (err == ESP_OK && net_config_ble_name_is_valid(tmp) && strlen(tmp) < len) {
        strlcpy(out, tmp, len);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Gespeicherter Bluetooth-Name ungültig (%s), nutze \"%s\"",
                 esp_err_to_name(err), default_name);
    }
    return ESP_OK;
}

esp_err_t ble_name_store_save(const char *name)
{
    if (!net_config_ble_name_is_valid(name)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(BLE_NAME_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_str(handle, BLE_NAME_STORE_KEY, name);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth-Name speichern fehlgeschlagen: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Bluetooth-Name \"%s\" gespeichert", name);
    }
    return err;
}
