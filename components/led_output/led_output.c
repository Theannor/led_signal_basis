#include "led_output.h"
#include "esp_log.h"
#include "led_strip.h"

// RMT-Auflösung: 10 MHz reicht für das WS2812-Timing
#define LED_OUTPUT_RMT_RESOLUTION_HZ  (10 * 1000 * 1000)

static const char *TAG = "led_output";

static led_strip_handle_t s_strip = NULL;
static size_t s_count = 0;

esp_err_t led_output_init(int gpio, size_t count)
{
    if (s_strip != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = count,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    // clk_src bleibt 0: der Treiber nimmt dann den Standard-Takt
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = LED_OUTPUT_RMT_RESOLUTION_HZ,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Strip anlegen fehlgeschlagen: %s", esp_err_to_name(err));
        s_strip = NULL;
        return err;
    }
    s_count = count;
    ESP_LOGI(TAG, "Strip an GPIO %d mit %u LEDs bereit", gpio, (unsigned)count);
    return ESP_OK;
}

esp_err_t led_output_show(const led_color_t *colors, size_t count)
{
    if (s_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (colors == NULL || count > s_count) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < count; i++) {
        esp_err_t err = led_strip_set_pixel(s_strip, i, colors[i].r, colors[i].g, colors[i].b);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Pixel %u setzen fehlgeschlagen: %s", (unsigned)i, esp_err_to_name(err));
            return err;
        }
    }

    esp_err_t err = led_strip_refresh(s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Strip aktualisieren fehlgeschlagen: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t led_output_deinit(void)
{
    if (s_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = led_strip_del(s_strip);
    s_strip = NULL;
    s_count = 0;
    return err;
}
