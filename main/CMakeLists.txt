idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES led_signal led_output led_store password_store config_store palette_store
                                ble_name_store ble_api ble_gatt
                                wifi_ap wifi_sta net_name web_server
                                espnow_store espnow_link
                                nvs_flash esp_netif esp_event esp_hw_support esp_system
                                bootloader_support)
