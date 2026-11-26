// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
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

// Schaltet den Access Point ab: im reinen AP-Modus wird das WLAN gestoppt,
// im AP+STA-Modus (ESP-NOW) bleibt nur die STA-Schnittstelle aktiv.
// Ohne aktiven AP passiert nichts (ESP_OK). Setzt die Geräteanzahl auf 0.
esp_err_t wifi_ap_stop(void);

// Anzahl der aktuell verbundenen Geräte
int wifi_ap_client_count(void);

// Zeitpunkt (ms seit Start) der letzten Abmeldung eines Geräts; vor der
// ersten Abmeldung der Startzeitpunkt des AP
uint32_t wifi_ap_last_left_ms(void);

#ifdef __cplusplus
}
#endif
