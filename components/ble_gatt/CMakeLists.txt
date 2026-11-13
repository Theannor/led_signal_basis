# NimBLE-Anbindung (nur auf dem Board, nicht im Host-Test)
idf_component_register(SRCS "ble_gatt.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES bt ble_api led_signal freertos log)
