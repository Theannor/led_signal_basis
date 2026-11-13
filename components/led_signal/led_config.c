#include <stddef.h>
#include "led_config.h"

bool led_config_gpio_allowed(int gpio, uint64_t allowed_mask)
{
    if (gpio < 0 || gpio >= 64) {
        return false;
    }
    return (allowed_mask & LED_CONFIG_GPIO_BIT(gpio)) != 0;
}

bool led_config_is_valid(const led_config_t *cfg, uint64_t allowed_mask)
{
    if (cfg == NULL) {
        return false;
    }
    return led_config_gpio_allowed(cfg->gpio, allowed_mask) &&
           cfg->count >= LED_CONFIG_COUNT_MIN && cfg->count <= LED_CONFIG_COUNT_MAX;
}

bool led_config_parse_uint(const char *text, unsigned max, unsigned *out)
{
    if (text == NULL || out == NULL || text[0] == '\0') {
        return false;
    }
    unsigned value = 0;
    for (const char *p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        unsigned digit = (unsigned)(*p - '0');
        // Überlauf und Obergrenze in einem Schritt prüfen
        if (digit > max || value > (max - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    *out = value;
    return true;
}
