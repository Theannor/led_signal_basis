// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Erlaubte Anzahl LEDs: mindestens eine LED pro Passwort-Buchstabe (16),
// höchstens so viele, wie die USB-Versorgung sicher schafft
#define LED_CONFIG_COUNT_MIN  16
#define LED_CONFIG_COUNT_MAX  25

// Bit n in einer GPIO-Maske
#define LED_CONFIG_GPIO_BIT(n)  (1ULL << (n))

// Anschluss des LED-Strips
typedef struct {
    uint8_t gpio;   // Datenleitung
    uint8_t count;  // Anzahl LEDs
} led_config_t;

// true, wenn gpio (0..63) in allowed_mask gesetzt ist
bool led_config_gpio_allowed(int gpio, uint64_t allowed_mask);

// true, wenn der GPIO erlaubt ist und die Anzahl in
// LED_CONFIG_COUNT_MIN..LED_CONFIG_COUNT_MAX liegt
bool led_config_is_valid(const led_config_t *cfg, uint64_t allowed_mask);

// Liest eine Dezimalzahl 0..max (nur Ziffern, mindestens eine).
// false bei leerem Text, anderen Zeichen oder Wert > max; out bleibt dann unverändert.
bool led_config_parse_uint(const char *text, unsigned max, unsigned *out);

#ifdef __cplusplus
}
#endif
