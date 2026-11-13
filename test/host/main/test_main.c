#include <stdlib.h>
#include "unity.h"
#include "led_output.h"
#include "Mockled_strip.h"
#include "Mockled_strip_rmt.h"
#include "Mocknvs.h"
#include "Mockesp_wifi.h"
#include "Mockesp_wifi_default.h"
#include "Mockesp_now.h"
#include "Mockesp_event.h"
#include "Mockesp_netif.h"
#include "Mockmdns.h"
#include "Mockesp_http_server.h"

// Testgruppen aus den einzelnen Dateien
void run_led_color_tests(void);
void run_led_output_tests(void);
void run_led_password_tests(void);
void run_led_twinkle_tests(void);
void run_password_store_tests(void);
void run_wifi_ap_tests(void);
void run_net_config_tests(void);
void run_web_form_tests(void);
void run_config_store_tests(void);
void run_wifi_sta_tests(void);
void run_net_name_tests(void);
void run_web_server_tests(void);
void run_led_palette_tests(void);
void run_palette_store_tests(void);
void run_web_api_tests(void);
void run_ble_name_store_tests(void);
void run_ble_api_tests(void);
void run_web_ble_tests(void);
void run_espnow_config_tests(void);
void run_espnow_proto_tests(void);
void run_espnow_scan_tests(void);
void run_espnow_store_tests(void);
void run_espnow_link_tests(void);
void run_espnow_support_tests(void);
void run_web_espnow_tests(void);
void run_led_config_tests(void);
void run_led_store_tests(void);
void run_web_led_tests(void);

void setUp(void)
{
    Mockled_strip_Init();
    Mockled_strip_rmt_Init();
    Mocknvs_Init();
    Mockesp_wifi_Init();
    Mockesp_wifi_default_Init();
    Mockesp_now_Init();
    Mockesp_event_Init();
    Mockesp_netif_Init();
    Mockmdns_Init();
    Mockesp_http_server_Init();
}

void tearDown(void)
{
    // Erwartungen des Tests prüfen, dann led_output für den nächsten Test zurücksetzen
    Mockled_strip_Verify();
    Mockled_strip_rmt_Verify();
    Mocknvs_Verify();
    Mockesp_wifi_Verify();
    Mockesp_wifi_default_Verify();
    Mockesp_now_Verify();
    Mockesp_event_Verify();
    Mockesp_netif_Verify();
    Mockmdns_Verify();
    Mockesp_http_server_Verify();
    led_strip_del_IgnoreAndReturn(ESP_OK);
    led_output_deinit();
    Mockled_strip_Destroy();
    Mockled_strip_rmt_Destroy();
    Mocknvs_Destroy();
    Mockesp_wifi_Destroy();
    Mockesp_wifi_default_Destroy();
    Mockesp_now_Destroy();
    Mockesp_event_Destroy();
    Mockesp_netif_Destroy();
    Mockmdns_Destroy();
    Mockesp_http_server_Destroy();
}

void app_main(void)
{
    UNITY_BEGIN();
    run_led_color_tests();
    run_led_output_tests();
    run_led_password_tests();
    run_led_twinkle_tests();
    run_password_store_tests();
    run_wifi_ap_tests();
    run_net_config_tests();
    run_web_form_tests();
    run_config_store_tests();
    run_wifi_sta_tests();
    run_net_name_tests();
    run_web_server_tests();
    run_led_palette_tests();
    run_palette_store_tests();
    run_web_api_tests();
    run_ble_name_store_tests();
    run_ble_api_tests();
    run_web_ble_tests();
    run_espnow_config_tests();
    run_espnow_proto_tests();
    run_espnow_scan_tests();
    run_espnow_store_tests();
    run_espnow_link_tests();
    run_espnow_support_tests();
    run_web_espnow_tests();
    run_led_config_tests();
    run_led_store_tests();
    run_web_led_tests();
    exit(UNITY_END());
}
