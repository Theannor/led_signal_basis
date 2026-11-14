// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "web_server.h"
#include "web_form.h"
#include "Mockesp_http_server.h"
#include "Mocknvs.h"

// Tests für den LED-Strip-Bereich der Einstellungen (POST /led)

static int s_dummy_server;
#define FAKE_SERVER ((httpd_handle_t)&s_dummy_server)
#define FAKE_NVS    ((nvs_handle_t)7)
#define MASK        (LED_CONFIG_GPIO_BIT(4) | LED_CONFIG_GPIO_BIT(10) | LED_CONFIG_GPIO_BIT(21))

static esp_err_t (*s_get_root)(httpd_req_t *req);
static esp_err_t (*s_post_led)(httpd_req_t *req);

static const char *s_body;
static char s_sent[8192];
static size_t s_sent_len;
static char s_status[40];
static int s_restarts;

static esp_err_t start_stub(httpd_handle_t *handle, const httpd_config_t *config, int cmock_num_calls)
{
    *handle = FAKE_SERVER;
    TEST_ASSERT_GREATER_OR_EQUAL(9, config->max_uri_handlers);
    return ESP_OK;
}

static esp_err_t register_stub(httpd_handle_t handle, const httpd_uri_t *uri, int cmock_num_calls)
{
    if (strcmp(uri->uri, "/") == 0 && uri->method == HTTP_GET) {
        s_get_root = uri->handler;
    } else if (strcmp(uri->uri, "/led") == 0 && uri->method == HTTP_POST) {
        s_post_led = uri->handler;
    }
    return ESP_OK;
}

static int recv_stub(httpd_req_t *req, char *buf, size_t len, int cmock_num_calls)
{
    static size_t offset;
    if (cmock_num_calls == 0) {
        offset = 0;
    }
    size_t rest = strlen(s_body) - offset;
    size_t n = rest < len ? rest : len;
    memcpy(buf, s_body + offset, n);
    offset += n;
    return (int)n;
}

static esp_err_t send_stub(httpd_req_t *req, const char *buf, ssize_t len, int cmock_num_calls)
{
    TEST_ASSERT_LESS_THAN(sizeof(s_sent), (size_t)len);
    memcpy(s_sent, buf, (size_t)len);
    s_sent[len] = '\0';
    s_sent_len = (size_t)len;
    return ESP_OK;
}

static esp_err_t set_status_stub(httpd_req_t *req, const char *status, int cmock_num_calls)
{
    strlcpy(s_status, status, sizeof(s_status));
    return ESP_OK;
}

static void fake_restart(void)
{
    s_restarts++;
}

// Server mit allen Bereichen (Bluetooth, ESP-NOW, LED-Strip) starten
static void start_server(void)
{
    web_server_ctx_t ctx = {
        .client_mode = false,
        .port = 80,
        .config = {.ssid = "Heim", .hostname = "lampe"},
        .schedule_restart = fake_restart,
        .ble_name = "LED Signal",
        .own_mac = {0x24, 0x6f, 0x28, 0x01, 0x02, 0x03},
        .espnow_window_s = 60,
        .led = {.gpio = 10, .count = 16},
        .led_gpio_mask = MASK,
    };
    s_restarts = 0;
    s_sent[0] = s_status[0] = '\0';
    s_get_root = s_post_led = NULL;
    httpd_start_Stub(start_stub);
    httpd_register_uri_handler_Stub(register_stub);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_start(&ctx));
    TEST_ASSERT_NOT_NULL(s_get_root);
    TEST_ASSERT_NOT_NULL(s_post_led);

    httpd_resp_set_status_Stub(set_status_stub);
    httpd_resp_set_type_IgnoreAndReturn(ESP_OK);
    httpd_resp_send_Stub(send_stub);
}

static void stop_server(void)
{
    httpd_stop_ExpectAndReturn(FAKE_SERVER, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_stop());
}

static void post_led(const char *body)
{
    httpd_req_t req = {0};
    s_body = body;
    req.content_len = strlen(body);
    httpd_req_recv_Stub(recv_stub);
    TEST_ASSERT_EQUAL(ESP_OK, s_post_led(&req));
}

static void expect_save(uint8_t gpio, uint8_t count)
{
    static nvs_handle_t handle = FAKE_NVS;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
    nvs_set_u8_ExpectAndReturn(FAKE_NVS, "led_gpio", gpio, ESP_OK);
    nvs_set_u8_ExpectAndReturn(FAKE_NVS, "led_count", count, ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_NVS, ESP_OK);
    nvs_close_Expect(FAKE_NVS);
}

static void test_render_led_card_only_with_mask(void)
{
    char buf[4096];
    web_page_info_t info = {.hostname = "lampe", .led_gpio_mask = MASK, .led_gpio = 21, .led_count = 20};
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "action=\"/led\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<option value=\"4\">GPIO 4</option>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<option value=\"10\">GPIO 10</option>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<option value=\"21\" selected>GPIO 21</option>"));
    TEST_ASSERT_NULL(strstr(buf, "value=\"5\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "name=\"led_count\" min=\"16\" max=\"25\" value=\"20\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "</main></body></html>"));

    info.led_gpio_mask = 0;
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NULL(strstr(buf, "/led"));
}

static void test_full_page_fits(void)
{
    start_server();
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_get_root(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "action=\"/led\""));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "action=\"/espnow\""));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "action=\"/ble\""));
    stop_server();
}

static void test_post_led_saves_and_restarts(void)
{
    start_server();
    expect_save(21, 25);
    post_led("led_gpio=21&led_count=+25+");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<option value=\"21\" selected>"));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"25\""));
    stop_server();
}

static void test_post_led_rejects_invalid(void)
{
    start_server();
    post_led("led_gpio=5&led_count=20");    // GPIO nicht in der Liste
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_led("led_gpio=10&led_count=15");   // zu wenige
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_led("led_gpio=10&led_count=26");   // zu viele
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_led("led_gpio=10&led_count=abc");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_led("led_gpio=10&led_count=");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_led("led_gpio=266&led_count=20");  // würde in uint8_t zu 10
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_led("led_count=20");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    // Alte Werte bleiben auf der Seite
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<option value=\"10\" selected>"));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"16\""));
    stop_server();
}

static void test_post_led_reports_nvs_error(void)
{
    start_server();
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    post_led("led_gpio=4&led_count=20");
    TEST_ASSERT_EQUAL_STRING("500 Internal Server Error", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

// Wird von test_main.c aufgerufen
void run_web_led_tests(void)
{
    RUN_TEST(test_render_led_card_only_with_mask);
    RUN_TEST(test_full_page_fits);
    RUN_TEST(test_post_led_saves_and_restarts);
    RUN_TEST(test_post_led_rejects_invalid);
    RUN_TEST(test_post_led_reports_nvs_error);
}
