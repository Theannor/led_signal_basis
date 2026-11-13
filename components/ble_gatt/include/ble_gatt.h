#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Startet den Bluetooth-LE-Stack (NimBLE) als Peripheral mit dem Dienst
// "LED Signal Palette" und wirbt unter device_name. Lese- und Schreibzugriffe
// auf die Paletten-Characteristic gehen an ble_api (vorher ble_api_init aufrufen).
esp_err_t ble_gatt_start(const char *device_name);

#ifdef __cplusplus
}
#endif
