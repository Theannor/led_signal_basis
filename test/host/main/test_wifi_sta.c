// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "wifi_sta.h"
#include "Mockesp_wifi.h"
#include "Mockesp_wifi_default.h"
#include "Mockesp_event.h"
#include "Mockesp_netif.h"

// Der esp_netif-Mock von ESP-IDF definiert den Event-Basisnamen nicht
ESP_EVENT_DEFINE_BASE(IP_EVENT);

static int s_dummy_netif;
#define FAKE_NETIF ((esp_netif_t *)&s_dummy_netif)

static esp_event_handler_t s_handler;
static wifi_config_t s_seen_config;
static bool s_send_ip_on_start;

static esp_err_t register_stub(esp_event_base_t base, int32_t id, esp_event_handler_t handler,
                               void *arg, esp_event_handler_instance_t *instance, int cmock_num_calls)
{
    s_handler = handler;
    *instance = (esp_event_handler_instance_t)&s_handler;  // irgendein Wert != NULL
    return ESP_OK;
}

static esp_err_t set_config_stub(wifi_interface_t interface, wifi_config_t *conf, int cmock_num_calls)
{
    TEST_ASSERT_EQUAL(WIFI_IF_STA, interface);
    s_seen_config = *conf;
    return ESP_OK;
}

// Simuliert eine IP-Adresse vom Router
static void send_got_ip(void)
{
    ip_event_got_ip_t event = {0};
    event.ip_info.ip.addr = ESP_IP4TOADDR(192, 168, 1, 50);
    s_handler(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, &event);
}

static esp_err_t start_stub(int cmock_num_calls)
{
    if (s_send_ip_on_start) {
        send_got_ip();
    }
    return ESP_OK;
}

static net_config_t make_config(const char *password)
{
    net_config_t cfg = {.ssid = "Heim", .hostname = "lampe"};
    strcpy(cfg.password, password);
    return cfg;
}

// Erwartungen bis einschließlich esp_wifi_start
static void expect_start_sequence(void)
{
    esp_netif_create_default_wifi_sta_ExpectAndReturn(FAKE_NETIF);
    esp_netif_set_hostname_ExpectAndReturn(FAKE_NETIF, "lampe", ESP_OK);
    esp_wifi_init_ExpectAnyArgsAndReturn(ESP_OK);
    esp_event_handler_instance_register_Stub(register_stub);
    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_STA, ESP_OK);
    esp_wifi_set_config_Stub(set_config_stub);
    esp_wifi_start_Stub(start_stub);
}

// Erwartungen für den Abbau nach Fehlschlag
static void expect_cleanup(void)
{
    esp_event_handler_instance_unregister_ExpectAnyArgsAndReturn(ESP_OK);
    esp_event_handler_instance_unregister_ExpectAnyArgsAndReturn(ESP_OK);
    esp_wifi_stop_ExpectAndReturn(ESP_OK);
    esp_wifi_deinit_ExpectAndReturn(ESP_OK);
    esp_netif_destroy_default_wifi_Expect(FAKE_NETIF);
}

static void connect_ok(void)
{
    net_config_t cfg = make_config("geheim123");
    s_send_ip_on_start = true;
    expect_start_sequence();
    TEST_ASSERT_EQUAL(ESP_OK, wifi_sta_connect(&cfg, 1000));
}

static void test_connect_success(void)
{
    connect_ok();
    TEST_ASSERT_TRUE(wifi_sta_is_connected());
    char ip[16];
    wifi_sta_ip(ip, sizeof(ip));
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", ip);
    TEST_ASSERT_EQUAL_STRING("Heim", (const char *)s_seen_config.sta.ssid);
    TEST_ASSERT_EQUAL_STRING("geheim123", (const char *)s_seen_config.sta.password);
    TEST_ASSERT_EQUAL(WIFI_AUTH_WPA2_PSK, s_seen_config.sta.threshold.authmode);
}

static void test_connect_open_network(void)
{
    net_config_t cfg = make_config("");
    s_send_ip_on_start = true;
    expect_start_sequence();
    TEST_ASSERT_EQUAL(ESP_OK, wifi_sta_connect(&cfg, 1000));
    TEST_ASSERT_EQUAL(WIFI_AUTH_OPEN, s_seen_config.sta.threshold.authmode);
}

static void test_connect_timeout_cleans_up(void)
{
    net_config_t cfg = make_config("geheim123");
    s_send_ip_on_start = false;
    expect_start_sequence();
    expect_cleanup();
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, wifi_sta_connect(&cfg, 10));
    TEST_ASSERT_FALSE(wifi_sta_is_connected());
    char ip[16];
    wifi_sta_ip(ip, sizeof(ip));
    TEST_ASSERT_EQUAL_STRING("-", ip);
}

static void test_connect_rejects_bad_config(void)
{
    net_config_t cfg = make_config("geheim123");
    cfg.ssid[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_sta_connect(&cfg, 10));
    cfg = make_config("kurz");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_sta_connect(&cfg, 10));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_sta_connect(NULL, 10));
}

static void test_connect_init_error_cleans_up_netif(void)
{
    net_config_t cfg = make_config("geheim123");
    esp_netif_create_default_wifi_sta_ExpectAndReturn(FAKE_NETIF);
    esp_netif_set_hostname_ExpectAndReturn(FAKE_NETIF, "lampe", ESP_OK);
    esp_wifi_init_ExpectAnyArgsAndReturn(ESP_ERR_NO_MEM);
    esp_netif_destroy_default_wifi_Expect(FAKE_NETIF);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, wifi_sta_connect(&cfg, 10));
}

static void test_start_event_triggers_connect(void)
{
    connect_ok();
    esp_wifi_connect_ExpectAndReturn(ESP_OK);
    s_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_START, NULL);
}

static void test_disconnect_reconnects_and_remembers_time(void)
{
    connect_ok();
    esp_wifi_connect_ExpectAndReturn(ESP_OK);
    wifi_event_sta_disconnected_t event = {0};
    s_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event);
    TEST_ASSERT_FALSE(wifi_sta_is_connected());
    uint32_t now = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
    TEST_ASSERT_UINT32_WITHIN(100, now, wifi_sta_lost_at_ms());

    // Neue IP -> wieder verbunden
    send_got_ip();
    TEST_ASSERT_TRUE(wifi_sta_is_connected());
}

static void test_disconnect_with_reason_still_reconnects(void)
{
    connect_ok();
    // Zweimal derselbe Grund: wird nur einmal geloggt, verbindet aber jedes Mal neu
    esp_wifi_connect_ExpectAndReturn(ESP_OK);
    esp_wifi_connect_ExpectAndReturn(ESP_OK);
    wifi_event_sta_disconnected_t event = {.reason = 201};
    s_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event);
    s_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event);
    TEST_ASSERT_FALSE(wifi_sta_is_connected());
}

// Wird von test_main.c aufgerufen
void run_wifi_sta_tests(void)
{
    RUN_TEST(test_connect_success);
    RUN_TEST(test_connect_open_network);
    RUN_TEST(test_connect_timeout_cleans_up);
    RUN_TEST(test_connect_rejects_bad_config);
    RUN_TEST(test_connect_init_error_cleans_up_netif);
    RUN_TEST(test_start_event_triggers_connect);
    RUN_TEST(test_disconnect_reconnects_and_remembers_time);
    RUN_TEST(test_disconnect_with_reason_still_reconnects);
}
