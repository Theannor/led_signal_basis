#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "led_color.h"

#ifdef __cplusplus
extern "C" {
#endif

// Legt den WS2812B-Strip (GRB) an GPIO gpio mit count LEDs an (SPI-Backend mit DMA).
// Gibt ESP_ERR_INVALID_STATE zurück, wenn schon initialisiert.
esp_err_t led_output_init(int gpio, size_t count);

// Schreibt count Farben auf den Strip und aktualisiert ihn.
// Bei einem Fehler wird nicht aktualisiert; der Fehler wird geloggt und zurückgegeben.
esp_err_t led_output_show(const led_color_t *colors, size_t count);

// Gibt den Strip wieder frei (vor allem für Tests).
esp_err_t led_output_deinit(void);

#ifdef __cplusplus
}
#endif
