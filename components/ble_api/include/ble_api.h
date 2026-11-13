#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "led_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

// Größte Länge des Paletten-Werts (je Farbe 3 Bytes R, G, B)
#define BLE_API_PALETTE_MAX_LEN  (3 * LED_PALETTE_MAX)

// Verbindung zur übrigen Anwendung
typedef struct {
    void (*get_palette)(led_palette_t *out);                   // aktuelle Palette liefern
    void (*on_palette_changed)(const led_palette_t *palette);  // nach dem Speichern (darf NULL sein)
} ble_api_ctx_t;

// Merkt sich ctx (wird kopiert). get_palette darf nicht NULL sein.
esp_err_t ble_api_init(const ble_api_ctx_t *ctx);

// Schreibt die aktuelle Palette im Binärformat nach buf.
// Gibt die Länge zurück, -1 bei Fehler (nicht initialisiert, buf zu klein).
int ble_api_palette_read(uint8_t *buf, size_t len);

// Setzt die Palette aus dem Binärformat, speichert sie im NVS und meldet sie
// über on_palette_changed. ESP_ERR_INVALID_SIZE bei falscher Länge,
// ESP_ERR_INVALID_STATE wenn nicht initialisiert, sonst Fehler des NVS.
esp_err_t ble_api_palette_write(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
