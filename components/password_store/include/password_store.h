// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Liefert das AP-Passwort aus dem NVS. Fehlt es oder ist es ungültig, wird mit
// rand_fn ein neues erzeugt und gespeichert. out muss LED_PASSWORD_LEN + 1
// Zeichen fassen. nvs_flash_init() muss vorher aufgerufen worden sein.
esp_err_t password_store_get(char *out, uint32_t (*rand_fn)(void));

#ifdef __cplusplus
}
#endif
