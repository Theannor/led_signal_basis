#include <string.h>
#include "palette_store.h"
#include "esp_log.h"
#include "nvs.h"

// Ablageort im NVS (gleicher Namespace wie die übrigen Einstellungen)
#define PALETTE_STORE_NAMESPACE  "led_signal"
#define PALETTE_STORE_KEY        "palette"

// Format des Blobs: 1 Byte Anzahl, dann je Farbe 3 Byte r, g, b
#define PALETTE_BLOB_MAX         (1 + 3 * LED_PALETTE_MAX)

static const char *TAG = "palette_store";

esp_err_t palette_store_load(led_palette_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(PALETTE_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    uint8_t blob[PALETTE_BLOB_MAX];
    size_t len = sizeof(blob);
    err = nvs_get_blob(handle, PALETTE_STORE_KEY, blob, &len);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Keine Palette gespeichert, nutze zufällige Farben");
        return ESP_OK;
    }
    if (err != ESP_OK || len < 1 || blob[0] > LED_PALETTE_MAX || len != 1 + 3 * (size_t)blob[0]) {
        ESP_LOGW(TAG, "Gespeicherte Palette ungültig (%s), nutze zufällige Farben",
                 esp_err_to_name(err));
        return ESP_OK;
    }
    out->count = blob[0];
    for (size_t i = 0; i < out->count; i++) {
        out->colors[i].r = blob[1 + 3 * i];
        out->colors[i].g = blob[2 + 3 * i];
        out->colors[i].b = blob[3 + 3 * i];
    }
    ESP_LOGI(TAG, "Palette mit %u Farben geladen", (unsigned)out->count);
    return ESP_OK;
}

esp_err_t palette_store_save(const led_palette_t *palette)
{
    if (palette == NULL || palette->count > LED_PALETTE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t blob[PALETTE_BLOB_MAX];
    blob[0] = (uint8_t)palette->count;
    for (size_t i = 0; i < palette->count; i++) {
        blob[1 + 3 * i] = palette->colors[i].r;
        blob[2 + 3 * i] = palette->colors[i].g;
        blob[3 + 3 * i] = palette->colors[i].b;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(PALETTE_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(handle, PALETTE_STORE_KEY, blob, 1 + 3 * palette->count);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Palette speichern fehlgeschlagen: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Palette mit %u Farben gespeichert", (unsigned)palette->count);
    }
    return err;
}
