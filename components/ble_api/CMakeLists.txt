# Logik der Bluetooth-Schnittstelle ohne NimBLE (auch auf dem Host testbar)
idf_component_register(SRCS "ble_api.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common led_signal
                       PRIV_REQUIRES palette_store log)
