#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "net_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Verbindet sich als Client mit cfg->ssid und wartet bis zu timeout_ms auf
// eine IP-Adresse. Erfolg: ESP_OK, die Verbindung wird danach bei Abbrüchen
// selbst wiederhergestellt. Kein Erfolg: ESP_ERR_TIMEOUT, das WLAN ist dann
// wieder komplett abgebaut (z. B. für wifi_ap_start()).
// Vorher nötig: nvs_flash_init(), esp_netif_init(), esp_event_loop_create_default().
esp_err_t wifi_sta_connect(const net_config_t *cfg, uint32_t timeout_ms);

// true, solange eine IP-Adresse vorhanden ist
bool wifi_sta_is_connected(void);

// Zeitpunkt (ms seit Start) des letzten Verbindungsverlusts
uint32_t wifi_sta_lost_at_ms(void);

// Aktuelle IP-Adresse als Text ("-" ohne Verbindung)
void wifi_sta_ip(char *buf, size_t len);

#ifdef __cplusplus
}
#endif
