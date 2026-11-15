// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "web_server.h"
#include "web_form.h"
#include "Mockesp_http_server.h"
#include "Mocknvs.h"

// Tests für den Bluetooth-Bereich der Einstellungen (POST /ble) und
// web_server_set_palette

static int s_dummy_server;
#define FAKE_SERVER ((httpd_handle_t)&s_dummy_server)
#define FAKE_NVS    ((nvs_handle_t)5)

static esp_err_t (*s_get_root)(httpd_req_t *req);
static esp_err_t (*s_get_ble)(httpd_req_t *req);
static esp_err_t (*s_post_ble)(httpd_req_t *req);
static esp_err_t (*s_api_get)(httpd_req_t *req);

static const char *s_body;
static char s_sent[8192];
static size_t s_sent_len;
static char s_status[40];
static int s_restarts;

static esp_err_t start_stub(httpd_handle_t *handle, const httpd_config_t *config, int cmock_num_calls)
{
    *handle = FAKE_SERVER;
    // Alle Seiten müssen in die Standard-Anzahl an Handlern passen
    TEST_ASSERT_GREATER_OR_EQUAL(7, config->max_uri_handlers);
    return ESP_OK;
}

static esp_err_t register_stub(httpd_handle_t handle, const httpd_uri_t *uri, int cmock_num_calls)
{
    if (strcmp(uri->uri, "/") == 0 && uri->method == HTTP_GET) {
        s_get_root = uri->handler;
    } else if (strcmp(uri->uri, "/ble") == 0 && uri->method == HTTP_GET) {
        s_get_ble = uri->handler;
    } else if (strcmp(uri->uri, "/ble") == 0 && uri->method == HTTP_POST) {
        s_post_ble = uri->handler;
    } else if (strcmp(uri->uri, "/api/colors") == 0 && uri->method == HTTP_GET) {
        s_api_get = uri->handler;
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

static void start_server(const char *ble_name)
{
    web_server_ctx_t ctx = {
        .client_mode = true,
        .port = 80,
        .config = {.ssid = "Heim", .hostname = "lampe"},
        .schedule_restart = fake_restart,
        .palette = {.colors = {{.r = 255}}, .count = 1},
    };
    strlcpy(ctx.ble_name, ble_name, sizeof(ctx.ble_name));
    s_restarts = 0;
    s_sent[0] = s_status[0] = '\0';
    s_get_root = s_get_ble = s_post_ble = s_api_get = NULL;
    httpd_start_Stub(start_stub);
    httpd_register_uri_handler_Stub(register_stub);
    // Keine Cookie- und Accept-Language-Header -> deutsche Seiten
    httpd_req_get_hdr_value_str_IgnoreAndReturn(ESP_ERR_NOT_FOUND);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_start(&ctx));
    TEST_ASSERT_NOT_NULL(s_get_root);
    TEST_ASSERT_NOT_NULL(s_get_ble);
    TEST_ASSERT_NOT_NULL(s_post_ble);
    TEST_ASSERT_NOT_NULL(s_api_get);

    httpd_resp_set_status_Stub(set_status_stub);
    httpd_resp_set_type_IgnoreAndReturn(ESP_OK);
    httpd_resp_send_Stub(send_stub);
}

static void stop_server(void)
{
    httpd_stop_ExpectAndReturn(FAKE_SERVER, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_stop());
}

static void post_ble(const char *body)
{
    httpd_req_t req = {0};
    s_body = body;
    req.content_len = strlen(body);
    httpd_req_recv_Stub(recv_stub);
    TEST_ASSERT_EQUAL(ESP_OK, s_post_ble(&req));
}

static void expect_save_name(const char *name)
{
    static nvs_handle_t handle = FAKE_NVS;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
    nvs_set_str_ExpectAndReturn(FAKE_NVS, "ble_name", name, ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_NVS, ESP_OK);
    nvs_close_Expect(FAKE_NVS);
}

static void test_render_shows_ble_card_only_with_name(void)
{
    char buf[4096];
    web_page_info_t info = {.client_mode = true, .hostname = "lampe", .ble_name = "Mein <LED>",
                            .settings_page = WEB_SETTINGS_BLE};
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "action=\"/ble\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/ble\" class=\"cur\">Bluetooth</a>"));
    // Nur die Bluetooth-Karte
    TEST_ASSERT_NULL(strstr(buf, "action=\"/config\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "value=\"Mein &lt;LED&gt;\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "</main></body></html>"));

    // Ohne Namen weder Unterseite noch Menüeintrag: WLAN wird gezeigt
    info.ble_name = NULL;
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NULL(strstr(buf, "/ble"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "action=\"/config\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "</main></body></html>"));
}

static void test_page_with_longest_values_fits(void)
{
    // Längste Eingaben, die überall maskiert werden müssen
    start_server("\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"");
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_get_ble(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "action=\"/ble\""));
    // Andere Tests empfangen die Seite in 4096-Byte-Puffern
    TEST_ASSERT_LESS_THAN(4096, s_sent_len);
    stop_server();
}

static void test_post_ble_saves_and_restarts(void)
{
    start_server("LED Signal");
    expect_save_name("Wohnzimmer 2");
    post_ble("ble_name=+Wohnzimmer+2+");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"Wohnzimmer 2\""));
    stop_server();
}

static void test_post_ble_rejects_invalid_name(void)
{
    start_server("LED Signal");
    post_ble("ble_name=");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_ble("ble_name=K%C3%BCche");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_ble("ble_name=123456789012345678901");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    post_ble("anderes=1");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    // Alter Name bleibt auf der Seite
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"LED Signal\""));
    stop_server();
}

static void test_post_ble_reports_nvs_error(void)
{
    start_server("LED Signal");
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    post_ble("ble_name=Neu");
    TEST_ASSERT_EQUAL_STRING("500 Internal Server Error", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

static void test_set_palette_shows_in_api(void)
{
    start_server("LED Signal");
    led_palette_t p = {.count = 2, .colors = {{.g = 255}, {.b = 255}}};
    TEST_ASSERT_EQUAL(ESP_OK, web_server_set_palette(&p));
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_api_get(&req));
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[\"#00ff00\",\"#0000ff\"]}", s_sent);

    p.count = LED_PALETTE_MAX + 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, web_server_set_palette(&p));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, web_server_set_palette(NULL));
    stop_server();
    p.count = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, web_server_set_palette(&p));
}

// Wird von test_main.c aufgerufen
void run_web_ble_tests(void)
{
    RUN_TEST(test_render_shows_ble_card_only_with_name);
    RUN_TEST(test_page_with_longest_values_fits);
    RUN_TEST(test_post_ble_saves_and_restarts);
    RUN_TEST(test_post_ble_rejects_invalid_name);
    RUN_TEST(test_post_ble_reports_nvs_error);
    RUN_TEST(test_set_palette_shows_in_api);
}
