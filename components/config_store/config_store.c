// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "config_store.h"
#include "esp_log.h"
#include "nvs.h"

// Ablageort im NVS (gleicher Namespace wie das AP-Passwort)
#define CONFIG_STORE_NAMESPACE  "led_signal"
#define KEY_SSID                "sta_ssid"
#define KEY_PASSWORD            "sta_pass"
#define KEY_HOSTNAME            "hostname"

static const char *TAG = "config_store";

// Standardwerte setzen
static void set_defaults(net_config_t *cfg, const char *default_hostname)
{
    memset(cfg, 0, sizeof(*cfg));
    strlcpy(cfg->hostname, default_hostname, sizeof(cfg->hostname));
}

// Einen Text lesen; bei Fehler bleibt out unverändert
static void read_str(nvs_handle_t handle, const char *key, char *out, size_t out_len)
{
    char tmp[NET_CONFIG_PASSWORD_MAX + 1] = {0};
    size_t len = out_len;
    if (out_len > sizeof(tmp)) {
        return;
    }
    if (nvs_get_str(handle, key, tmp, &len) == ESP_OK) {
        memcpy(out, tmp, out_len);
        out[out_len - 1] = '\0';
    }
}

esp_err_t config_store_load(net_config_t *cfg, const char *default_hostname)
{
    if (cfg == NULL || default_hostname == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    set_defaults(cfg, default_hostname);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    read_str(handle, KEY_SSID, cfg->ssid, sizeof(cfg->ssid));
    read_str(handle, KEY_PASSWORD, cfg->password, sizeof(cfg->password));
    read_str(handle, KEY_HOSTNAME, cfg->hostname, sizeof(cfg->hostname));
    nvs_close(handle);

    if (net_config_validate(cfg) != NET_CONFIG_OK) {
        ESP_LOGW(TAG, "Gespeicherte Konfiguration ungültig, nutze Standardwerte");
        set_defaults(cfg, default_hostname);
    }
    return ESP_OK;
}

esp_err_t config_store_save(const net_config_t *cfg)
{
    if (cfg == NULL || net_config_validate(cfg) != NET_CONFIG_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_str(handle, KEY_SSID, cfg->ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, KEY_PASSWORD, cfg->password);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, KEY_HOSTNAME, cfg->hostname);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Konfiguration speichern fehlgeschlagen: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Konfiguration gespeichert (SSID \"%s\", Hostname \"%s\")",
                 cfg->ssid, cfg->hostname);
    }
    return err;
}
