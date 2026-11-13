#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "wifi_sta.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#define GOT_IP_BIT  (1 << 0)  // Bit in der Event-Gruppe: IP-Adresse erhalten

static const char *TAG = "wifi_sta";

static EventGroupHandle_t s_events = NULL;
static esp_netif_t *s_netif = NULL;
static esp_event_handler_instance_t s_wifi_handler = NULL;
static esp_event_handler_instance_t s_ip_handler = NULL;

// Werden im Event-Task geschrieben und in anderen Tasks gelesen
static atomic_bool s_connected = false;
static atomic_uint s_lost_at_ms = 0;
static char s_ip[16] = "-";
static uint16_t s_last_reason = 0;  // zuletzt geloggter Trenngrund

static uint32_t now_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        // Grund nur bei Änderung loggen, sonst flutet die Wiederholschleife das Log
        if (event != NULL && event->reason != s_last_reason) {
            s_last_reason = event->reason;
            const char *text = net_disconnect_reason_text(event->reason);
            ESP_LOGW(TAG, "Getrennt, Grund %u%s%s", event->reason,
                     text ? ": " : "", text ? text : "");
        }
        if (atomic_exchange(&s_connected, false)) {
            atomic_store(&s_lost_at_ms, now_ms());
            ESP_LOGW(TAG, "Verbindung verloren, versuche erneut");
        }
        strlcpy(s_ip, "-", sizeof(s_ip));
        // Immer neu versuchen; beim Start begrenzt das Timeout die Versuche
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        atomic_store(&s_connected, true);
        s_last_reason = 0;
        ESP_LOGI(TAG, "Verbunden, IP %s", s_ip);
        xEventGroupSetBits(s_events, GOT_IP_BIT);
    }
}

// WLAN wieder vollständig abbauen (nach Fehlschlag)
static void cleanup(bool wifi_initialized)
{
    if (s_wifi_handler != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_handler);
        s_wifi_handler = NULL;
    }
    if (s_ip_handler != NULL) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_handler);
        s_ip_handler = NULL;
    }
    if (wifi_initialized) {
        esp_wifi_stop();
        esp_wifi_deinit();
    }
    if (s_netif != NULL) {
        esp_netif_destroy_default_wifi(s_netif);
        s_netif = NULL;
    }
    atomic_store(&s_connected, false);
    strlcpy(s_ip, "-", sizeof(s_ip));
}

esp_err_t wifi_sta_connect(const net_config_t *cfg, uint32_t timeout_ms)
{
    if (cfg == NULL || cfg->ssid[0] == '\0' || net_config_validate(cfg) != NET_CONFIG_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_events == NULL) {
        s_events = xEventGroupCreate();
        if (s_events == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    xEventGroupClearBits(s_events, GOT_IP_BIT);
    atomic_store(&s_connected, false);
    s_last_reason = 0;

    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == NULL) {
        ESP_LOGE(TAG, "Netzwerk-Interface für den Client konnte nicht angelegt werden");
        return ESP_FAIL;
    }
    esp_err_t err = esp_netif_set_hostname(s_netif, cfg->hostname);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Hostname setzen fehlgeschlagen: %s", esp_err_to_name(err));
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init fehlgeschlagen: %s", esp_err_to_name(err));
        cleanup(false);
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              event_handler, NULL, &s_wifi_handler);
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                  event_handler, NULL, &s_ip_handler);
    }

    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, cfg->ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, cfg->password, sizeof(config.sta.password));
    // Ohne Passwort offenes WLAN erlauben, sonst mindestens WPA2
    config.sta.threshold.authmode = cfg->password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    if (err == ESP_OK) {
        err = esp_wifi_set_mode(WIFI_MODE_STA);
    }
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_STA, &config);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Client starten fehlgeschlagen: %s", esp_err_to_name(err));
        cleanup(true);
        return err;
    }

    ESP_LOGI(TAG, "Verbinde mit \"%s\" als \"%s\" ...", cfg->ssid, cfg->hostname);
    EventBits_t bits = xEventGroupWaitBits(s_events, GOT_IP_BIT, pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(timeout_ms));
    if ((bits & GOT_IP_BIT) == 0) {
        ESP_LOGW(TAG, "Keine Verbindung zu \"%s\" nach %u ms", cfg->ssid, (unsigned)timeout_ms);
        cleanup(true);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

bool wifi_sta_is_connected(void)
{
    return atomic_load(&s_connected);
}

uint32_t wifi_sta_lost_at_ms(void)
{
    return atomic_load(&s_lost_at_ms);
}

void wifi_sta_ip(char *buf, size_t len)
{
    if (buf != NULL && len > 0) {
        strlcpy(buf, s_ip, len);
    }
}
