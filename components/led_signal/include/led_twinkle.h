#pragma once

#include <stddef.h>
#include <stdint.h>
#include "led_color.h"

#ifdef __cplusplus
extern "C" {
#endif

// Einstellungen für das Funkeln: Zufallsquelle und Bereich der Fadezeit
typedef struct {
    uint32_t (*rand_fn)(void);  // auf dem Board esp_random, im Test eine feste Folge
    uint32_t min_fade_ms;       // kürzeste Fadezeit
    uint32_t max_fade_ms;       // längste Fadezeit (einschließlich)
    const led_color_t *palette; // Farben zur Auswahl; NULL oder leer = zufälliger Farbton
    size_t palette_count;       // Anzahl Farben in palette
} led_twinkle_cfg_t;

// Zustand einer LED: von Schwarz in fade_ms auf color, in fade_ms zurück auf
// Schwarz, dann beginnt ein neuer Zyklus mit neuer Farbe und Fadezeit.
typedef struct {
    led_color_t color;    // Zielfarbe bei voller Helligkeit
    uint32_t fade_ms;     // Dauer des Ein- und des Ausblendens
    uint32_t elapsed_ms;  // Zeit seit Beginn des Zyklus
} led_twinkle_t;

// Neuer Zyklus ab Schwarz: zufällige Farbe (erste Zufallszahl; aus palette,
// wenn vorhanden, sonst ein voll gesättigter Farbton), zufällige Fadezeit
// min_fade_ms..max_fade_ms (zweite Zufallszahl). Ist max kleiner als min,
// gilt min; die Fadezeit ist mindestens 1 ms.
void led_twinkle_start(led_twinkle_t *t, const led_twinkle_cfg_t *cfg);

// Aktuelle Farbe der LED, skaliert auf brightness
led_color_t led_twinkle_color(const led_twinkle_t *t, uint8_t brightness);

// Zählt für count LEDs dt_ms weiter, startet beendete Zyklen neu und
// schreibt die aktuellen Farben nach out.
void led_twinkle_update(led_twinkle_t *leds, size_t count, uint32_t dt_ms,
                        const led_twinkle_cfg_t *cfg, uint8_t brightness, led_color_t *out);

#ifdef __cplusplus
}
#endif
