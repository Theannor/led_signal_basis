// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include "led_color.h"

// Einen Kanal skalieren; Rechnung in 32 Bit, damit nichts überläuft
static uint8_t scale_channel(uint8_t value, uint8_t brightness)
{
    return (uint8_t)(((uint32_t)value * brightness + 127) / 255);
}

led_color_t led_color_scale(led_color_t color, uint8_t brightness)
{
    led_color_t result = {
        .r = scale_channel(color.r, brightness),
        .g = scale_channel(color.g, brightness),
        .b = scale_channel(color.b, brightness),
    };
    return result;
}

led_color_t led_color_from_hue(uint16_t hue)
{
    hue %= 360;
    // 6 Sektoren zu je 60 Grad; rise steigt im Sektor von 0 auf 255
    uint8_t sector = hue / 60;
    uint8_t rise = (uint8_t)((hue % 60) * 255 / 60);
    uint8_t fall = 255 - rise;

    switch (sector) {
    case 0:  return (led_color_t){.r = 255,  .g = rise, .b = 0};
    case 1:  return (led_color_t){.r = fall, .g = 255,  .b = 0};
    case 2:  return (led_color_t){.r = 0,    .g = 255,  .b = rise};
    case 3:  return (led_color_t){.r = 0,    .g = fall, .b = 255};
    case 4:  return (led_color_t){.r = rise, .g = 0,    .b = 255};
    default: return (led_color_t){.r = 255,  .g = 0,    .b = fall};
    }
}

void led_rainbow_fill(led_color_t *colors, size_t count, uint16_t offset, uint8_t brightness)
{
    if (colors == NULL || count == 0) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        uint32_t hue = ((uint32_t)offset + i * 360 / count) % 360;
        colors[i] = led_color_scale(led_color_from_hue((uint16_t)hue), brightness);
    }
}

uint8_t led_pulse_level(uint32_t t_ms, uint32_t period_ms)
{
    if (period_ms == 0) {
        return 255;
    }
    uint32_t phase = t_ms % period_ms;
    uint32_t half = period_ms / 2;
    if (half == 0) {
        return 255;
    }
    // Aufsteigend in der ersten Hälfte, absteigend in der zweiten
    uint32_t dist = phase <= half ? phase : period_ms - phase;
    uint32_t level = dist * 255 / half;
    return (uint8_t)(level > 255 ? 255 : level);
}
