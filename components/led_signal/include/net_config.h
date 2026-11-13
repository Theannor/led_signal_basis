#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Grenzen laut WLAN- bzw. Hostnamen-Regeln
#define NET_CONFIG_SSID_MAX      32
#define NET_CONFIG_PASSWORD_MIN  8
#define NET_CONFIG_PASSWORD_MAX  63
#define NET_CONFIG_HOSTNAME_MAX  32
#define NET_CONFIG_BLE_NAME_MAX  20  // passt sicher ins Advertising-Paket

// Einstellungen für den WLAN-Client
typedef struct {
    char ssid[NET_CONFIG_SSID_MAX + 1];          // leer = kein Client, nur AP
    char password[NET_CONFIG_PASSWORD_MAX + 1];  // leer = offenes WLAN
    char hostname[NET_CONFIG_HOSTNAME_MAX + 1];
} net_config_t;

// Ergebnis der Prüfung: welches Feld ist ungültig
typedef enum {
    NET_CONFIG_OK = 0,
    NET_CONFIG_ERR_SSID,
    NET_CONFIG_ERR_PASSWORD,
    NET_CONFIG_ERR_HOSTNAME,
} net_config_result_t;

// 1..32 Zeichen aus a-z, A-Z, 0-9 und '-', nicht mit '-' am Anfang oder Ende
bool net_config_hostname_is_valid(const char *hostname);

// Bluetooth-Gerätename: 1..20 druckbare ASCII-Zeichen (0x20..0x7E),
// kein Leerzeichen am Anfang oder Ende
bool net_config_ble_name_is_valid(const char *name);

// Prüft alle Felder: SSID 0..32 Zeichen, Passwort leer oder 8..63, Hostname gültig
net_config_result_t net_config_validate(const net_config_t *cfg);

// true, wenn die Verbindung seit mindestens limit_ms weg ist.
// Zeiten in ms (z. B. aus dem Tick-Zähler); ein Überlauf von now_ms ist erlaubt.
bool net_should_fall_back(uint32_t now_ms, uint32_t lost_at_ms, bool connected, uint32_t limit_ms);

// SSID des eigenen Access Points: prefix + '-' + letzte zwei MAC-Bytes in
// Hex-Großbuchstaben (z. B. "led_signal_basis-A1B2"), damit mehrere Geräte
// unterscheidbar sind. false, wenn out zu klein ist oder die SSID länger als
// NET_CONFIG_SSID_MAX würde.
bool net_ap_ssid(const char *prefix, const uint8_t mac[6], char *out, size_t len);

// Kurzer Text zu einem WLAN-Trenngrund (reason aus WIFI_EVENT_STA_DISCONNECTED),
// NULL bei unbekanntem Code
const char *net_disconnect_reason_text(uint16_t reason);

#ifdef __cplusplus
}
#endif
