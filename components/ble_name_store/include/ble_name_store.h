// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lädt den Bluetooth-Gerätenamen aus dem NVS nach out (len Bytes inkl. '\0').
// Fehlt er oder ist er ungültig, steht danach default_name in out.
// Bei einem NVS-Fehler steht ebenfalls default_name in out und der Fehler
// wird zurückgegeben.
esp_err_t ble_name_store_load(char *out, size_t len, const char *default_name);

// Prüft den Namen (ESP_ERR_INVALID_ARG wenn ungültig) und speichert ihn im NVS.
esp_err_t ble_name_store_save(const char *name);

#ifdef __cplusplus
}
#endif
