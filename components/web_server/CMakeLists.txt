idf_component_register(SRCS "web_server.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common led_signal
                       PRIV_REQUIRES esp_http_server config_store palette_store ble_name_store espnow_store led_store freertos log)
