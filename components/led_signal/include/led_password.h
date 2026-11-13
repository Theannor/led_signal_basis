#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "led_color.h"

#ifdef __cplusplus
extern "C" {
#endif

// Passwortlänge = eine LED pro Buchstabe
#define LED_PASSWORD_LEN 16

// Erzeugt LED_PASSWORD_LEN zufällige Buchstaben aus "rgb" plus '\0'.
// out muss LED_PASSWORD_LEN + 1 Zeichen fassen. rand_fn liefert Zufallszahlen
// (auf dem Board esp_random, im Test eine feste Folge).
void led_password_generate(char *out, uint32_t (*rand_fn)(void));

// true, wenn pw genau LED_PASSWORD_LEN Zeichen hat und nur r, g, b enthält
bool led_password_is_valid(const char *pw);

// Füllt out (LED_PASSWORD_LEN Farben) mit dem Passwort: r = Rot, g = Grün,
// b = Blau. Die LED an Position highlight ist weiß (highlight >= LED_PASSWORD_LEN:
// keine weiße LED). Alle Farben werden auf brightness skaliert.
void led_password_frame(const char *pw, size_t highlight, uint8_t brightness, led_color_t *out);

#ifdef __cplusplus
}
#endif
