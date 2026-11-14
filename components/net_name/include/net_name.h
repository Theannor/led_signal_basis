// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Macht das Gerät per mDNS als <hostname>.local erreichbar und meldet die
// Webseite (HTTP auf http_port) als Dienst an.
esp_err_t net_name_start(const char *hostname, uint16_t http_port);

// Meldet den Namen und den Dienst wieder ab (mDNS aus)
void net_name_stop(void);

#ifdef __cplusplus
}
#endif
