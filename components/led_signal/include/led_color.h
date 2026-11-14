// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// RGB-Farbe mit 8 Bit pro Kanal
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

// Skaliert jeden Kanal mit brightness/255 (kaufmännisch gerundet).
// brightness 255 lässt die Farbe unverändert, 0 ergibt Schwarz.
led_color_t led_color_scale(led_color_t color, uint8_t brightness);

// Voll gesättigte Farbe zum Farbton hue in Grad (0 = Rot, 120 = Grün,
// 240 = Blau). Werte ab 360 werden auf 0..359 zurückgeführt.
led_color_t led_color_from_hue(uint16_t hue);

// Füllt colors mit einem Regenbogen über count LEDs. LED i bekommt den
// Farbton (offset + i * 360 / count) % 360, skaliert auf brightness.
// Ein wachsender offset lässt den Regenbogen wandern.
void led_rainbow_fill(led_color_t *colors, size_t count, uint16_t offset, uint8_t brightness);

// Helligkeit eines gleichmäßigen Pulsierens (Dreieck): 0 bei t = 0, 255 bei
// halber Periode, dann wieder abnehmend. period_ms 0 ergibt immer 255.
uint8_t led_pulse_level(uint32_t t_ms, uint32_t period_ms);

#ifdef __cplusplus
}
#endif
