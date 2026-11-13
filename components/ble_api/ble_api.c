#include "ble_api.h"
#include "esp_log.h"
#include "palette_store.h"

static const char *TAG = "ble_api";

static ble_api_ctx_t s_ctx;

esp_err_t ble_api_init(const ble_api_ctx_t *ctx)
{
    if (ctx == NULL || ctx->get_palette == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_ctx = *ctx;
    return ESP_OK;
}

int ble_api_palette_read(uint8_t *buf, size_t len)
{
    if (s_ctx.get_palette == NULL) {
        return -1;
    }
    led_palette_t palette;
    s_ctx.get_palette(&palette);
    return led_palette_to_bytes(&palette, buf, len);
}

esp_err_t ble_api_palette_write(const uint8_t *data, size_t len)
{
    if (s_ctx.get_palette == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    led_palette_t palette;
    if (!led_palette_from_bytes(data, len, &palette)) {
        ESP_LOGW(TAG, "Palette mit ungültiger Länge %u empfangen", (unsigned)len);
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err = palette_store_save(&palette);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "Palette mit %u Farben per Bluetooth gesetzt", (unsigned)palette.count);
    if (s_ctx.on_palette_changed != NULL) {
        s_ctx.on_palette_changed(&palette);
    }
    return ESP_OK;
}
