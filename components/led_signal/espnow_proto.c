// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "espnow_proto.h"

#define MAGIC0  'L'
#define MAGIC1  'S'

static void write_header(uint8_t *buf, espnow_msg_type_t type, uint32_t hash)
{
    buf[0] = MAGIC0;
    buf[1] = MAGIC1;
    buf[2] = ESPNOW_PROTO_VERSION;
    buf[3] = (uint8_t)type;
    buf[4] = (uint8_t)(hash & 0xff);
    buf[5] = (uint8_t)((hash >> 8) & 0xff);
    buf[6] = (uint8_t)((hash >> 16) & 0xff);
    buf[7] = (uint8_t)((hash >> 24) & 0xff);
}

int espnow_proto_encode_hello(uint32_t hash, uint8_t *buf, size_t len)
{
    if (buf == NULL || len < ESPNOW_PROTO_HEADER_LEN) {
        return -1;
    }
    write_header(buf, ESPNOW_MSG_HELLO, hash);
    return ESPNOW_PROTO_HEADER_LEN;
}

int espnow_proto_encode_palette(const led_palette_t *palette, uint8_t *buf, size_t len)
{
    if (palette == NULL || buf == NULL || len < ESPNOW_PROTO_HEADER_LEN) {
        return -1;
    }
    int n = led_palette_to_bytes(palette, buf + ESPNOW_PROTO_HEADER_LEN,
                                 len - ESPNOW_PROTO_HEADER_LEN);
    if (n < 0) {
        return -1;
    }
    write_header(buf, ESPNOW_MSG_PALETTE, led_palette_hash(palette));
    return ESPNOW_PROTO_HEADER_LEN + n;
}

bool espnow_proto_decode(const uint8_t *data, size_t len, espnow_msg_t *out)
{
    if (data == NULL || out == NULL || len < ESPNOW_PROTO_HEADER_LEN ||
        data[0] != MAGIC0 || data[1] != MAGIC1 || data[2] != ESPNOW_PROTO_VERSION) {
        return false;
    }
    uint32_t hash = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                    ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    espnow_msg_t msg = {.hash = hash};
    switch (data[3]) {
    case ESPNOW_MSG_HELLO:
        if (len != ESPNOW_PROTO_HEADER_LEN) {
            return false;
        }
        msg.type = ESPNOW_MSG_HELLO;
        break;
    case ESPNOW_MSG_PALETTE:
        if (!led_palette_from_bytes(data + ESPNOW_PROTO_HEADER_LEN,
                                    len - ESPNOW_PROTO_HEADER_LEN, &msg.palette) ||
            led_palette_hash(&msg.palette) != hash) {
            return false;
        }
        msg.type = ESPNOW_MSG_PALETTE;
        break;
    default:
        return false;
    }
    *out = msg;
    return true;
}
