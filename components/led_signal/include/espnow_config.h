#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESPNOW_MAC_LEN        6
#define ESPNOW_MAC_STR_LEN    18  // "aa:bb:cc:dd:ee:ff" + '\0'
#define ESPNOW_PEERS_MAX      10  // ESP-NOW erlaubt max. 20 Peers
// Textform der Liste: je MAC 17 Zeichen + ", " als Trenner + '\0'
#define ESPNOW_PEER_LIST_STR_MAX  (ESPNOW_PEERS_MAX * (ESPNOW_MAC_STR_LEN + 1))
// Binärformat: Version, Rolle, Gateway-MAC, Anzahl, Peer-MACs
#define ESPNOW_CONFIG_BYTES_MAX   (3 + ESPNOW_MAC_LEN * (1 + ESPNOW_PEERS_MAX))

// Rolle des Geräts im ESP-NOW-Verbund
typedef enum {
    ESPNOW_ROLE_GATEWAY = 0,   // Daten per WLAN/Bluetooth annehmen, an peers weiterleiten
    ESPNOW_ROLE_RECEIVER = 1,  // Daten nur per ESP-NOW vom gateway annehmen
} espnow_role_t;

typedef struct {
    espnow_role_t role;
    uint8_t gateway[ESPNOW_MAC_LEN];                  // nur Empfänger; alles 0 = nicht gesetzt
    uint8_t peers[ESPNOW_PEERS_MAX][ESPNOW_MAC_LEN];  // nur Gateway
    size_t peer_count;                                // 0 = nichts weiterleiten
} espnow_config_t;

typedef enum {
    ESPNOW_CONFIG_OK = 0,
    ESPNOW_CONFIG_ERR_ROLE,
    ESPNOW_CONFIG_ERR_PEERS,
    ESPNOW_CONFIG_ERR_GATEWAY,
} espnow_config_result_t;

// Standard: Gateway ohne Empfänger (verhält sich wie ohne ESP-NOW)
void espnow_config_default(espnow_config_t *cfg);

// Liest "aa:bb:cc:dd:ee:ff" (auch '-' als Trenner, Groß-/Kleinschreibung egal,
// Leerraum am Rand erlaubt). Abgelehnt werden Gruppen-/Broadcast-Adressen und
// 00:00:00:00:00:00. mac wird nur bei Erfolg geändert.
bool espnow_mac_parse(const char *text, uint8_t mac[ESPNOW_MAC_LEN]);

// Schreibt die MAC als "aa:bb:cc:dd:ee:ff" (Kleinbuchstaben) nach out
void espnow_mac_format(const uint8_t mac[ESPNOW_MAC_LEN], char out[ESPNOW_MAC_STR_LEN]);

// true, wenn die MAC nicht 00:00:00:00:00:00 ist
bool espnow_mac_is_set(const uint8_t mac[ESPNOW_MAC_LEN]);

// Liest eine durch Komma getrennte MAC-Liste nach cfg->peers/peer_count.
// Leere Einträge werden übersprungen, doppelte nur einmal übernommen.
// false bei ungültiger MAC oder mehr als ESPNOW_PEERS_MAX Einträgen;
// cfg wird nur bei Erfolg geändert.
bool espnow_peer_list_parse(const char *text, espnow_config_t *cfg);

// Schreibt die Peer-Liste als "aa:..:ff, 11:..:66" nach out.
// Gibt die Länge zurück, -1 wenn out zu klein ist.
int espnow_peer_list_format(const espnow_config_t *cfg, char *out, size_t len);

// true, wenn mac in cfg->peers steht
bool espnow_config_has_peer(const espnow_config_t *cfg, const uint8_t mac[ESPNOW_MAC_LEN]);

// Prüft cfg: Empfänger braucht eine Gateway-MAC; die eigene MAC (own_mac,
// darf NULL sein) darf weder Gateway noch Peer sein.
espnow_config_result_t espnow_config_validate(const espnow_config_t *cfg,
                                              const uint8_t own_mac[ESPNOW_MAC_LEN]);

// Binärformat zum Speichern. Gibt die Länge zurück, -1 wenn buf zu klein ist.
int espnow_config_to_bytes(const espnow_config_t *cfg, uint8_t *buf, size_t len);

// Liest das Binärformat; out wird nur bei Erfolg geändert
bool espnow_config_from_bytes(const uint8_t *data, size_t len, espnow_config_t *out);

#ifdef __cplusplus
}
#endif
