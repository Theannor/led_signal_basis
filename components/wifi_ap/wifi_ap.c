// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <stdatomic.h>
#include <string.h>
#include "wifi_ap.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "wifi_ap";

// Wird im Event-Handler (Event-Task) geschrieben und in anderen Tasks gelesen
static atomic_int s_clients = 0;
static atomic_uint s_left_at_ms = 0;

static uint32_t now_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

// Zählt verbundene Geräte mit
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base != WIFI_EVENT) {
        return;
    }
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = data;
        int count = atomic_fetch_add(&s_clients, 1) + 1;
        ESP_LOGI(TAG, "Gerät " MACSTR " verbunden (%d aktiv)", MAC2STR(event->mac), count);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = data;
        // Nie unter 0 zählen
        int old = atomic_load(&s_clients);
        while (old > 0 && !atomic_compare_exchange_weak(&s_clients, &old, old - 1)) {
        }
        atomic_store(&s_left_at_ms, now_ms());
        ESP_LOGI(TAG, "Gerät " MACSTR " getrennt (%d aktiv)", MAC2STR(event->mac),
                 atomic_load(&s_clients));
    }
}

esp_err_t wifi_ap_start(const char *ssid, const char *password,
                        uint8_t channel, uint8_t max_clients)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t ssid_len = strlen(ssid);
    size_t pw_len = strlen(password);
    if (ssid_len == 0 || ssid_len > 32 || pw_len < 8 || pw_len > 63) {
        return ESP_ERR_INVALID_ARG;
    }

    atomic_store(&s_clients, 0);
    atomic_store(&s_left_at_ms, now_ms());

    if (esp_netif_create_default_wifi_ap() == NULL) {
        ESP_LOGE(TAG, "Netzwerk-Interface für den AP konnte nicht angelegt werden");
        return ESP_FAIL;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Event-Handler registrieren fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }

    wifi_config_t config = {
        .ap = {
            .ssid_len = (uint8_t)ssid_len,
            .channel = channel,
            .max_connection = max_clients,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    memcpy(config.ap.ssid, ssid, ssid_len);
    memcpy(config.ap.password, password, pw_len);

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &config);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP starten fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "AP \"%s\" auf Kanal %u gestartet", ssid, channel);
    return ESP_OK;
}

int wifi_ap_client_count(void)
{
    return atomic_load(&s_clients);
}

uint32_t wifi_ap_last_left_ms(void)
{
    return atomic_load(&s_left_at_ms);
}
