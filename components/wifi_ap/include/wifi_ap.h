#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Startet einen WLAN-Access-Point (WPA2-PSK).
// Vorher nötig: nvs_flash_init(), esp_netif_init(), esp_event_loop_create_default().
// password muss 8..63 Zeichen haben.
esp_err_t wifi_ap_start(const char *ssid, const char *password,
                        uint8_t channel, uint8_t max_clients);

// Anzahl der aktuell verbundenen Geräte
int wifi_ap_client_count(void);

// Zeitpunkt (ms seit Start) der letzten Abmeldung eines Geräts; vor der
// ersten Abmeldung der Startzeitpunkt des AP
uint32_t wifi_ap_last_left_ms(void);

#ifdef __cplusplus
}
#endif
