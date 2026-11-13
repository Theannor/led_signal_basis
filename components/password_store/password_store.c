#include <string.h>
#include "password_store.h"
#include "esp_log.h"
#include "nvs.h"
#include "led_password.h"

// Ablageort im NVS
#define PASSWORD_STORE_NAMESPACE  "led_signal"
#define PASSWORD_STORE_KEY        "ap_password"

static const char *TAG = "password_store";

esp_err_t password_store_get(char *out, uint32_t (*rand_fn)(void))
{
    if (out == NULL || rand_fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(PASSWORD_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }

    // Gespeichertes Passwort lesen
    char stored[LED_PASSWORD_LEN + 1] = {0};
    size_t length = sizeof(stored);
    err = nvs_get_str(handle, PASSWORD_STORE_KEY, stored, &length);
    if (err == ESP_OK && led_password_is_valid(stored)) {
        memcpy(out, stored, sizeof(stored));
        nvs_close(handle);
        return ESP_OK;
    }
    if (err == ESP_OK || err == ESP_ERR_NVS_INVALID_LENGTH) {
        ESP_LOGW(TAG, "Gespeichertes Passwort ungültig, erzeuge neues");
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Passwort lesen fehlgeschlagen (%s), erzeuge neues", esp_err_to_name(err));
    }

    // Neues Passwort erzeugen und speichern
    led_password_generate(out, rand_fn);
    err = nvs_set_str(handle, PASSWORD_STORE_KEY, out);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Passwort speichern fehlgeschlagen: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Neues AP-Passwort erzeugt und gespeichert");
    }
    nvs_close(handle);
    return err;
}
