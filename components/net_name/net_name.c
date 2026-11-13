#include "net_name.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "net_name";

esp_err_t net_name_start(const char *hostname, uint16_t http_port)
{
    if (hostname == NULL || hostname[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS starten fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    err = mdns_hostname_set(hostname);
    if (err == ESP_OK) {
        err = mdns_instance_name_set(hostname);
    }
    if (err == ESP_OK) {
        err = mdns_service_add(NULL, "_http", "_tcp", http_port, NULL, 0);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS einrichten fehlgeschlagen: %s", esp_err_to_name(err));
        mdns_free();
        return err;
    }
    ESP_LOGI(TAG, "Erreichbar als http://%s.local", hostname);
    return ESP_OK;
}

void net_name_stop(void)
{
    mdns_free();
    ESP_LOGI(TAG, "mDNS beendet");
}
