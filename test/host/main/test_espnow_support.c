#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_color.h"
#include "net_config.h"
#include "net_name.h"
#include "wifi_ap.h"
#include "Mockesp_wifi.h"
#include "Mockesp_wifi_default.h"
#include "Mockesp_event.h"
#include "Mockmdns.h"

// Hilfsfunktionen, die für ESP-NOW dazugekommen sind: AP-SSID mit MAC,
// Puls-Anzeige, Zeitpunkt der letzten AP-Abmeldung, mDNS beenden

static const uint8_t MAC[6] = {0x10, 0x20, 0x30, 0x40, 0xa1, 0x0b};

static void test_ap_ssid_appends_mac(void)
{
    char ssid[33];
    TEST_ASSERT_TRUE(net_ap_ssid("led_signal_basis", MAC, ssid, sizeof(ssid)));
    TEST_ASSERT_EQUAL_STRING("led_signal_basis-A10B", ssid);
}

static void test_ap_ssid_rejects_too_long(void)
{
    char ssid[64];
    // 28 + 5 = 33 Zeichen: länger als eine SSID
    TEST_ASSERT_FALSE(net_ap_ssid("abcdefghijklmnopqrstuvwxyz12", MAC, ssid, sizeof(ssid)));
    TEST_ASSERT_TRUE(net_ap_ssid("abcdefghijklmnopqrstuvwxyz1", MAC, ssid, sizeof(ssid)));
    char small[8];
    TEST_ASSERT_FALSE(net_ap_ssid("led_signal", MAC, small, sizeof(small)));
    TEST_ASSERT_EQUAL_STRING("", small);
    TEST_ASSERT_FALSE(net_ap_ssid(NULL, MAC, ssid, sizeof(ssid)));
    TEST_ASSERT_FALSE(net_ap_ssid("x", NULL, ssid, sizeof(ssid)));
}

static void test_pulse_level_triangle(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, led_pulse_level(0, 2000));
    TEST_ASSERT_EQUAL_UINT8(127, led_pulse_level(500, 2000));
    TEST_ASSERT_EQUAL_UINT8(255, led_pulse_level(1000, 2000));
    TEST_ASSERT_EQUAL_UINT8(127, led_pulse_level(1500, 2000));
    TEST_ASSERT_EQUAL_UINT8(0, led_pulse_level(2000, 2000));
    TEST_ASSERT_EQUAL_UINT8(255, led_pulse_level(5000, 2000));  // periodisch
    TEST_ASSERT_EQUAL_UINT8(255, led_pulse_level(123, 0));
    TEST_ASSERT_EQUAL_UINT8(255, led_pulse_level(0, 1));
}

static esp_event_handler_t s_handler;

static esp_err_t register_stub(esp_event_base_t base, int32_t id, esp_event_handler_t handler,
                               void *arg, esp_event_handler_instance_t *instance, int cmock_num_calls)
{
    s_handler = handler;
    return ESP_OK;
}

static int s_dummy_netif;

static void test_ap_last_left_tracks_disconnects(void)
{
    esp_netif_create_default_wifi_ap_ExpectAndReturn((esp_netif_t *)&s_dummy_netif);
    esp_wifi_init_ExpectAnyArgsAndReturn(ESP_OK);
    esp_event_handler_instance_register_Stub(register_stub);
    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_AP, ESP_OK);
    esp_wifi_set_config_IgnoreAndReturn(ESP_OK);
    esp_wifi_start_ExpectAndReturn(ESP_OK);

    uint32_t before = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
    TEST_ASSERT_EQUAL(ESP_OK, wifi_ap_start("led_signal_basis", "rgbrgbrgbrgbrgbr", 1, 4));
    uint32_t started = wifi_ap_last_left_ms();
    TEST_ASSERT_UINT32_WITHIN(20, before, started);

    vTaskDelay(pdMS_TO_TICKS(50));
    wifi_event_ap_staconnected_t on = {0};
    s_handler(NULL, WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &on);
    TEST_ASSERT_EQUAL_UINT32(started, wifi_ap_last_left_ms());  // Anmelden ändert nichts
    wifi_event_ap_stadisconnected_t off = {0};
    s_handler(NULL, WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &off);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(started + 40, wifi_ap_last_left_ms());
}

static void test_name_stop_frees_mdns(void)
{
    mdns_free_Expect();
    net_name_stop();
}

// Wird von test_main.c aufgerufen
void run_espnow_support_tests(void)
{
    RUN_TEST(test_ap_ssid_appends_mac);
    RUN_TEST(test_ap_ssid_rejects_too_long);
    RUN_TEST(test_pulse_level_triangle);
    RUN_TEST(test_ap_last_left_tracks_disconnects);
    RUN_TEST(test_name_stop_frees_mdns);
}
