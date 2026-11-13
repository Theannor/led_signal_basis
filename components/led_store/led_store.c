#include "led_store.h"
#include "esp_log.h"
#include "nvs.h"

// Ablageort im NVS (gleicher Namespace wie die übrigen Einstellungen)
#define LED_STORE_NAMESPACE  "led_signal"
#define KEY_GPIO             "led_gpio"
#define KEY_COUNT            "led_count"

static const char *TAG = "led_store";

esp_err_t led_store_load(led_config_t *cfg, const led_config_t *defaults, uint64_t allowed_mask)
{
    if (cfg == NULL || defaults == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *cfg = *defaults;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(LED_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    led_config_t stored = *defaults;
    esp_err_t err_gpio = nvs_get_u8(handle, KEY_GPIO, &stored.gpio);
    esp_err_t err_count = nvs_get_u8(handle, KEY_COUNT, &stored.count);
    nvs_close(handle);

    if (err_gpio == ESP_ERR_NVS_NOT_FOUND && err_count == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;  // noch nie gespeichert
    }
    if (err_gpio == ESP_OK && err_count == ESP_OK && led_config_is_valid(&stored, allowed_mask)) {
        *cfg = stored;
    } else {
        ESP_LOGW(TAG, "Gespeicherte LED-Einstellungen ungültig, nutze GPIO %u mit %u LEDs",
                 (unsigned)defaults->gpio, (unsigned)defaults->count);
    }
    return ESP_OK;
}

esp_err_t led_store_save(const led_config_t *cfg, uint64_t allowed_mask)
{
    if (!led_config_is_valid(cfg, allowed_mask)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(LED_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_u8(handle, KEY_GPIO, cfg->gpio);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, KEY_COUNT, cfg->count);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED-Einstellungen speichern fehlgeschlagen: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "LED-Einstellungen gespeichert (GPIO %u, %u LEDs)",
                 (unsigned)cfg->gpio, (unsigned)cfg->count);
    }
    return err;
}
