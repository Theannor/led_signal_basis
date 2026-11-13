#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "espnow_config.h"
#include "espnow_scan.h"
#include "led_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

// Liefert die aktuelle Palette (wird aus der ESP-NOW-Task aufgerufen)
typedef void (*espnow_link_get_palette_t)(led_palette_t *out);
// Meldet eine per ESP-NOW empfangene, geänderte Palette (aus der ESP-NOW-Task)
typedef void (*espnow_link_on_palette_t)(const led_palette_t *palette);

// Einstellungen für den Empfänger
typedef struct {
    espnow_scan_t scan;          // Kanalsuche: nur die Einstellungen werden genutzt
    uint32_t ack_timeout_ms;     // so lange auf die Bestätigung eines HELLO warten
    const char *country;         // Ländercode für erlaubte Kanäle (z. B. "DE"), NULL = unverändert
} espnow_link_receiver_cfg_t;

// Gateway: ESP-NOW auf der STA-Schnittstelle starten, alle cfg->peers
// anlegen und HELLOs dieser Empfänger beantworten (aktuelle Palette senden,
// wenn deren Prüfsumme abweicht). Läuft das WLAN nur als AP, wird auf AP+STA
// umgeschaltet. Vorher muss das WLAN gestartet sein. Der Modem-Sleep bleibt an
// (Pflicht mit Bluetooth); verpasste HELLOs wiederholt der Empfänger.
esp_err_t espnow_link_gateway_start(const espnow_config_t *cfg,
                                    espnow_link_get_palette_t get_palette);

// Gateway: Palette an alle Empfänger senden. Fehler einzelner Empfänger
// werden nur geloggt (sie holen sich den Stand beim nächsten HELLO).
// ESP_ERR_INVALID_STATE, wenn das Gateway nicht läuft.
esp_err_t espnow_link_gateway_send(const led_palette_t *palette);

// Empfänger: WLAN in den STA-Modus (ohne Verbindung, ohne Stromsparen) umschalten, ESP-NOW
// starten und in einer eigenen Task den Kanal des Gateways suchen, dort
// regelmäßig HELLO senden und empfangene Paletten über on_palette melden.
// Vorher muss das WLAN gestartet sein (z. B. als AP).
esp_err_t espnow_link_receiver_start(const espnow_config_t *cfg,
                                     const espnow_link_receiver_cfg_t *rcfg,
                                     espnow_link_get_palette_t get_palette,
                                     espnow_link_on_palette_t on_palette);

// Empfänger: true, solange das Gateway auf HELLOs antwortet
bool espnow_link_receiver_connected(void);

// Beendet Tasks und ESP-NOW wieder (Gateway oder Empfänger)
void espnow_link_stop(void);

#ifdef __cplusplus
}
#endif
