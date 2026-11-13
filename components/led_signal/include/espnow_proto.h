#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "led_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

// Paket: 'L' 'S', Version, Typ, Prüfsumme der Palette (4 Byte, little endian),
// danach bei PALETTE die Farben im Binärformat (je Farbe R, G, B)
#define ESPNOW_PROTO_VERSION     1
#define ESPNOW_PROTO_HEADER_LEN  8
#define ESPNOW_PROTO_MAX_LEN     (ESPNOW_PROTO_HEADER_LEN + 3 * LED_PALETTE_MAX)

typedef enum {
    ESPNOW_MSG_HELLO = 1,    // Empfänger -> Gateway: "ich habe Palette mit dieser Prüfsumme"
    ESPNOW_MSG_PALETTE = 2,  // Gateway -> Empfänger: aktuelle Palette
} espnow_msg_type_t;

typedef struct {
    espnow_msg_type_t type;
    uint32_t hash;           // led_palette_hash() der (eigenen bzw. gesendeten) Palette
    led_palette_t palette;   // nur bei ESPNOW_MSG_PALETTE
} espnow_msg_t;

// HELLO mit der Prüfsumme der eigenen Palette. Länge oder -1 (buf zu klein).
int espnow_proto_encode_hello(uint32_t hash, uint8_t *buf, size_t len);

// PALETTE mit Prüfsumme und Farben. Länge oder -1 (buf zu klein, Palette ungültig).
int espnow_proto_encode_palette(const led_palette_t *palette, uint8_t *buf, size_t len);

// Liest ein Paket. false bei falscher Kennung, Version, Länge oder wenn die
// Prüfsumme nicht zur Palette passt. out wird nur bei Erfolg geändert.
bool espnow_proto_decode(const uint8_t *data, size_t len, espnow_msg_t *out);

#ifdef __cplusplus
}
#endif
