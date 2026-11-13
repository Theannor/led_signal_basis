#include <string.h>
#include "led_password.h"

// Erlaubte Buchstaben
static const char LETTERS[] = "rgb";

void led_password_generate(char *out, uint32_t (*rand_fn)(void))
{
    if (out == NULL || rand_fn == NULL) {
        return;
    }
    for (size_t i = 0; i < LED_PASSWORD_LEN; i++) {
        out[i] = LETTERS[rand_fn() % 3];
    }
    out[LED_PASSWORD_LEN] = '\0';
}

bool led_password_is_valid(const char *pw)
{
    if (pw == NULL || strlen(pw) != LED_PASSWORD_LEN) {
        return false;
    }
    for (size_t i = 0; i < LED_PASSWORD_LEN; i++) {
        if (pw[i] != 'r' && pw[i] != 'g' && pw[i] != 'b') {
            return false;
        }
    }
    return true;
}

// Farbe zu einem Passwort-Buchstaben; Unbekanntes bleibt dunkel
static led_color_t color_for_letter(char c)
{
    switch (c) {
    case 'r': return (led_color_t){.r = 255, .g = 0,   .b = 0};
    case 'g': return (led_color_t){.r = 0,   .g = 255, .b = 0};
    case 'b': return (led_color_t){.r = 0,   .g = 0,   .b = 255};
    default:  return (led_color_t){0};
    }
}

void led_password_frame(const char *pw, size_t highlight, uint8_t brightness, led_color_t *out)
{
    if (pw == NULL || out == NULL) {
        return;
    }
    const led_color_t white = {.r = 255, .g = 255, .b = 255};
    for (size_t i = 0; i < LED_PASSWORD_LEN; i++) {
        led_color_t c = (i == highlight) ? white : color_for_letter(pw[i]);
        out[i] = led_color_scale(c, brightness);
    }
}
