// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Suche des WLAN-Kanals, auf dem das Gateway funkt (reine Logik, ohne Funk).
// Ablauf: auf jedem Kanal bis zu tries_per_channel HELLOs senden; kommt eine
// Bestätigung (ESP-NOW-ACK), ist der Kanal gefunden. Danach regelmäßig ein
// HELLO als Lebenszeichen; bleiben max_misses in Folge unbestätigt, beginnt
// die Suche erneut (Router hat Kanal gewechselt, Gateway aus, ...).

typedef enum {
    ESPNOW_SCAN_SEARCHING,  // Kanal unbekannt, wird durchprobiert
    ESPNOW_SCAN_CONNECTED,  // Gateway antwortet auf dem aktuellen Kanal
} espnow_scan_state_t;

typedef struct {
    // Einstellungen
    uint8_t max_channel;        // höchster Kanal (in Europa 13)
    uint8_t tries_per_channel;  // Versuche je Kanal bei der Suche (mind. 1)
    uint8_t max_misses;         // so viele verpasste Lebenszeichen -> neu suchen (mind. 1)
    uint32_t retry_ms;          // Abstand der Versuche bei der Suche bzw. nach einem Fehlschlag
    uint32_t keepalive_ms;      // Abstand der Lebenszeichen, solange alles klappt
    // Zustand
    espnow_scan_state_t state;
    uint8_t channel;            // aktueller Kanal (1..max_channel)
    uint8_t tries;              // Fehlversuche auf dem aktuellen Kanal (Suche)
    uint8_t misses;             // verpasste Lebenszeichen in Folge (verbunden)
} espnow_scan_t;

// Setzt den Zustand zurück: Suche ab Kanal 1. Die Einstellungen in s müssen
// vorher gesetzt sein; ungültige Werte (0) werden auf 1 bzw. 13 korrigiert.
void espnow_scan_init(espnow_scan_t *s);

// Ergebnis des letzten HELLO auf s->channel eintragen (acked = bestätigt).
// Gibt true zurück, wenn sich der Kanal geändert hat.
bool espnow_scan_result(espnow_scan_t *s, bool acked);

// Wartezeit bis zum nächsten HELLO
uint32_t espnow_scan_wait_ms(const espnow_scan_t *s);

#ifdef __cplusplus
}
#endif
