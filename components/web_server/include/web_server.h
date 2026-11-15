// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "espnow_config.h"
#include "led_config.h"
#include "led_palette.h"
#include "net_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Rahmenbedingungen für die Webseiten
typedef struct {
    bool client_mode;                          // true = WLAN-Client, false = AP
    uint16_t port;                             // HTTP-Port, meist 80
    net_config_t config;                       // aktuell gespeicherte Konfiguration
    void (*get_ip)(char *buf, size_t len);     // liefert die aktuelle IP-Adresse
    void (*schedule_restart)(void);            // nach dem Speichern aufgerufen
    led_palette_t palette;                     // aktuell gespeicherte Farbpalette
    void (*on_palette_changed)(const led_palette_t *palette);  // nach dem Speichern der Palette (darf NULL sein)
    char ble_name[NET_CONFIG_BLE_NAME_MAX + 1];  // Bluetooth-Gerätename (leer = ohne Bluetooth-Bereich)
    espnow_config_t espnow;                    // gespeicherte ESP-NOW-Einstellungen
    uint8_t own_mac[ESPNOW_MAC_LEN];           // eigene ESP-NOW-MAC (alles 0 = ohne ESP-NOW-Bereich)
    bool receiver_mode;                        // läuft als ESP-NOW-Empfänger: Farben nicht per HTTP
    unsigned espnow_window_s;                  // Einstellungsfenster eines Empfängers (nur Anzeige)
    led_config_t led;                          // gespeicherter GPIO und Anzahl der LEDs
    uint64_t led_gpio_mask;                    // wählbare GPIOs (0 = ohne LED-Bereich)
    const char *version;                       // Firmware-Version, muss gültig bleiben (NULL = unbekannt)
} web_server_ctx_t;

// Startet den HTTP-Server mit
//   GET  /        Einstellungen-Seite
//   POST /config  Einstellungen prüfen, speichern, Neustart anstoßen
//   GET  /colors       Farben-Seite
//   GET  /style.css    gemeinsames Stylesheet (Cache-Control: max-age=3600)
//   GET  /api/colors   Palette als {"colors":["#rrggbb",...]}
//   PUT  /api/colors   Palette im selben Format setzen und speichern (ohne Neustart)
//   POST /ble          Bluetooth-Gerätenamen prüfen, speichern, Neustart anstoßen
//   POST /espnow       ESP-NOW-Rolle und MAC-Adressen prüfen, speichern, Neustart anstoßen
//   POST /led          GPIO und Anzahl der LEDs prüfen, speichern, Neustart anstoßen
//   GET  /api/version  Firmware-Version als {"version":"2026.1"}
// Im Empfänger-Modus antworten /colors und PUT /api/colors mit 403.
esp_err_t web_server_start(const web_server_ctx_t *ctx);

// Übernimmt eine anderswo (z. B. per Bluetooth) geänderte Palette, damit
// GET /api/colors und die Farben-Seite sie zeigen. Speichert nicht im NVS.
// ESP_ERR_INVALID_STATE, wenn der Server nicht läuft.
esp_err_t web_server_set_palette(const led_palette_t *palette);

// Stoppt den HTTP-Server wieder
esp_err_t web_server_stop(void);

#ifdef __cplusplus
}
#endif
