idf_component_register(SRCS "espnow_link.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common led_signal
                       PRIV_REQUIRES esp_wifi freertos log)
