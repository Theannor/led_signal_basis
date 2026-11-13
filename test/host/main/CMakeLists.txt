idf_component_register(SRCS "test_main.c" "test_led_color.c" "test_led_output.c" "test_led_password.c" "test_led_twinkle.c"
                            "test_password_store.c" "test_wifi_ap.c"
                            "test_net_config.c" "test_web_form.c"
                            "test_config_store.c" "test_wifi_sta.c" "test_net_name.c"
                            "test_web_server.c" "test_led_palette.c" "test_palette_store.c"
                            "test_web_api.c" "test_ble_name_store.c" "test_ble_api.c" "test_web_ble.c"
                            "test_espnow_config.c" "test_espnow_proto.c" "test_espnow_scan.c"
                            "test_espnow_store.c" "test_espnow_link.c" "test_espnow_support.c"
                            "test_web_espnow.c" "test_led_config.c" "test_led_store.c" "test_web_led.c"
                       REQUIRES unity led_signal led_output led_strip password_store config_store palette_store ble_name_store ble_api espnow_store espnow_link led_store nvs_flash
                                wifi_ap wifi_sta net_name mdns web_server esp_http_server esp_wifi esp_event esp_netif)
