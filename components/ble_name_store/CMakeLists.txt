idf_component_register(SRCS "ble_name_store.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES led_signal nvs_flash log)
