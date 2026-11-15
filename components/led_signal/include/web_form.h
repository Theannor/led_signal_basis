// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "web_lang.h"

#ifdef __cplusplus
extern "C" {
#endif

// Sucht key in einem Formular-Body (application/x-www-form-urlencoded) und
// schreibt den dekodierten Wert ('+' -> Leerzeichen, %XX) nach out.
// false, wenn der Schlüssel fehlt, der Wert nicht in out passt oder falsch kodiert ist.
bool web_form_get_field(const char *body, const char *key, char *out, size_t out_len);

// Entfernt Leerraum (Leerzeichen, Tab, Zeilenumbruch) am Anfang und Ende von s.
void web_form_trim(char *s);

// Ersetzt & < > " ' durch HTML-Entities. false, wenn out zu klein ist.
bool web_html_escape(const char *in, char *out, size_t out_len);

// Unterseiten der Einstellungen (Untermenü); Pfade /, /ble, /espnow, /led
typedef enum {
    WEB_SETTINGS_WIFI = 0,   // WLAN und Hostname (/)
    WEB_SETTINGS_BLE,        // Bluetooth-Name (/ble)
    WEB_SETTINGS_ESPNOW,     // ESP-NOW-Rolle (/espnow)
    WEB_SETTINGS_LED,        // LED-Strip (/led)
} web_settings_page_t;

// Daten für die Konfigurationsseite (Texte dürfen NULL sein)
typedef struct {
    web_lang_t lang;         // Sprache der Seite (0 = Deutsch)
    web_settings_page_t settings_page;  // gezeigte Unterseite der Einstellungen
    bool client_mode;        // true = als Client im WLAN, false = eigener AP
    const char *ip;          // aktuelle IP-Adresse
    const char *hostname;    // aktueller Hostname
    const char *version;     // Firmware-Version (NULL = ohne Zeile)
    const char *ssid;        // gespeicherte Client-SSID
    const char *ble_name;    // Bluetooth-Gerätename (NULL = ohne Bluetooth-Bereich)
    const char *message;     // Hinweis oben auf der Seite
    bool message_is_error;   // Hinweis als Fehler darstellen
    // ESP-NOW
    bool receiver_mode;          // läuft als ESP-NOW-Empfänger: Menü ohne Farben
    const char *own_mac;         // eigene ESP-NOW-MAC (NULL = ohne ESP-NOW-Bereich)
    bool espnow_receiver;        // gespeicherte Rolle: Empfänger (sonst Gateway)
    const char *espnow_peers;    // Empfänger-Liste des Gateways als Text
    const char *espnow_gateway;  // Gateway-MAC des Empfängers als Text
    unsigned espnow_window_s;    // so lange ist der AP eines Empfängers nach dem Start offen
    // LED-Strip
    uint64_t led_gpio_mask;      // wählbare GPIOs (0 = ohne LED-Bereich)
    unsigned led_gpio;           // gespeicherter GPIO der Datenleitung
    unsigned led_count;          // gespeicherte Anzahl LEDs
} web_page_info_t;

// Baut die Unterseite info->settings_page der Einstellungen (Menü, im Empfänger-Modus
// ohne Farben, und Untermenü der vorhandenen Unterseiten) nach buf. Bluetooth nur mit
// ble_name, ESP-NOW nur mit own_mac, LED nur mit led_gpio_mask; fehlt die Unterseite,
// wird WLAN gezeigt. Gibt die Länge zurück, -1 wenn buf zu klein ist.
// Das Client-Passwort wird nie in die Seite geschrieben.
int web_page_render(char *buf, size_t len, const web_page_info_t *info);

// Baut die Status-Seite (Modus, IP, Hostname, ESP-NOW-MAC, Version; mit Menü) nach buf.
// Nutzt aus info nur lang, client_mode, receiver_mode, ip, hostname, own_mac und version.
// Gibt die Länge zurück, -1 wenn buf zu klein ist.
int web_status_page_render(char *buf, size_t len, const web_page_info_t *info);

// Baut die Farben-Seite (mit Menü) in der Sprache lang nach buf. Die Seite lädt
// und speichert die Palette per JavaScript über GET/PUT /api/colors.
// Gibt die Länge zurück, -1 wenn buf zu klein ist.
int web_colors_page_render_lang(char *buf, size_t len, web_lang_t lang);

// Wie web_colors_page_render_lang(), auf Deutsch
int web_colors_page_render(char *buf, size_t len);

// Baut {"version":"<version>"} nach buf. Gibt die Länge zurück, -1 wenn buf
// zu klein ist oder version NULL ist bzw. Anführungszeichen, Backslash oder Steuerzeichen enthält.
int web_version_json(const char *version, char *buf, size_t len);

// Baut {"error":"<message>"} nach buf und maskiert dabei Anführungszeichen und Backslash.
// Gibt die Länge zurück, -1 wenn buf zu klein ist, message NULL ist oder Steuerzeichen enthält.
int web_error_json(const char *message, char *buf, size_t len);

// Gemeinsames Stylesheet aller Seiten (konstanter Text im Flash)
const char *web_style_css(void);

#ifdef __cplusplus
}
#endif
