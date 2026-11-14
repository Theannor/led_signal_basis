// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include "led_twinkle.h"

void led_twinkle_start(led_twinkle_t *t, const led_twinkle_cfg_t *cfg)
{
    if (t == NULL || cfg == NULL || cfg->rand_fn == NULL) {
        return;
    }
    uint32_t r = cfg->rand_fn();
    if (cfg->palette != NULL && cfg->palette_count > 0) {
        t->color = cfg->palette[r % cfg->palette_count];
    } else {
        t->color = led_color_from_hue((uint16_t)(r % 360));
    }

    uint32_t min = cfg->min_fade_ms;
    uint32_t max = cfg->max_fade_ms < min ? min : cfg->max_fade_ms;
    // Spanne in 64 Bit, damit max - min + 1 nicht überläuft
    uint64_t span = (uint64_t)max - min + 1;
    t->fade_ms = min + (uint32_t)(cfg->rand_fn() % span);
    if (t->fade_ms == 0) {
        t->fade_ms = 1;
    }
    t->elapsed_ms = 0;
}

led_color_t led_twinkle_color(const led_twinkle_t *t, uint8_t brightness)
{
    if (t == NULL || t->fade_ms == 0) {
        return (led_color_t){0};
    }
    // Abstand zum Schwarz: steigt bis fade_ms, fällt danach wieder
    uint32_t pos = t->elapsed_ms <= t->fade_ms ? t->elapsed_ms
                 : t->elapsed_ms < 2 * t->fade_ms ? 2 * t->fade_ms - t->elapsed_ms
                 : 0;
    uint8_t level = (uint8_t)(((uint64_t)pos * 255 + t->fade_ms / 2) / t->fade_ms);
    return led_color_scale(led_color_scale(t->color, level), brightness);
}

void led_twinkle_update(led_twinkle_t *leds, size_t count, uint32_t dt_ms,
                        const led_twinkle_cfg_t *cfg, uint8_t brightness, led_color_t *out)
{
    if (leds == NULL || out == NULL || cfg == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        leds[i].elapsed_ms += dt_ms;
        if (leds[i].elapsed_ms >= 2 * leds[i].fade_ms) {
            led_twinkle_start(&leds[i], cfg);
        }
        out[i] = led_twinkle_color(&leds[i], brightness);
    }
}
