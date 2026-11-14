// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <stdio.h>
#include <string.h>
#include "led_palette.h"

// Wert einer Hex-Ziffer, -1 bei ungültigem Zeichen
static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// Liest zwei Hex-Ziffern als Byte; false bei ungültigem Zeichen
static bool hex_byte(const char *s, uint8_t *out)
{
    int hi = hex_digit(s[0]);
    int lo = hex_digit(s[1]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    *out = (uint8_t)(hi * 16 + lo);
    return true;
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    return p;
}

// Liest einen JSON-String "#rrggbb" bzw. "rrggbb" ab p (p zeigt auf '"').
// Gibt die Position nach dem schließenden '"' zurück, NULL bei Fehler.
static const char *parse_color(const char *p, led_color_t *out)
{
    if (*p != '"') {
        return NULL;
    }
    p++;
    if (*p == '#') {
        p++;
    }
    // Länge vorher prüfen, damit hex_byte nicht über das Stringende liest
    for (int i = 0; i < 6; i++) {
        if (p[i] == '\0') {
            return NULL;
        }
    }
    if (!hex_byte(p, &out->r) || !hex_byte(p + 2, &out->g) || !hex_byte(p + 4, &out->b)) {
        return NULL;
    }
    p += 6;
    return *p == '"' ? p + 1 : NULL;
}

// Liest ein Array von Farben ab p (p zeigt auf '['). Gibt die Position nach
// ']' zurück, NULL bei Fehler oder zu vielen Farben.
static const char *parse_array(const char *p, led_palette_t *out)
{
    if (*p != '[') {
        return NULL;
    }
    out->count = 0;
    p = skip_ws(p + 1);
    if (*p == ']') {
        return p + 1;
    }
    while (true) {
        if (out->count >= LED_PALETTE_MAX) {
            return NULL;
        }
        p = parse_color(p, &out->colors[out->count]);
        if (p == NULL) {
            return NULL;
        }
        out->count++;
        p = skip_ws(p);
        if (*p == ']') {
            return p + 1;
        }
        if (*p != ',') {
            return NULL;
        }
        p = skip_ws(p + 1);
    }
}

bool led_palette_from_json(const char *json, led_palette_t *out)
{
    if (json == NULL || out == NULL) {
        return false;
    }
    led_palette_t tmp = {0};
    const char *p = skip_ws(json);
    if (*p == '{') {
        // Einziger erlaubter Schlüssel: "colors"
        static const char key[] = "\"colors\"";
        p = skip_ws(p + 1);
        if (strncmp(p, key, sizeof(key) - 1) != 0) {
            return false;
        }
        p = skip_ws(p + sizeof(key) - 1);
        if (*p != ':') {
            return false;
        }
        p = parse_array(skip_ws(p + 1), &tmp);
        if (p == NULL) {
            return false;
        }
        p = skip_ws(p);
        if (*p != '}') {
            return false;
        }
        p++;
    } else {
        p = parse_array(p, &tmp);
        if (p == NULL) {
            return false;
        }
    }
    if (*skip_ws(p) != '\0') {
        return false;
    }
    *out = tmp;
    return true;
}

int led_palette_to_json(const led_palette_t *palette, char *buf, size_t len)
{
    if (palette == NULL || buf == NULL || len == 0 || palette->count > LED_PALETTE_MAX) {
        return -1;
    }
    size_t pos = 0;
    int n = snprintf(buf, len, "{\"colors\":[");
    if (n < 0 || (size_t)n >= len) {
        return -1;
    }
    pos = (size_t)n;
    for (size_t i = 0; i < palette->count; i++) {
        const led_color_t *c = &palette->colors[i];
        n = snprintf(buf + pos, len - pos, "%s\"#%02x%02x%02x\"",
                     i > 0 ? "," : "", c->r, c->g, c->b);
        if (n < 0 || (size_t)n >= len - pos) {
            return -1;
        }
        pos += (size_t)n;
    }
    n = snprintf(buf + pos, len - pos, "]}");
    if (n < 0 || (size_t)n >= len - pos) {
        return -1;
    }
    return (int)(pos + (size_t)n);
}

bool led_palette_from_bytes(const uint8_t *data, size_t len, led_palette_t *out)
{
    if (out == NULL || (data == NULL && len > 0)) {
        return false;
    }
    if (len % 3 != 0 || len / 3 > LED_PALETTE_MAX) {
        return false;
    }
    led_palette_t tmp = {0};
    tmp.count = len / 3;
    for (size_t i = 0; i < tmp.count; i++) {
        tmp.colors[i].r = data[3 * i];
        tmp.colors[i].g = data[3 * i + 1];
        tmp.colors[i].b = data[3 * i + 2];
    }
    *out = tmp;
    return true;
}

int led_palette_to_bytes(const led_palette_t *palette, uint8_t *buf, size_t len)
{
    if (palette == NULL || palette->count > LED_PALETTE_MAX) {
        return -1;
    }
    size_t need = 3 * palette->count;
    if (need > len || (buf == NULL && need > 0)) {
        return -1;
    }
    for (size_t i = 0; i < palette->count; i++) {
        buf[3 * i] = palette->colors[i].r;
        buf[3 * i + 1] = palette->colors[i].g;
        buf[3 * i + 2] = palette->colors[i].b;
    }
    return (int)need;
}

// FNV-1a: einfach, schnell und für kleine Datenmengen gut genug gestreut
#define FNV_OFFSET  2166136261u
#define FNV_PRIME   16777619u

static uint32_t fnv_add(uint32_t hash, uint8_t byte)
{
    return (hash ^ byte) * FNV_PRIME;
}

uint32_t led_palette_hash(const led_palette_t *palette)
{
    if (palette == NULL || palette->count > LED_PALETTE_MAX) {
        return 0;
    }
    uint32_t hash = fnv_add(FNV_OFFSET, (uint8_t)palette->count);
    for (size_t i = 0; i < palette->count; i++) {
        hash = fnv_add(hash, palette->colors[i].r);
        hash = fnv_add(hash, palette->colors[i].g);
        hash = fnv_add(hash, palette->colors[i].b);
    }
    return hash;
}

bool led_palette_is_off(const led_palette_t *palette)
{
    if (palette == NULL || palette->count == 0 || palette->count > LED_PALETTE_MAX) {
        return false;
    }
    for (size_t i = 0; i < palette->count; i++) {
        const led_color_t *c = &palette->colors[i];
        if (c->r != 0 || c->g != 0 || c->b != 0) {
            return false;
        }
    }
    return true;
}
