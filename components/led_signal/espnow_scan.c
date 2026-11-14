// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <stddef.h>
#include "espnow_scan.h"

#define DEFAULT_MAX_CHANNEL  13

void espnow_scan_init(espnow_scan_t *s)
{
    if (s == NULL) {
        return;
    }
    if (s->max_channel == 0) {
        s->max_channel = DEFAULT_MAX_CHANNEL;
    }
    if (s->tries_per_channel == 0) {
        s->tries_per_channel = 1;
    }
    if (s->max_misses == 0) {
        s->max_misses = 1;
    }
    s->state = ESPNOW_SCAN_SEARCHING;
    s->channel = 1;
    s->tries = 0;
    s->misses = 0;
}

bool espnow_scan_result(espnow_scan_t *s, bool acked)
{
    if (s == NULL) {
        return false;
    }
    if (acked) {
        s->state = ESPNOW_SCAN_CONNECTED;
        s->tries = 0;
        s->misses = 0;
        return false;
    }
    if (s->state == ESPNOW_SCAN_CONNECTED) {
        s->misses++;
        if (s->misses >= s->max_misses) {
            // Neu suchen, zuerst nochmal auf dem bisherigen Kanal
            s->state = ESPNOW_SCAN_SEARCHING;
            s->misses = 0;
            s->tries = 0;
        }
        return false;
    }
    s->tries++;
    if (s->tries < s->tries_per_channel) {
        return false;
    }
    s->tries = 0;
    s->channel = (uint8_t)(s->channel % s->max_channel + 1);
    return true;
}

uint32_t espnow_scan_wait_ms(const espnow_scan_t *s)
{
    if (s == NULL) {
        return 0;
    }
    if (s->state == ESPNOW_SCAN_CONNECTED && s->misses == 0) {
        return s->keepalive_ms;
    }
    return s->retry_ms;
}
