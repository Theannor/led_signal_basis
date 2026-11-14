// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "led_color.h"

#ifdef __cplusplus
extern "C" {
#endif

// Höchstzahl der Farben in einer Palette
#define LED_PALETTE_MAX 16

// Liste von Farben, aus der das Funkeln zufällig wählt (count 0 = leer)
typedef struct {
    led_color_t colors[LED_PALETTE_MAX];
    size_t count;
} led_palette_t;

// Liest eine Farbliste im JSON-Format: {"colors":["#ff0000","00ff00",...]}
// oder nur das Array ["#ff0000",...]. Jede Farbe hat 6 Hex-Ziffern, '#' ist
// optional, Groß-/Kleinschreibung egal. Höchstens LED_PALETTE_MAX Farben,
// eine leere Liste ist erlaubt. out wird nur bei Erfolg (true) geändert.
bool led_palette_from_json(const char *json, led_palette_t *out);

// Schreibt die Palette als {"colors":["#rrggbb",...]} (Kleinbuchstaben) nach buf.
// Gibt die Länge zurück, -1 wenn buf zu klein ist.
int led_palette_to_json(const led_palette_t *palette, char *buf, size_t len);

// Liest eine Palette im Binärformat: je Farbe 3 Bytes R, G, B (0..48 Bytes,
// 0 Bytes = leere Palette). false, wenn len kein Vielfaches von 3 ist oder
// mehr als LED_PALETTE_MAX Farben enthält. out wird nur bei Erfolg geändert.
bool led_palette_from_bytes(const uint8_t *data, size_t len, led_palette_t *out);

// Schreibt die Palette im Binärformat (je Farbe R, G, B) nach buf.
// Gibt die Länge zurück, -1 wenn buf zu klein oder die Palette ungültig ist.
int led_palette_to_bytes(const led_palette_t *palette, uint8_t *buf, size_t len);

// Prüfsumme über Anzahl und Farben (FNV-1a, 32 Bit). Gleiche Paletten haben
// denselben Wert; dient zum Erkennen, ob ein Gerät schon den aktuellen Stand hat.
// Ungültige Palette (NULL, mehr als LED_PALETTE_MAX Farben) ergibt 0.
uint32_t led_palette_hash(const led_palette_t *palette);

// true, wenn die Palette mindestens eine Farbe hat und alle Farben Schwarz
// (0,0,0) sind: Sonderfall "LEDs aus". Leere Palette (zufällige Farbtöne),
// NULL oder ungültige Palette ergeben false.
bool led_palette_is_off(const led_palette_t *palette);

#ifdef __cplusplus
}
#endif
