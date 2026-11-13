idf_component_register(SRCS "espnow_store.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common led_signal
                       PRIV_REQUIRES nvs_flash log)
