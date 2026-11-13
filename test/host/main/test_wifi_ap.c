#include <string.h>
#include "unity.h"
#include "wifi_ap.h"
#include "Mockesp_wifi.h"
#include "Mockesp_wifi_default.h"
#include "Mockesp_event.h"

#define TEST_SSID      "led_signal_basis"
#define TEST_PASSWORD  "rgbrgbrgbrgbrgbr"

// Platzhalter für das Netzwerk-Interface; wird nie dereferenziert
static int s_dummy_netif;

// Vom Stub gemerkter Event-Handler und gesehene Konfiguration
static esp_event_handler_t s_handler;
static wifi_config_t s_seen_config;

static esp_err_t register_stub(esp_event_base_t base, int32_t id, esp_event_handler_t handler,
                               void *arg, esp_event_handler_instance_t *instance, int cmock_num_calls)
{
    (void)cmock_num_calls;
    s_handler = handler;
    return ESP_OK;
}

static esp_err_t set_config_stub(wifi_interface_t interface, wifi_config_t *conf, int cmock_num_calls)
{
    (void)cmock_num_calls;
    TEST_ASSERT_EQUAL(WIFI_IF_AP, interface);
    s_seen_config = *conf;
    return ESP_OK;
}

// Alle Aufrufe eines erfolgreichen Starts erwarten und den AP starten
static void start_ok(void)
{
    esp_netif_create_default_wifi_ap_ExpectAndReturn((esp_netif_t *)&s_dummy_netif);
    esp_wifi_init_ExpectAnyArgsAndReturn(ESP_OK);
    esp_event_handler_instance_register_Stub(register_stub);
    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_AP, ESP_OK);
    esp_wifi_set_config_Stub(set_config_stub);
    esp_wifi_start_ExpectAndReturn(ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_ap_start(TEST_SSID, TEST_PASSWORD, 1, 4));
}

// Simuliert ein Verbinden/Trennen-Event
static void send_event(int32_t id)
{
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t event = {0};
        s_handler(NULL, WIFI_EVENT, id, &event);
    } else {
        wifi_event_ap_stadisconnected_t event = {0};
        s_handler(NULL, WIFI_EVENT, id, &event);
    }
}

static void test_start_passes_ap_config(void)
{
    start_ok();
    TEST_ASSERT_EQUAL_STRING(TEST_SSID, (const char *)s_seen_config.ap.ssid);
    TEST_ASSERT_EQUAL_UINT8(strlen(TEST_SSID), s_seen_config.ap.ssid_len);
    TEST_ASSERT_EQUAL_STRING(TEST_PASSWORD, (const char *)s_seen_config.ap.password);
    TEST_ASSERT_EQUAL(WIFI_AUTH_WPA2_PSK, s_seen_config.ap.authmode);
    TEST_ASSERT_EQUAL_UINT8(1, s_seen_config.ap.channel);
    TEST_ASSERT_EQUAL_UINT8(4, s_seen_config.ap.max_connection);
}

static void test_start_rejects_bad_args(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_ap_start(NULL, TEST_PASSWORD, 1, 4));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_ap_start(TEST_SSID, NULL, 1, 4));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_ap_start("", TEST_PASSWORD, 1, 4));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_ap_start(TEST_SSID, "kurz", 1, 4));
}

static void test_start_reports_init_error(void)
{
    esp_netif_create_default_wifi_ap_ExpectAndReturn((esp_netif_t *)&s_dummy_netif);
    esp_wifi_init_ExpectAnyArgsAndReturn(ESP_ERR_NO_MEM);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, wifi_ap_start(TEST_SSID, TEST_PASSWORD, 1, 4));
}

static void test_start_reports_missing_netif(void)
{
    esp_netif_create_default_wifi_ap_ExpectAndReturn(NULL);
    TEST_ASSERT_EQUAL(ESP_FAIL, wifi_ap_start(TEST_SSID, TEST_PASSWORD, 1, 4));
}

static void test_start_reports_wifi_start_error(void)
{
    esp_netif_create_default_wifi_ap_ExpectAndReturn((esp_netif_t *)&s_dummy_netif);
    esp_wifi_init_ExpectAnyArgsAndReturn(ESP_OK);
    esp_event_handler_instance_register_Stub(register_stub);
    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_AP, ESP_OK);
    esp_wifi_set_config_Stub(set_config_stub);
    esp_wifi_start_ExpectAndReturn(ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, wifi_ap_start(TEST_SSID, TEST_PASSWORD, 1, 4));
}

static void test_events_change_client_count(void)
{
    start_ok();
    TEST_ASSERT_EQUAL_INT(0, wifi_ap_client_count());
    send_event(WIFI_EVENT_AP_STACONNECTED);
    send_event(WIFI_EVENT_AP_STACONNECTED);
    TEST_ASSERT_EQUAL_INT(2, wifi_ap_client_count());
    send_event(WIFI_EVENT_AP_STADISCONNECTED);
    TEST_ASSERT_EQUAL_INT(1, wifi_ap_client_count());
}

static void test_client_count_never_negative(void)
{
    start_ok();
    send_event(WIFI_EVENT_AP_STADISCONNECTED);
    TEST_ASSERT_EQUAL_INT(0, wifi_ap_client_count());
}

static void test_start_resets_client_count(void)
{
    start_ok();
    send_event(WIFI_EVENT_AP_STACONNECTED);
    start_ok();
    TEST_ASSERT_EQUAL_INT(0, wifi_ap_client_count());
}

// Wird von test_main.c aufgerufen
void run_wifi_ap_tests(void)
{
    RUN_TEST(test_start_passes_ap_config);
    RUN_TEST(test_start_rejects_bad_args);
    RUN_TEST(test_start_reports_init_error);
    RUN_TEST(test_start_reports_missing_netif);
    RUN_TEST(test_start_reports_wifi_start_error);
    RUN_TEST(test_events_change_client_count);
    RUN_TEST(test_client_count_never_negative);
    RUN_TEST(test_start_resets_client_count);
}
